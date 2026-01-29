#include "EditorCommands.hpp"
#include "EditorInterface/ECSExports.hpp"
#include <EditorInterface/RendererExports.hpp>
#include "../EditorScene.hpp"
#include <ECS/Core/Entity.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Hierarchy.hpp>
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

	CreateUICanvasEntityCommand::CreateUICanvasEntityCommand() : m_entity(0) {}

	void CreateUICanvasEntityCommand::Execute()
	{
		//m_entity = NE::ECS::Command::CreateUICanvasEntity();
		//EditorScene::s_entities.push_back(EditorEntity{ m_entity });
		//Editor::EditorScene::s_selectedEntity = &EditorScene::s_entities.back();

		//// setup as root node in hierarchy
		//Editor::Node node{};
		//node.id = m_entity;
		//node.parent = NE::ECS::NO_ENTITY;  // canvas has no parent
		//node.orderKey = static_cast<float>(EditorScene::s_roots.size());
		//EditorScene::s_nodes[m_entity] = node;
		//EditorScene::s_roots.push_back(m_entity);
	}

	void CreateUICanvasEntityCommand::Undo()
	{
		//const uint32_t id = m_entity;

		//// remove from editor entities list
		//auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
		//	[id](const EditorEntity& entt) {
		//		return entt.linkedEntity == id;
		//	});

		//if (it != EditorScene::s_entities.end())
		//{
		//	EditorScene::s_entities.erase(it);
		//}

		//// remove from scene graph
		//EditorScene::s_nodes.erase(id);

		//// remove from roots
		//auto& roots = EditorScene::s_roots;
		//roots.erase(std::remove(roots.begin(), roots.end(), id), roots.end());

		//// remove from any parent's children list
		//for (auto& [parent, vec] : EditorScene::s_children)
		//{
		//	vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
		//}

		//// clear selection if needed
		//if (Editor::EditorScene::s_selectedEntity &&
		//	Editor::EditorScene::s_selectedEntity->linkedEntity == id) {
		//	Editor::EditorScene::s_selectedEntity = nullptr;
		//}

		//NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateUIImageEntityCommand::CreateUIImageEntityCommand(uint32_t parentCanvas)
		: m_entity(0), m_parentCanvas(parentCanvas) {
	}

	void CreateUIImageEntityCommand::Execute()
	{
		//m_entity = NE::ECS::Command::CreateUIImageEntity(m_parentCanvas);
		//EditorScene::s_entities.push_back(EditorEntity{ m_entity });
		//Editor::EditorScene::s_selectedEntity = &EditorScene::s_entities.back();

		//// setup as child node in hierarchy
		//Editor::Node node{};
		//node.id = m_entity;
		//node.parent = m_parentCanvas;  // image belongs to canvas

		//// order key based on number of existing children
		//auto& children = EditorScene::s_children[m_parentCanvas];
		//node.orderKey = static_cast<float>(children.size());

		//EditorScene::s_nodes[m_entity] = node;
		//children.push_back(m_entity);  // add to parent's children list
	}

	void CreateUIImageEntityCommand::Undo()
	{
		//const uint32_t id = m_entity;

		//// remove from editor entities list
		//auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
		//	[id](const EditorEntity& entt) {
		//		return entt.linkedEntity == id;
		//	});

		//if (it != EditorScene::s_entities.end())
		//{
		//	EditorScene::s_entities.erase(it);
		//}

		//// remove from scene graph
		//EditorScene::s_nodes.erase(id);

		//// remove from roots
		//auto& roots = EditorScene::s_roots;
		//roots.erase(std::remove(roots.begin(), roots.end(), id), roots.end());

		//// remove from all parent's children lists
		//for (auto& [parent, vec] : EditorScene::s_children)
		//{
		//	vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
		//}

		//// clear selection if needed
		//if (Editor::EditorScene::s_selectedEntity &&
		//	Editor::EditorScene::s_selectedEntity->linkedEntity == id) {
		//	Editor::EditorScene::s_selectedEntity = nullptr;
		//}

		//NE::ECS::Command::DestroyEntity(m_entity);
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