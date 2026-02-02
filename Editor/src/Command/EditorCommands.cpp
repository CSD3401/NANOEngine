#include "EditorCommands.hpp"

// Include component headers before ECSExports (which has forward declarations)
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Hierarchy.hpp>
#include <ECS/Components/UIRectTransform.hpp>
#include <ECS/Components/UICanvas.hpp>
#include <ECS/Components/UIImage.hpp>
#include <ECS/Components/UIText.hpp>
#include <ECS/Components/UIButton.hpp>

#include "EditorInterface/ECSExports.hpp"
#include <EditorInterface/RendererExports.hpp>
#include "../EditorScene.hpp"
#include <ECS/Core/Entity.hpp>
#include <SceneManagement/SceneManager.hpp>
#include <algorithm>
#include <Core/LUIDGenerator.hpp>

namespace Editor {
	CreateEmptyEntityCommand::CreateEmptyEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateEmptyEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateEmptyEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateEmptyEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	// Helper function to find existing canvas or create one
	uint32_t FindOrCreateCanvas() {
		// Search for existing canvas entity
		auto& ecs = NE::GetScene().GetECSCoordinator();
		const auto& entities = ecs.GetEntityManager().GetUsedEntities();

		for (auto e : entities) {
			if (NE::ECS::Query::HasUICanvas(e)) {
				return e;
			}
		}

		// No canvas found, create one
		uint32_t canvasEntity = NE::ECS::Command::CreateUICanvasEntity();
		EditorScene::s_rootOrder.push_back(canvasEntity);
		return canvasEntity;
	}

	// ==================== CreateUICanvasCommand ====================
	CreateUICanvasCommand::CreateUICanvasCommand() {}

	void CreateUICanvasCommand::Execute() {
		m_entity = NE::ECS::Command::CreateUICanvasEntity();
		EditorScene::s_rootOrder.push_back(m_entity);
		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUICanvasCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	// ==================== CreateUITextCommand ====================
	CreateUITextCommand::CreateUITextCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUITextCommand::Execute() {
		// Find or create canvas
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		// Check if parent is valid and under a canvas
		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			// Walk up to find the canvas root
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				auto& rect = NE::ECS::Query::GetUIRectTransform(canvasEntity);
				canvasEntity = rect.parent;
			}
		}

		// If no canvas found in hierarchy, find or create one
		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		// Determine parent for the text entity
		uint32_t parentForText = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the text entity
		m_entity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_entity,
			NE::ECS::Component::EntityMeta{ .name = "Text", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_entity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform
		NE::ECS::Component::UIRectTransform rect{};
		rect.luid = NE::Core::LUIDGenerator::Generate("rt");
		rect.width = 160.0f;
		rect.height = 30.0f;
		rect.x = 0.0f;
		rect.y = 0.0f;
		rect.parent = parentForText;
		if (parentForText != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(parentForText)) {
			rect.parentLuid = NE::ECS::Query::GetUIRectTransform(parentForText).luid;
		}
		NE::ECS::Command::AddUIRectTransformComponent(m_entity, rect);

		// Setup UIText
		NE::ECS::Component::UIText text{};
		text.text = "New Text";
		text.fontSize = 24.0f;
		text.color = NE::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		text.horizontalAlign = NE::ECS::Component::UIText::Alignment::CENTER;
		text.verticalAlign = NE::ECS::Component::UIText::VerticalAlignment::MIDDLE;
		NE::ECS::Command::AddUITextComponent(m_entity, text);

		// Set hierarchy parent
		NE::ECS::Command::SetParent(m_entity, parentForText, -1, false);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUITextCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	// ==================== CreateUIImageCommand ====================
	CreateUIImageCommand::CreateUIImageCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUIImageCommand::Execute() {
		// Find or create canvas (same logic as text)
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				auto& rect = NE::ECS::Query::GetUIRectTransform(canvasEntity);
				canvasEntity = rect.parent;
			}
		}

		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		uint32_t parentForImage = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the image entity
		m_entity = NE::ECS::Command::CreateUIImageEntity(parentForImage);

		// Set hierarchy parent
		NE::ECS::Command::SetParent(m_entity, parentForImage, -1, false);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUIImageCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	// ==================== CreateUIButtonCommand ====================
	CreateUIButtonCommand::CreateUIButtonCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUIButtonCommand::Execute() {
		// Find or create canvas
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				auto& rect = NE::ECS::Query::GetUIRectTransform(canvasEntity);
				canvasEntity = rect.parent;
			}
		}

		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		uint32_t parentForButton = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the button entity
		m_entity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_entity,
			NE::ECS::Component::EntityMeta{ .name = "Button", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_entity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for button
		NE::ECS::Component::UIRectTransform buttonRect{};
		buttonRect.luid = NE::Core::LUIDGenerator::Generate("rt");
		buttonRect.width = 160.0f;
		buttonRect.height = 30.0f;
		buttonRect.x = 0.0f;
		buttonRect.y = 0.0f;
		buttonRect.parent = parentForButton;
		if (parentForButton != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(parentForButton)) {
			buttonRect.parentLuid = NE::ECS::Query::GetUIRectTransform(parentForButton).luid;
		}
		NE::ECS::Command::AddUIRectTransformComponent(m_entity, buttonRect);

		// Setup UIImage for button background
		NE::ECS::Component::UIImage img{};
		img.color = NE::Math::Vec4(0.8f, 0.8f, 0.8f, 1.0f);
		NE::ECS::Command::AddUIImageComponent(m_entity, img);

		// Setup UIButton
		NE::ECS::Component::UIButton button{};
		button.normalColor = NE::Math::Vec4(0.8f, 0.8f, 0.8f, 1.0f);
		button.hoverColor = NE::Math::Vec4(0.9f, 0.9f, 0.9f, 1.0f);
		button.pressedColor = NE::Math::Vec4(0.6f, 0.6f, 0.6f, 1.0f);
		button.disabledColor = NE::Math::Vec4(0.5f, 0.5f, 0.5f, 0.5f);
		NE::ECS::Command::AddUIButtonComponent(m_entity, button);

		// Set hierarchy parent for button
		NE::ECS::Command::SetParent(m_entity, parentForButton, -1, false);

		// Create child Text entity
		m_textEntity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_textEntity,
			NE::ECS::Component::EntityMeta{ .name = "Text", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_textEntity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform for text (stretch to fill button)
		NE::ECS::Component::UIRectTransform textRect{};
		textRect.luid = NE::Core::LUIDGenerator::Generate("rt");
		textRect.anchorMinX = 0.0f; textRect.anchorMinY = 0.0f;
		textRect.anchorMaxX = 1.0f; textRect.anchorMaxY = 1.0f;
		textRect.offsetMinX = 0.0f; textRect.offsetMinY = 0.0f;
		textRect.offsetMaxX = 0.0f; textRect.offsetMaxY = 0.0f;
		textRect.width = 160.0f;
		textRect.height = 30.0f;
		textRect.parent = m_entity;
		textRect.parentLuid = buttonRect.luid;
		NE::ECS::Command::AddUIRectTransformComponent(m_textEntity, textRect);

		// Setup UIText for button label
		NE::ECS::Component::UIText text{};
		text.text = "Button";
		text.fontSize = 18.0f;
		text.color = NE::Math::Vec4(0.2f, 0.2f, 0.2f, 1.0f);
		text.horizontalAlign = NE::ECS::Component::UIText::Alignment::CENTER;
		text.verticalAlign = NE::ECS::Component::UIText::VerticalAlignment::MIDDLE;
		NE::ECS::Command::AddUITextComponent(m_textEntity, text);

		// Set hierarchy parent for text
		NE::ECS::Command::SetParent(m_textEntity, m_entity, -1, false);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUIButtonCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_textEntity);
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	// ==================== CreateUIPanelCommand ====================
	CreateUIPanelCommand::CreateUIPanelCommand(uint32_t parentEntity)
		: m_parentEntity(parentEntity) {}

	void CreateUIPanelCommand::Execute() {
		// Find or create canvas
		uint32_t canvasEntity = NE::ECS::NO_ENTITY;

		if (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity)) {
			canvasEntity = m_parentEntity;
			while (canvasEntity != NE::ECS::NO_ENTITY) {
				if (NE::ECS::Query::HasUICanvas(canvasEntity)) break;
				auto& rect = NE::ECS::Query::GetUIRectTransform(canvasEntity);
				canvasEntity = rect.parent;
			}
		}

		if (canvasEntity == NE::ECS::NO_ENTITY || !NE::ECS::Query::HasUICanvas(canvasEntity)) {
			m_canvasEntity = FindOrCreateCanvas();
			m_createdCanvas = (m_canvasEntity != NE::ECS::NO_ENTITY);
			canvasEntity = m_canvasEntity;
		}

		uint32_t parentForPanel = (m_parentEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(m_parentEntity))
			? m_parentEntity : canvasEntity;

		// Create the panel entity
		m_entity = NE::ECS::Command::CreateEntityNoComponents();

		NE::ECS::Command::AddEntityMetaComponent(m_entity,
			NE::ECS::Component::EntityMeta{ .name = "Panel", .luid = NE::Core::LUIDGenerator::Generate("em") });

		NE::ECS::Command::AddHierarchyComponent(m_entity, NE::ECS::Component::Hierarchy{});

		// Setup UIRectTransform with some default stretch
		NE::ECS::Component::UIRectTransform rect{};
		rect.luid = NE::Core::LUIDGenerator::Generate("rt");
		rect.width = 200.0f;
		rect.height = 200.0f;
		rect.x = 0.0f;
		rect.y = 0.0f;
		rect.parent = parentForPanel;
		if (parentForPanel != NE::ECS::NO_ENTITY && NE::ECS::Query::HasUIRectTransform(parentForPanel)) {
			rect.parentLuid = NE::ECS::Query::GetUIRectTransform(parentForPanel).luid;
		}
		NE::ECS::Command::AddUIRectTransformComponent(m_entity, rect);

		// Setup UIImage with semi-transparent white
		NE::ECS::Component::UIImage img{};
		img.color = NE::Math::Vec4(1.0f, 1.0f, 1.0f, 0.4f);
		NE::ECS::Command::AddUIImageComponent(m_entity, img);

		// Set hierarchy parent
		NE::ECS::Command::SetParent(m_entity, parentForPanel, -1, false);

		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateUIPanelCommand::Undo() {
		NE::ECS::Command::DestroyEntity(m_entity);
		if (m_createdCanvas && m_canvasEntity != NE::ECS::NO_ENTITY) {
			EditorScene::UnregisterRoot(m_canvasEntity);
			NE::ECS::Command::DestroyEntity(m_canvasEntity);
		}
	}

	DeleteEntityCommand::DeleteEntityCommand(std::vector<uint32_t> deletedEntity, uint32_t oldParent) 
		: m_entities(deletedEntity), oldParentEntity(oldParent) {}

	void DeleteEntityCommand::Execute() {
		for (auto& e : m_entities) {
			m_data = NE::CopyEntity(e);
			NE::ECS::Command::DestroyEntity(e);
			EditorScene::UnregisterRoot(e);
		}

		EditorScene::s_selection.Clear();
		//const uint32_t rootId = m_entity;

		//std::vector<uint32_t> toDelete;
		//EditorScene::GetAllDescendants(rootId, toDelete);

		//// store information about deleted UI entities before destroying them
		//m_deletedEntities.clear();
		//for (uint32_t id : toDelete) {
		//	DeletedUIEntityInfo info;
		//	info.id = id;
		//	info.wasCanvas = NE::ECS::Query::HasUICanvas(id);
		//	info.wasUIImage = NE::ECS::Query::HasUIImage(id);
		//	info.parentId = NE::ECS::Query::GetParent(id);
		//	m_deletedEntities.push_back(info);
		//}

		//for (uint32_t id : toDelete) {
		//	{
		//		auto it = std::find_if(
		//			EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
		//			[id](const EditorEntity& e) { return e.linkedEntity == id; }
		//		);
		//		if (it != EditorScene::s_entities.end()) {
		//			EditorScene::s_entities.erase(it);
		//		}
		//	}

		//	EditorScene::s_nodes.erase(id);

		//	auto& roots = EditorScene::s_roots;
		//	roots.erase(std::remove(roots.begin(), roots.end(), id), roots.end());

		//	for (auto& [parent, vec] : EditorScene::s_children) {
		//		vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
		//	}

		//	NE::ECS::Command::DestroyEntity(id);
		//}

		//if (EditorScene::s_selectedEntity &&
		//	std::find(toDelete.begin(), toDelete.end(),
		//		EditorScene::s_selectedEntity->linkedEntity) != toDelete.end()) {
		//	EditorScene::s_selectedEntity = nullptr;
		//}
	}

	void DeleteEntityCommand::Undo() {
		for (auto& e : m_entities) {
			auto newEntt = NE::PasteEntity(m_data);
			EditorScene::SetParent(newEntt, oldParentEntity, -1, true);

			if (oldParentEntity == NE::ECS::NO_ENTITY)
				EditorScene::RegisterRoot(newEntt);
		}
		//// sort entities so parents are recreated before children
		//// (entities with no parent first, then their children, etc.)
		//std::sort(m_deletedEntities.begin(), m_deletedEntities.end(),
		//	[](const DeletedUIEntityInfo& a, const DeletedUIEntityInfo& b) {
		//		// Canvases (no parent) should come first
		//		if (a.parentId == NE::ECS::NO_ENTITY && b.parentId != NE::ECS::NO_ENTITY) return true;
		//		if (a.parentId != NE::ECS::NO_ENTITY && b.parentId == NE::ECS::NO_ENTITY) return false;
		//		return false;
		//	});

		//std::unordered_map<uint32_t, uint32_t> oldToNewId; // map old id to new id

		//for (const auto& info : m_deletedEntities)
		//{
		//	uint32_t newEntity;
		//	uint32_t newParentId = NE::ECS::NO_ENTITY;
		//	if (info.parentId != NE::ECS::NO_ENTITY)
		//	{
		//		// If the parent was also deleted & recreated, remap to the NEW id
		//		auto it = oldToNewId.find(info.parentId);
		//		if (it != oldToNewId.end())
		//		{
		//			newParentId = it->second; // parent was recreated
		//		}
		//		else
		//		{
		//			newParentId = info.parentId; // parent still exists, keep original id
		//		}
		//	}

		//	// recreate the correct type of entity
		//	if (info.wasCanvas)
		//	{
		//		newEntity = NE::ECS::Command::CreateUICanvasEntity();
		//	}
		//	else if (info.wasUIImage)
		//	{
		//		newEntity = NE::ECS::Command::CreateUIImageEntity(newParentId);
		//	}
		//	else
		//	{
		//		// just regular 3D entity
		//		newEntity = NE::ECS::Command::CreateEntity();
		//	}

		//	oldToNewId[info.id] = newEntity;

		//	// add to editor scene
		//	EditorScene::s_entities.push_back(EditorEntity{ newEntity });

		//	// setup editor hierarchy
		//	Editor::Node node{};
		//	node.id = newEntity;

		//	if (newParentId != NE::ECS::NO_ENTITY)
		//	{
		//		// has a parent
		//		node.parent = newParentId;
		//		auto& children = EditorScene::s_children[newParentId];
		//		node.orderKey = static_cast<float>(children.size());
		//		children.push_back(newEntity);
		//	}
		//	else
		//	{
		//		// root entity
		//		node.parent = NE::ECS::NO_ENTITY;
		//		node.orderKey = static_cast<float>(EditorScene::s_roots.size());
		//		EditorScene::s_roots.push_back(newEntity);
		//	}

		//	EditorScene::s_nodes[newEntity] = node;
		//}

		//// update m_entity to the new root id (the canvas in this case)
		//if (!m_deletedEntities.empty())
		//{
		//	// find the root entity (the one with no parent)
		//	for (const auto& info : m_deletedEntities)
		//	{
		//		if (info.parentId == NE::ECS::NO_ENTITY)
		//		{
		//			m_entity = oldToNewId[info.id];
		//			break;
		//		}
		//	}

		//	// select the recreated root entity
		//	auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
		//		[id = m_entity](const EditorEntity& e) { return e.linkedEntity == id; });
		//	if (it != EditorScene::s_entities.end()) {
		//		Editor::EditorScene::s_selectedEntity = &(*it);
		//	}
		//}
	}

	CreateCubeEntityCommand::CreateCubeEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateCubeEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateCubeEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateCubeEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateSphereEntityCommand::CreateSphereEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateSphereEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateSphereEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateSphereEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateCylinderEntityCommand::CreateCylinderEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateCylinderEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateCylinderEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateCylinderEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateCapsuleEntityCommand::CreateCapsuleEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreateCapsuleEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateCapsuleEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateCapsuleEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreatePlaneEntityCommand::CreatePlaneEntityCommand(uint32_t parentEntity) 
		: m_entity(0), m_parentEntity(parentEntity) {}

	void CreatePlaneEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreatePlaneEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreatePlaneEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	SetEntityLayerCommand::SetEntityLayerCommand(
		uint32_t entity, uint8_t before, uint8_t after)
		: m_entity(entity), m_before(before), m_after(after) {
	}

	void SetEntityLayerCommand::Execute() {
		NE::ECS::Command::SetLayer(m_entity, static_cast<NE::Core::LayerID>(m_after));
	}

	void SetEntityLayerCommand::Undo() {
		NE::ECS::Command::SetLayer(m_entity, static_cast<NE::Core::LayerID>(m_before));
	}

	CreateDirectionalLightCommand::CreateDirectionalLightCommand(uint32_t parentEntity)
		: m_entity(0), m_parentEntity(parentEntity) {
	}

	void CreateDirectionalLightCommand::Execute() {
		m_entity = NE::ECS::Command::CreateDirectionalLightEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateDirectionalLightCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreatePointLightCommand::CreatePointLightCommand(uint32_t parentEntity)
		: m_entity(0), m_parentEntity(parentEntity) {
	}

	void CreatePointLightCommand::Execute() {
		m_entity = NE::ECS::Command::CreatePointLightEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreatePointLightCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateSpotLightCommand::CreateSpotLightCommand(uint32_t parentEntity)
		: m_entity(0), m_parentEntity(parentEntity) {
	}

	void CreateSpotLightCommand::Execute() {
		m_entity = NE::ECS::Command::CreateSpotLightEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}


	void CreateSpotLightCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateQuadEntityCommand::CreateQuadEntityCommand(uint32_t parentEntity)
		: m_entity(0), m_parentEntity(parentEntity) {
	}

	void CreateQuadEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateQuadEntity(m_parentEntity);
		if (m_parentEntity == NE::ECS::NO_ENTITY)
			EditorScene::s_rootOrder.push_back(m_entity);
	}

	void CreateQuadEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	HierarchyChangeCommand::HierarchyChangeCommand(uint32_t child, uint32_t newParent, int newInsertIndex) 
		: childEntity(child), newParentEntity(newParent), newInsertIndex(newInsertIndex) 
	{
		oldParentEntity = EditorScene::GetParent(childEntity);
		oldInsertIndex = EditorScene::GetIndexInParentOrRoot(childEntity);
	}

	void HierarchyChangeCommand::Execute() {
		EditorScene::SetParent(childEntity, newParentEntity, newInsertIndex, true);
	}

	void HierarchyChangeCommand::Undo() {
		EditorScene::SetParent(childEntity, oldParentEntity, oldInsertIndex, true);
	}

}