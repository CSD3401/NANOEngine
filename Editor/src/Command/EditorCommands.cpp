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

	DeleteEntityCommand::DeleteEntityCommand(uint32_t deletedEntity) : m_entity(deletedEntity) {}

	void DeleteEntityCommand::Execute()
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