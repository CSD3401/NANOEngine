#include "EditorCommands.hpp"
#include "ECSInternals.hpp"
#include "../EditorScene.hpp"

namespace Editor {

	CreateEntityCommand::CreateEntityCommand(uint32_t* entity)
	{
		
		*entity = m_entity;
	}

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

}