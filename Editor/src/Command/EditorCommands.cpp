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
	CreateEmptyEntityCommand::CreateEmptyEntityCommand() : m_entity(0) {}

	void CreateEmptyEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateEntity();
		EditorScene::s_rootOrder.push_back(m_entity);
		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateEmptyEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateCanvasEntityCommand::CreateCanvasEntityCommand() : m_entity(0) {}

	void CreateCanvasEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateCanvasEntity();
		EditorScene::s_rootOrder.push_back(m_entity);
		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateCanvasEntityCommand::Undo() {
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

	DeleteEntityCommand::DeleteEntityCommand(std::vector<uint32_t> deletedEntity) : m_entities(deletedEntity) {}

	void DeleteEntityCommand::Execute() {
		for (auto& e : m_entities) {
			NE::ECS::Command::DestroyEntity(e);
			EditorScene::UnregisterRoot(e);
		}

		EditorScene::s_selection.Clear();
	}

	void DeleteEntityCommand::Undo() {
		return;
		for (auto& e : m_entities) {
			NE::ECS::Command::CreateEntity();
			EditorScene::RegisterRoot(e);
		}
	}

	CreateCubeEntityCommand::CreateCubeEntityCommand() : m_entity(0) {}

	void CreateCubeEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateEntity();
		NE::ECS::Command::AddRendererComponent(m_entity);
		NE::Renderer::Command::AssignModel(m_entity, "builtin:model/cube");
		EditorScene::s_rootOrder.push_back(m_entity);
		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateCubeEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateSphereEntityCommand::CreateSphereEntityCommand() : m_entity(0) {}

	void CreateSphereEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateEntity();
		NE::ECS::Command::AddRendererComponent(m_entity);
		NE::Renderer::Command::AssignModel(m_entity, "builtin:model/sphere");
		EditorScene::s_rootOrder.push_back(m_entity);
		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateSphereEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateCylinderEntityCommand::CreateCylinderEntityCommand() : m_entity(0) {}

	void CreateCylinderEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateEntity();
		NE::ECS::Command::AddRendererComponent(m_entity);
		NE::Renderer::Command::AssignModel(m_entity, "builtin:model/cylinder");
		EditorScene::s_rootOrder.push_back(m_entity);
		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateCylinderEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateCapsuleEntityCommand::CreateCapsuleEntityCommand() : m_entity(0) {}

	void CreateCapsuleEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateEntity();
		NE::ECS::Command::AddRendererComponent(m_entity);
		NE::Renderer::Command::AssignModel(m_entity, "builtin:model/capsule");
		EditorScene::s_rootOrder.push_back(m_entity);
		EditorScene::s_selection.SetSingle(m_entity);
	}

	void CreateCapsuleEntityCommand::Undo() {
		EditorScene::UnregisterRoot(m_entity);
		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreatePlaneEntityCommand::CreatePlaneEntityCommand() : m_entity(0) {}

	void CreatePlaneEntityCommand::Execute() {
		m_entity = NE::ECS::Command::CreateEntity();
		NE::ECS::Command::AddRendererComponent(m_entity);
		NE::Renderer::Command::AssignModel(m_entity, "builtin:model/plane");
		EditorScene::s_rootOrder.push_back(m_entity);
		EditorScene::s_selection.SetSingle(m_entity);
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

}