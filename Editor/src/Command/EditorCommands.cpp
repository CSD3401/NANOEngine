#include "EditorCommands.hpp"
#include "ECSInternals.hpp"
#include "EditorInterface/ECSExports.hpp"
#include "../EditorScene.hpp"

namespace Editor {

	CreateEntityCommand::CreateEntityCommand() : m_entity(0) {}

	void CreateEntityCommand::Execute()
	{
		m_entity = NE::ECS::Command::CreateEntity();
		EditorScene::s_entities.push_back(EditorEntity{ m_entity });
		Editor::EditorScene::BuildFlatHierarchy();
		Editor::EditorScene::s_selectedEntity = &EditorScene::s_entities.back();
	}

	void CreateEntityCommand::Undo()
	{
		// temp
		auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
			[id = m_entity](const EditorEntity& entt) {
				return entt.linkedEntity == id;
			});

		if (it != EditorScene::s_entities.end()) {
			EditorScene::s_entities.erase(it);
		}

		Editor::EditorScene::s_selectedEntity = nullptr;
		NE::ECS::Command::DestroyEntity(m_entity);
		Editor::EditorScene::BuildFlatHierarchy();
	}

	DeleteEntityCommand::DeleteEntityCommand(uint32_t deletedEntity) : m_entity(deletedEntity) {}

	void DeleteEntityCommand::Execute()
	{
		// temp
		auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
			[id = m_entity](const EditorEntity& entt) {
				return entt.linkedEntity == id;
			});

		if (it != EditorScene::s_entities.end()) {
			EditorScene::s_entities.erase(it);
		}


		NE::ECS::Command::DestroyEntity(m_entity);
	}

	void DeleteEntityCommand::Undo()
	{
		m_entity = NE::ECS::Command::CreateEntity();
		EditorScene::s_entities.push_back(EditorEntity{ m_entity });
	}

}