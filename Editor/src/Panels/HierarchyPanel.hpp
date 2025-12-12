#pragma once
#include "IPanel.hpp"

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <ECS/Core/Entity.hpp>

namespace Editor {
	class HierarchyPanel final : public IPanel {
	public:
		HierarchyPanel();

		void OnImGuiRender() override;

	private:
		void DrawEntityNode(NE::ECS::Entity e, std::vector<NE::ECS::Entity>& preorder, NE::ECS::Entity parent, int indexInParent);
		void HandleDragSource(NE::ECS::Entity e,
			const std::vector<NE::ECS::Entity>& preorder);
		void HandleDropTargets(NE::ECS::Entity e,
			NE::ECS::Entity parent,
			int indexInParent);

		void DrawContextMenu();

		std::unordered_map<NE::ECS::Entity, bool> m_expanded;
		std::unordered_set<NE::ECS::Entity> m_forceOpen;

		NE::ECS::Entity m_lastPrimary = NE::ECS::NO_ENTITY;
		bool selectionChanged = false;

		NE::ECS::Entity m_dragRep = NE::ECS::NO_ENTITY;
		std::vector<NE::ECS::Entity> m_draggedEntities;

		bool            m_previewAsChild = false;
		NE::ECS::Entity m_previewParent = NE::ECS::NO_ENTITY;

		NE::ECS::Entity m_previewParentForInsert = NE::ECS::NO_ENTITY;
		int             m_previewInsert = -1;
		float           m_previewLineY = -1.f;
		float           m_previewLineX1 = 0.f;
		float           m_previewLineX2 = 0.f;

		NE::ECS::Entity m_clickCandidate = NE::ECS::NO_ENTITY;
		bool            m_clickHadCtrl = false;
		bool            m_clickHadShift = false;
		bool            m_clickThisFrame = false;
	};
}
