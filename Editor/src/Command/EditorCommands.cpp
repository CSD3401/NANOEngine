#include "EditorCommands.hpp"
#include "ECSInternals.hpp"
#include "EditorInterface/ECSExports.hpp"
#include "../EditorScene.hpp"
#include <ECS/Core/Entity.hpp>
#include <algorithm>

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

		// store information about deleted UI entities before destroying them
		m_deletedEntities.clear();
		for (uint32_t id : toDelete) {
			DeletedUIEntityInfo info;
			info.id = id;
			info.wasCanvas = NE::ECS::Query::HasUICanvas(id);
			info.wasUIImage = NE::ECS::Query::HasUIImage(id);
			info.parentId = NE::ECS::Command::GetParent(id);
			m_deletedEntities.push_back(info);
		}

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
		// Sort entities so parents are recreated before children
			// (entities with no parent first, then their children, etc.)
		std::sort(m_deletedEntities.begin(), m_deletedEntities.end(),
			[](const DeletedUIEntityInfo& a, const DeletedUIEntityInfo& b) {
				// Canvases (no parent) should come first
				if (a.parentId == NE::ECS::NO_ENTITY && b.parentId != NE::ECS::NO_ENTITY) return true;
				if (a.parentId != NE::ECS::NO_ENTITY && b.parentId == NE::ECS::NO_ENTITY) return false;
				return false;
			});

		std::unordered_map<uint32_t, uint32_t> oldToNewId;  // Map old IDs to new IDs

		for (const auto& info : m_deletedEntities) {
			uint32_t newEntity;
			uint32_t newParentId = NE::ECS::NO_ENTITY;
			if (info.parentId != NE::ECS::NO_ENTITY)
			{
				// If the parent was also deleted & recreated, remap to the NEW id
				auto it = oldToNewId.find(info.parentId);
				if (it != oldToNewId.end()) 
				{
					newParentId = it->second; // parent was recreated
				}
				else 
				{
					newParentId = info.parentId; // parent still exists, keep original id
				}
			}

			// Recreate the correct type of entity
			if (info.wasCanvas) {
				newEntity = NE::ECS::Command::CreateUICanvasEntity();
			}
			else if (info.wasUIImage) {
				newEntity = NE::ECS::Command::CreateUIImageEntity(newParentId);
			}
			else {
				// Regular 3D entity
				newEntity = NE::ECS::Command::CreateEntity();
			}

			oldToNewId[info.id] = newEntity;

			// Add to editor scene
			EditorScene::s_entities.push_back(EditorEntity{ newEntity });

			// Setup editor hierarchy
			Editor::Node node{};
			node.id = newEntity;

			if (newParentId != NE::ECS::NO_ENTITY) {
				// Has a parent
				node.parent = newParentId;
				auto& children = EditorScene::s_children[newParentId];
				node.orderKey = static_cast<float>(children.size());
				children.push_back(newEntity);
			}
			else {
				// Root entity
				node.parent = NE::ECS::NO_ENTITY;
				node.orderKey = static_cast<float>(EditorScene::s_roots.size());
				EditorScene::s_roots.push_back(newEntity);
			}

			EditorScene::s_nodes[newEntity] = node;
		}

		// Update m_entity to the new root ID (the canvas in this case)
		if (!m_deletedEntities.empty()) {
			// Find the root entity (the one with no parent)
			for (const auto& info : m_deletedEntities) {
				if (info.parentId == NE::ECS::NO_ENTITY) {
					m_entity = oldToNewId[info.id];
					break;
				}
			}

			// Select the recreated root entity
			auto it = std::find_if(EditorScene::s_entities.begin(), EditorScene::s_entities.end(),
				[id = m_entity](const EditorEntity& e) { return e.linkedEntity == id; });
			if (it != EditorScene::s_entities.end()) {
				Editor::EditorScene::s_selectedEntity = &(*it);
			}
		}
	}
}