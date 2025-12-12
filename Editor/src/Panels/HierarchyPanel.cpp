#include "HierarchyPanel.hpp"
#include <imgui/imgui.h>
#include "EditorInterface/ECSExports.hpp"
#include "../EditorScene.hpp"
#include "Events/EventBus.hpp"
#include "../EditorEvents.hpp"
#include <ECS/Core/Entity.hpp>
#include <Engine.hpp>
#include <imgui/imgui_internal.h>
//#include <algorithm>
#include <ECS/Components/EntityMeta.hpp>
//#include "../AssetManagement/AssetManager.hpp"
//#include <Math/Vec3.hpp>
#include <ECS/Components/Hierarchy.hpp>


namespace Editor {
	namespace {
		bool IsAncestor(NE::ECS::Entity ancestor, NE::ECS::Entity node) {
			using namespace NE::ECS;

			if (ancestor == NE::ECS::NO_ENTITY || node == NE::ECS::NO_ENTITY)
				return false;

			while (node != NE::ECS::NO_ENTITY) {
				auto& h = Query::GetEntityHierarchy(node);
				if (h.parent == NE::ECS::Component::INVALID_PARENT)
					break;

				Entity parent = static_cast<Entity>(h.parent);
				if (parent == ancestor)
					return true;

				node = parent;
			}
			return false;
		}
	}

	HierarchyPanel::HierarchyPanel() {
		EditorScene::s_rootOrder.reserve(256);
		auto& numEntities = NE::GetNumEntities();
		for (auto e : numEntities) {
			auto& h = NE::ECS::Query::GetEntityHierarchy(e);
			if (h.parent == NE::ECS::Component::INVALID_PARENT) {
				EditorScene::s_rootOrder.push_back(e);
			}
		}
	}

	void HierarchyPanel::OnImGuiRender() {
		using NE::ECS::Entity;

		ImGui::Begin("Hierarchy");

		selectionChanged = false;
		m_forceOpen.clear();

		NE::ECS::Entity anchor = EditorScene::s_selection.GetPrimary();

		if (anchor != m_lastPrimary) {
			m_lastPrimary = anchor;
			selectionChanged = true;

			if (anchor != NE::ECS::NO_ENTITY) {
				using NE::ECS::Entity;
				using NE::ECS::Component::INVALID_PARENT;

				Entity cur = anchor;
				while (cur != NE::ECS::NO_ENTITY) {
					m_forceOpen.insert(cur);

					auto& h = NE::ECS::Query::GetEntityHierarchy(cur);
					if (h.parent == INVALID_PARENT)
						break;

					cur = static_cast<Entity>(h.parent);
				}

				for (Entity e : m_forceOpen) {
					m_expanded[e] = true;
				}
			}
		}

		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && !ImGui::IsAnyItemHovered()) {
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				EditorScene::s_selection.Clear();
			}
		}

		std::vector<Entity> preorder;
		preorder.reserve(EditorScene::s_rootOrder.size());

		for (size_t i = 0; i < EditorScene::s_rootOrder.size(); ++i) {
			DrawEntityNode(EditorScene::s_rootOrder[i], preorder,
				NE::ECS::NO_ENTITY,
				static_cast<int>(i));
		}

		EditorScene::s_selection.SetLastPreorder(preorder);

		//if (ImGui::BeginPopup("HierarchyContext")) {
		//	DrawContextMenu();
		//	ImGui::EndPopup();
		//}

		ImDrawList* dl = ImGui::GetWindowDrawList();
		bool hierHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

		// --- preview line only when hovered & we have a line ---
		if (hierHovered &&
			m_dragRep != NE::ECS::NO_ENTITY &&
			m_previewInsert >= 0 &&
			m_previewLineY >= 0.f) {
			dl->AddLine(ImVec2(m_previewLineX1, m_previewLineY),
				ImVec2(m_previewLineX2, m_previewLineY),
				IM_COL32(255, 255, 0, 200), 2.0f);
			dl->AddLine(ImVec2(m_previewLineX1, m_previewLineY - 3),
				ImVec2(m_previewLineX1, m_previewLineY + 3),
				IM_COL32(255, 255, 0, 200), 2.0f);
			dl->AddLine(ImVec2(m_previewLineX2, m_previewLineY - 3),
				ImVec2(m_previewLineX2, m_previewLineY + 3),
				IM_COL32(255, 255, 0, 200), 2.0f);
		}

		// --- commit drag on mouse release ---
		if (m_dragRep != NE::ECS::NO_ENTITY &&
			ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (hierHovered && !m_draggedEntities.empty()) {
				if (m_previewAsChild && m_previewParent != NE::ECS::NO_ENTITY) {
					// Drop as children of previewParent, append as last child
					for (auto child : m_draggedEntities) {
						if (child == m_previewParent)
							continue;

						if (IsAncestor(child, m_previewParent))
							continue;

						EditorScene::SetParent(child,
							m_previewParent,
							std::numeric_limits<int>::max(),
							true);
					}
				} else if (m_previewInsert >= 0) {
					int insertIndex = m_previewInsert;

					if (m_previewParentForInsert == NE::ECS::NO_ENTITY) {
						for (auto child : m_draggedEntities) {
							auto& h = NE::ECS::Query::GetEntityHierarchy(child);
							if (h.parent != NE::ECS::Component::INVALID_PARENT) {
								EditorScene::SetParent(child,
									NE::ECS::NO_ENTITY,
									0,
									true);
							}

							EditorScene::ReorderRoot(child, insertIndex);
							++insertIndex;
						}
					} else {
						for (auto child : m_draggedEntities) {
							if (child == m_previewParentForInsert)
								continue;

							if (IsAncestor(child, m_previewParentForInsert))
								continue;

							EditorScene::SetParent(child,
								m_previewParentForInsert,
								insertIndex,
								true);
							++insertIndex;
						}
					}
				}
			}

			m_dragRep = NE::ECS::NO_ENTITY;
			m_draggedEntities.clear();
			m_previewAsChild = false;
			m_previewParent = NE::ECS::NO_ENTITY;
			m_previewParentForInsert = NE::ECS::NO_ENTITY;
			m_previewInsert = -1;
			m_previewLineY = -1.f;
		}

		if (m_clickThisFrame) {
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
				m_clickThisFrame = false;
				m_clickCandidate = NE::ECS::NO_ENTITY;
			}
		}

		if (m_clickThisFrame &&
			ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (m_clickCandidate != NE::ECS::NO_ENTITY) {
				auto& sel = EditorScene::s_selection;

				if (!m_clickHadCtrl && !m_clickHadShift) {
					sel.SetSingle(m_clickCandidate);
				} else if (m_clickHadCtrl) {
					sel.Toggle(m_clickCandidate);
				} else if (m_clickHadShift) {
					const auto& lastPreorder = sel.GetLastPreorder();
					sel.RangeSelect(sel.GetLastClicked(), m_clickCandidate, lastPreorder);
				}
			}

			m_clickThisFrame = false;
			m_clickCandidate = NE::ECS::NO_ENTITY;
			m_clickHadCtrl = false;
			m_clickHadShift = false;
		}

		if (ImGui::BeginPopupContextWindow("HierarchyContext",
			ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
			DrawContextMenu();
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void HierarchyPanel::DrawEntityNode(NE::ECS::Entity e, 
		std::vector<NE::ECS::Entity>& preorder, 
		NE::ECS::Entity parent, int indexInParent) 
	{
		preorder.push_back(e);

		auto& h = NE::ECS::Query::GetEntityHierarchy(e);
		bool isSelected = EditorScene::s_selection.Contains(e);
		bool hasChildren = !h.children.empty();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (!hasChildren)
			flags |= ImGuiTreeNodeFlags_Leaf;
		if (isSelected)
			flags |= ImGuiTreeNodeFlags_Selected;

		bool openInMap = m_expanded[e];
		if (openInMap) flags |= ImGuiTreeNodeFlags_DefaultOpen;

		auto& meta = NE::ECS::Query::GetEntityMeta(e);
		const char* name = meta.name.c_str();

		if (selectionChanged && m_forceOpen.count(e)) {
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}
		bool open = ImGui::TreeNodeEx((void*)(intptr_t)e, flags, "%s", name);
		
		HandleDragSource(e, preorder);

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			ImGuiIO& io = ImGui::GetIO();
			m_clickCandidate = e;
			m_clickHadCtrl = io.KeyCtrl;
			m_clickHadShift = io.KeyShift;
			m_clickThisFrame = true;
		}

		if (ImGui::BeginPopupContextItem("HierarchyContext")) {
			auto& sel = EditorScene::s_selection;
			if (!sel.Contains(e)) {
				sel.SetSingle(e);
			}

			DrawContextMenu(/*e*/);
			ImGui::EndPopup();
		}

		HandleDropTargets(e, parent, indexInParent);

		m_expanded[e] = open;

		if (open) {
			for (int i = 0; i < static_cast<int>(h.children.size()); ++i) {
				NE::ECS::Entity child = static_cast<NE::ECS::Entity>(h.children[i]);
				DrawEntityNode(child, preorder, e, i);
			}
			ImGui::TreePop();
		}
	}

	void HierarchyPanel::HandleDragSource(NE::ECS::Entity e, const std::vector<NE::ECS::Entity>& /*preorder*/) {
		using NE::ECS::Entity;

		if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			return;

		m_draggedEntities.clear();

		if (EditorScene::s_selection.Contains(e)) {
			const auto& sel = EditorScene::s_selection.GetSelection();
			m_draggedEntities.reserve(sel.size());

			for (Entity s : sel) {
				auto& h = NE::ECS::Query::GetEntityHierarchy(s);
				if (h.parent == NE::ECS::Component::INVALID_PARENT ||
					!EditorScene::s_selection.Contains(static_cast<Entity>(h.parent))) {
					m_draggedEntities.push_back(s);
				}
			}
		} else {
			m_draggedEntities.push_back(e);
		}

		m_dragRep = m_draggedEntities.empty() ? NE::ECS::NO_ENTITY : m_draggedEntities.front();

		ImGui::SetDragDropPayload("ENTITY_DRAG",
			m_draggedEntities.data(),
			m_draggedEntities.size() * sizeof(Entity));

		ImGui::Text("Move %d object(s)", (int)m_draggedEntities.size());
		ImGui::EndDragDropSource();
	}

	void HierarchyPanel::HandleDropTargets(NE::ECS::Entity e, NE::ECS::Entity parent, int indexInParent) {
		using NE::ECS::Entity;

		if (m_dragRep == NE::ECS::NO_ENTITY)
			return;

		ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

		if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			return;

		if (!ImGui::IsMouseHoveringRect(r.Min, r.Max, true))
			return;

		ImDrawList* dl = ImGui::GetWindowDrawList();

		const float h = r.Max.y - r.Min.y;
		const float y = ImGui::GetIO().MousePos.y;
		const float topBandEnd = r.Min.y + 0.25f * h;
		const float bottomBandBeg = r.Max.y - 0.25f * h;

		if (y < topBandEnd) {
			m_previewAsChild = false;
			m_previewParent = NE::ECS::NO_ENTITY;
			m_previewParentForInsert = parent;
			m_previewInsert = indexInParent;
			m_previewLineY = r.Min.y;
			m_previewLineX1 = r.Min.x;
			m_previewLineX2 = r.Max.x;
		} else if (y > bottomBandBeg) {
			m_previewAsChild = false;
			m_previewParent = NE::ECS::NO_ENTITY;
			m_previewParentForInsert = parent;
			m_previewInsert = indexInParent + 1;
			m_previewLineY = r.Max.y;
			m_previewLineX1 = r.Min.x;
			m_previewLineX2 = r.Max.x;
		} else {
			m_previewAsChild = true;
			m_previewParent = e;

			m_previewParentForInsert = NE::ECS::NO_ENTITY;
			m_previewInsert = -1;
			m_previewLineY = -1.f;

			dl->AddRectFilled(r.Min, r.Max,
				IM_COL32(255, 255, 0, 32), 4.0f);
			dl->AddRect(r.Min, r.Max,
				IM_COL32(255, 255, 0, 160), 4.0f, 0, 2.0f);
		}
	}

	void HierarchyPanel::DrawContextMenu() {
		if (ImGui::MenuItem("Create Entity", "", false, EditorScene::selectedPrefab.empty())) {
			NANOEngine::Events::EventBus::Get().Dispatch(
				NANOEngine::Events::EventDomain::Editor,
				CreateEntityEvent{}
			);
		}

		if (ImGui::BeginMenu("3D Object")) {
			ImGui::MenuItem("Cube", "", false, false);
			ImGui::MenuItem("Sphere", "", false, false);
			ImGui::MenuItem("Capsule", "", false, false);
			ImGui::MenuItem("Cylinder", "", false, false);
			ImGui::MenuItem("Plane", "", false, false);
			ImGui::MenuItem("Quad", "", false, false);
			ImGui::EndMenu();
		}
	}

}