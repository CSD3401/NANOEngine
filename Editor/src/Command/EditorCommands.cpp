#include "EditorCommands.hpp"
#include "ECSInternals.hpp"
#include "EditorInterface/ECSExports.hpp"
#include "../EditorScene.hpp"
#include <ECS/Core/Entity.hpp>

namespace Editor {

	CreateEntityCommand::CreateEntityCommand() : m_entity(0) {}

	void CreateEntityCommand::Execute()
	{
		m_entity = NE::ECS::Command::CreateEntity();
		EditorScene::s_entities.push_back(EditorEntity{ m_entity });

		Editor::EditorScene::s_selectedEntity = &EditorScene::s_entities.back();

		Editor::Node node{};
		node.id = m_entity;
		node.parent = NE::ECS::NO_ENTITY;
		node.orderKey = static_cast<float>(EditorScene::s_roots.size());

		EditorScene::s_nodes[m_entity] = node;
		EditorScene::s_roots.push_back(m_entity);

		Editor::EditorScene::s_selectedEntity = &EditorScene::s_entities.back();
	}

	void CreateEntityCommand::Undo()
	{
		const uint32_t id = m_entity;

		// temp
		auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
			[id = m_entity](const EditorEntity& entt) {
				return entt.linkedEntity == id;
			});

		if (it != EditorScene::s_entities.end()) {
			EditorScene::s_entities.erase(it);
		}

		EditorScene::s_nodes.erase(id);

		auto& roots = EditorScene::s_roots;
		roots.erase(std::remove(roots.begin(), roots.end(), id), roots.end());

		for (auto& [parent, vec] : EditorScene::s_children) {
			vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
		}

		if (Editor::EditorScene::s_selectedEntity &&
			Editor::EditorScene::s_selectedEntity->linkedEntity == id) {
			Editor::EditorScene::s_selectedEntity = nullptr;
		}

		NE::ECS::Command::DestroyEntity(m_entity);
	}

	//CreateUIEntityCommand::CreateUIEntityCommand() : m_entity(0) {}

	//void CreateUIEntityCommand::Execute()
	//{
	//	m_entity = NE::ECS::Command::CreateUIEntity();
	//	EditorScene::s_entities.push_back(EditorEntity{ m_entity });
	//	//Editor::EditorScene::BuildFlatHierarchy();
	//	Editor::EditorScene::s_selectedEntity = &EditorScene::s_entities.back();
	//}

	//void CreateUIEntityCommand::Undo()
	//{
	//	// temp
	//	auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
	//		[id = m_entity](const EditorEntity& entt) {
	//			return entt.linkedEntity == id;
	//		});

	//	if (it != EditorScene::s_entities.end()) {
	//		EditorScene::s_entities.erase(it);
	//	}

	//	Editor::EditorScene::s_selectedEntity = nullptr;
	//	NE::ECS::Command::DestroyEntity(m_entity);
	//	//Editor::EditorScene::BuildFlatHierarchy();
	//}

	CreateUICanvasEntityCommand::CreateUICanvasEntityCommand() : m_entity(0) {}

	void CreateUICanvasEntityCommand::Execute()
	{
		m_entity = NE::ECS::Command::CreateUICanvasEntity();
		EditorScene::s_entities.push_back(EditorEntity{ m_entity });
		Editor::EditorScene::s_selectedEntity = &EditorScene::s_entities.back();

		// setup as root node in hierarchy
		Editor::Node node{};
		node.id = m_entity;
		node.parent = NE::ECS::NO_ENTITY;  // canvas has no parent
		node.orderKey = static_cast<float>(EditorScene::s_roots.size());
		EditorScene::s_nodes[m_entity] = node;
		EditorScene::s_roots.push_back(m_entity);
	}

	void CreateUICanvasEntityCommand::Undo()
	{
		const uint32_t id = m_entity;

		// remove from editor entities list
		auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
			[id](const EditorEntity& entt) {
				return entt.linkedEntity == id;
			});

		if (it != EditorScene::s_entities.end()) 
		{
			EditorScene::s_entities.erase(it);
		}

		// remove from scene graph
		EditorScene::s_nodes.erase(id);

		// remove from roots
		auto& roots = EditorScene::s_roots;
		roots.erase(std::remove(roots.begin(), roots.end(), id), roots.end());

		// remove from any parent's children list
		for (auto& [parent, vec] : EditorScene::s_children) 
		{
			vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
		}

		// clear selection if needed
		if (Editor::EditorScene::s_selectedEntity &&
			Editor::EditorScene::s_selectedEntity->linkedEntity == id) {
			Editor::EditorScene::s_selectedEntity = nullptr;
		}

		NE::ECS::Command::DestroyEntity(m_entity);
	}

	CreateUIImageEntityCommand::CreateUIImageEntityCommand(uint32_t parentCanvas)
		: m_entity(0), m_parentCanvas(parentCanvas) {
	}

	void CreateUIImageEntityCommand::Execute()
	{
		m_entity = NE::ECS::Command::CreateUIImageEntity(m_parentCanvas);
		EditorScene::s_entities.push_back(EditorEntity{ m_entity });
		Editor::EditorScene::s_selectedEntity = &EditorScene::s_entities.back();

		// setup as child node in hierarchy
		Editor::Node node{};
		node.id = m_entity;
		node.parent = m_parentCanvas;  // image belongs to canvas

		// order key based on number of existing children
		auto& children = EditorScene::s_children[m_parentCanvas];
		node.orderKey = static_cast<float>(children.size());

		EditorScene::s_nodes[m_entity] = node;
		children.push_back(m_entity);  // add to parent's children list
	}

	void CreateUIImageEntityCommand::Undo()
	{
		const uint32_t id = m_entity;

		// remove from editor entities list
		auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
			[id](const EditorEntity& entt) {
				return entt.linkedEntity == id;
			});

		if (it != EditorScene::s_entities.end()) 
		{
			EditorScene::s_entities.erase(it);
		}

		// remove from scene graph
		EditorScene::s_nodes.erase(id);

		// remove from roots
		auto& roots = EditorScene::s_roots;
		roots.erase(std::remove(roots.begin(), roots.end(), id), roots.end());

		// remove from all parent's children lists
		for (auto& [parent, vec] : EditorScene::s_children) 
		{
			vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
		}

		// clear selection if needed
		if (Editor::EditorScene::s_selectedEntity &&
			Editor::EditorScene::s_selectedEntity->linkedEntity == id) {
			Editor::EditorScene::s_selectedEntity = nullptr;
		}

		NE::ECS::Command::DestroyEntity(m_entity);
	}

	DeleteEntityCommand::DeleteEntityCommand(uint32_t deletedEntity) : m_entity(deletedEntity) {}

	void DeleteEntityCommand::Execute()
	{
		const uint32_t rootId = m_entity;

		std::vector<uint32_t> toDelete;
		EditorScene::GetAllDescendants(rootId, toDelete);

		for (uint32_t id : toDelete) {
			{
				auto it = std::find_if(
					EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
					[id](const EditorEntity& e) { return e.linkedEntity == id; }
				);
				if (it != EditorScene::s_entities.end()) {
					EditorScene::s_entities.erase(it);
				}
			}

			EditorScene::s_nodes.erase(id);

			auto& roots = EditorScene::s_roots;
			roots.erase(std::remove(roots.begin(), roots.end(), id), roots.end());

			for (auto& [parent, vec] : EditorScene::s_children) {
				vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
			}

			NE::ECS::Command::DestroyEntity(id);
		}

		if (EditorScene::s_selectedEntity &&
			std::find(toDelete.begin(), toDelete.end(),
				EditorScene::s_selectedEntity->linkedEntity) != toDelete.end()) {
			EditorScene::s_selectedEntity = nullptr;
		}
	}

	void DeleteEntityCommand::Undo()
	{
		m_entity = NE::ECS::Command::CreateEntity();
		const uint32_t id = m_entity;

		EditorScene::s_entities.push_back(EditorEntity{ id });

		Editor::Node node{};
		node.id = id;
		node.parent = NE::ECS::NO_ENTITY;
		node.orderKey = static_cast<float>(EditorScene::s_roots.size());

		EditorScene::s_nodes[id] = node;
		EditorScene::s_roots.push_back(id);

		EditorScene::s_entities.push_back(EditorEntity{ id });
		Editor::EditorScene::s_selectedEntity = &EditorScene::s_entities.back();
	}

}