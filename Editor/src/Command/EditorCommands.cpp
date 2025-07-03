#include "EditorCommands.hpp"
#include "ECSInternals.hpp"
#include "../EditorScene.hpp"

namespace Editor {

	CreateEntityCommand::CreateEntityCommand() : m_entity(0) {}

	void CreateEntityCommand::Execute()
	{
		m_entity = NANOEngine::CreateEntity();
		EditorScene::s_entities.push_back(EditorEntity{ m_entity });
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

		
		NANOEngine::DestroyEntity(m_entity);
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


		NANOEngine::DestroyEntity(m_entity);
	}

	void DeleteEntityCommand::Undo()
	{
		m_entity = NANOEngine::CreateEntity();
		EditorScene::s_entities.push_back(EditorEntity{ m_entity });
	}

}