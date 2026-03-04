#include "pch.h"
#include "HierarchyPanel.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <Engine.hpp>
#include <ECS/Core/Entity.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Hierarchy.hpp>

#include "EditorInterface/ECSExports.hpp"
#include "../EditorScene.hpp"
#include "Events/EventBus.hpp"
#include "../EditorEvents.hpp"
#include "../Util/HierarchyUtils.hpp"
#include "../AssetManagement/AssetManager.hpp"
#include "../AssetManagement/Assets/PrefabAsset.hpp"

namespace Editor {
	namespace {
		std::string ToLower(std::string s) {
			for (auto& c : s)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return s;
		}

		bool MarkVisibleRecursive(NE::ECS::Entity e, const std::string& searchLower, std::unordered_set<uint32_t>& visible) {
			auto& meta = NE::ECS::Query::GetEntityMeta(e);
			const std::string nameLower = ToLower(meta.name);
			const bool selfMatch = !searchLower.empty() && nameLower.find(searchLower) != std::string::npos;

			auto& hierarchy = NE::ECS::Query::GetEntityHierarchy(e);
			bool descendantMatch = false;
			for (uint32_t child : hierarchy.children) {
				if (MarkVisibleRecursive(static_cast<NE::ECS::Entity>(child), searchLower, visible))
					descendantMatch = true;
			}

			if (selfMatch || descendantMatch) {
				visible.insert(static_cast<uint32_t>(e));
				return true;
			}

			return false;
		}

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

		std::vector<uint32_t> BuildDeleteRoots(const std::vector<uint32_t>& selection) {
			std::unordered_set<uint32_t> selected;
			selected.reserve(selection.size() * 2);
			for (auto e : selection) selected.insert(e);

			std::vector<uint32_t> roots;
			roots.reserve(selection.size());
			for (auto e : selection) {
				if (!Utility::IsDescendantOfSelected(e, selected))
					roots.push_back(e);
			}
			return roots;
		}
	}

	HierarchyPanel::HierarchyPanel() {
		EditorScene::BuildRoot();

		NANOEngine::Events::EventBus::Get().Subscribe<Events::SceneChangedEvent>(
			NANOEngine::Events::EventDomain::Editor,
			[&](const Events::SceneChangedEvent& e) {
				SceneChanged();
			}
		);
	}

	void HierarchyPanel::OnImGuiRender() {
		using NE::ECS::Entity;

		ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_MenuBar);

		static bool filtering = false;
		std::unordered_set<uint32_t> visible;
		if (ImGui::BeginMenuBar()) {
			if (ImGui::Button("+")) {
				ImGui::OpenPopup("HierarchyContextFromButton");
			}

			if (ImGui::BeginPopup("HierarchyContextFromButton")) { // TODO draw a different context menu for creation only
				DrawContextMenu();
				ImGui::EndPopup();
			}

			static char s_searchBuf[128] = "";
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##HierarchySearch", "Search...", s_searchBuf, IM_ARRAYSIZE(s_searchBuf));

			m_searchLower = ToLower(std::string(s_searchBuf));
			m_filtering = !m_searchLower.empty();
			m_visible.clear();
			if (m_filtering) {
				for (uint32_t root : EditorScene::s_rootOrder)
					MarkVisibleRecursive(static_cast<NE::ECS::Entity>(root), m_searchLower, m_visible);
			}

			ImGui::EndMenuBar();
		}

		if (EditorScene::selectedPrefab != "") {
			if (ImGui::Button("<")) {
				EditorScene::s_selection.Clear();
				NE::ClosePrefabScene();
				EditorScene::BuildRoot();
				EditorScene::selectedPrefab = "";
			}

			ImGui::SameLine();
			std::string selectedPrefabPath = Assets::AssetManager::GetInstance().RetrieveFilename(EditorScene::selectedPrefab);
			ImGui::Text(selectedPrefabPath.c_str());
			ImGui::SameLine();

			if (ImGui::Button("Save")) {
				auto record = Assets::AssetManager::GetInstance().GetRecord(EditorScene::selectedPrefab);
				dynamic_cast<Assets::PrefabAsset*>(record->asset.get())->SavePrefab(record->sourcePath.string(), true);
				
				NE::ReloadAllInstancesOfPrefab(EditorScene::selectedPrefab);
				//NE::SavePrefabScene(EditorScene::selectedPrefab);
				//std::string uuid = AssetManager::GetInstance().RetrieveUUID(EditorScene::selectedPrefab);
				//NE::ReloadAllInstancesOfPrefab(uuid, EditorScene::selectedPrefab);
			}

			ImGui::Separator();
		}

		selectionChanged = false;
		m_forceOpen.clear();

		NE::ECS::Entity anchor = EditorScene::s_selection.GetPrimary();

		if (anchor != m_lastPrimary) {
			m_lastPrimary = anchor;
			selectionChanged = true;

			if (anchor != NE::ECS::NO_ENTITY) {
				m_scrollToEntity = anchor;
				m_scrollToEntityPending = true;
			} else {
				m_scrollToEntity = NE::ECS::NO_ENTITY;
				m_scrollToEntityPending = false;
			}

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

		ImDrawList* dl = ImGui::GetWindowDrawList();
		bool hierHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

		if (hierHovered) {
			if (m_dragRep != NE::ECS::NO_ENTITY) {
				// Draw insert line logic
				if (m_previewInsert >= 0 && m_previewLineY >= 0.f) {
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

				// Scrolling logic
				ImGuiWindow* win = ImGui::GetCurrentWindow();
				const float innerTop = win->InnerRect.Min.y;
				const float innerBot = win->InnerRect.Max.y;
				const float mouseY = ImGui::GetIO().MousePos.y;
				const float margin = 18.0f;
				const float speed = 12.0f;
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					if (mouseY < innerTop + margin) ImGui::SetScrollY(ImGui::GetScrollY() - speed);
					else if (mouseY > innerBot - margin) ImGui::SetScrollY(ImGui::GetScrollY() + speed);
				}
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !EditorScene::s_selection.Empty()) {
				std::vector<uint32_t> toDelete = BuildDeleteRoots(EditorScene::s_selection.GetSelection());
				if (!toDelete.empty()) {
					NANOEngine::Events::EventBus::Get().Dispatch(
						NANOEngine::Events::EventDomain::Editor,
						Events::DeleteEntityEvent{ toDelete }
					);
				}
			}

			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::SelectEntityEvent(EditorScene::s_selection.GetLastClicked())
				);
			}
		}

		if (m_dragRep != NE::ECS::NO_ENTITY &&
			ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (hierHovered && !m_draggedEntities.empty()) {
				if (EditorScene::selectedPrefab.empty()) {
					for (auto child : m_draggedEntities) {
						if (!NE::ECS::Query::HasPrefabInstance(child) && NE::ECS::Query::HasPrefabLink(child)) {
							m_dragRep = NE::ECS::NO_ENTITY;
							m_draggedEntities.clear();
							m_previewAsChild = false;
							m_previewParent = NE::ECS::NO_ENTITY;
							m_previewParentForInsert = NE::ECS::NO_ENTITY;
							m_previewInsert = -1;
							m_previewLineY = -1.f;
							ImGui::End();
							return;
						}
					}
				}

				if (m_previewAsChild && m_previewParent != NE::ECS::NO_ENTITY) {
					for (auto child : m_draggedEntities) {
						if (child == m_previewParent)
							continue;

						if (IsAncestor(child, m_previewParent))
							continue;

						//EditorScene::SetParent(child,
						//	m_previewParent,
						//	std::numeric_limits<int>::max(),
						//	true);
						NANOEngine::Events::EventBus::Get().Dispatch(
							NANOEngine::Events::EventDomain::Editor,
							Events::HierarchyChangeEvent{ child, m_previewParent, std::numeric_limits<int>::max() }
						);
					}
				} else if (m_previewInsert >= 0) {
					int insertIndex = m_previewInsert;

					if (m_previewParentForInsert == NE::ECS::NO_ENTITY) {
						for (auto child : m_draggedEntities) {
							auto& h = NE::ECS::Query::GetEntityHierarchy(child);
							if (h.parent != NE::ECS::Component::INVALID_PARENT) {
								//EditorScene::SetParent(child,
								//	NE::ECS::NO_ENTITY,
								//	0,
								//	true);
								NANOEngine::Events::EventBus::Get().Dispatch(
									NANOEngine::Events::EventDomain::Editor,
									Events::HierarchyChangeEvent{ child, NE::ECS::NO_ENTITY, 0 }
								);
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

							//EditorScene::SetParent(child,
							//	m_previewParentForInsert,
							//	insertIndex,
							//	true);
							NANOEngine::Events::EventBus::Get().Dispatch(
								NANOEngine::Events::EventDomain::Editor,
								Events::HierarchyChangeEvent{ child, m_previewParentForInsert, insertIndex }
							);

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
			EditorScene::s_selection.Clear();
			DrawContextMenu();
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void HierarchyPanel::SceneChanged() {
		m_lastPrimary = NE::ECS::NO_ENTITY;
		m_dragRep = NE::ECS::NO_ENTITY;
		m_previewParentForInsert = NE::ECS::NO_ENTITY;
		m_clickCandidate = NE::ECS::NO_ENTITY;
		m_previewAsChild = false;
		m_scrollToEntity = NE::ECS::NO_ENTITY;
		m_scrollToEntityPending = false;

		m_filtering = false;
		m_visible.clear();

		m_expanded.clear();
		m_forceOpen.clear();
	}

	void HierarchyPanel::DrawEntityNode(NE::ECS::Entity e,
		std::vector<NE::ECS::Entity>& preorder,
		NE::ECS::Entity parent, int indexInParent)
	{
		if (m_filtering && m_visible.count(static_cast<uint32_t>(e)) == 0)
			return;

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
		const bool nameMatches = m_filtering && !m_searchLower.empty()
			&& ToLower(meta.name).find(m_searchLower) != std::string::npos;

		bool isActive = NE::ECS::Query::GetActive(e);
		bool isPrefab = NE::ECS::Query::HasPrefabLink(e);

		ImVec4 baseText = ImGui::GetStyleColorVec4(ImGuiCol_Text);
		ImVec4 disabled = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
		ImVec4 prefabBlue = ImVec4(0.35f, 0.65f, 1.0f, 1.0f);

		ImVec4 finalColor = baseText;
		bool useCustomColor = false;

		if (isPrefab && isActive) {
			finalColor = prefabBlue;
			useCustomColor = true;
		} else if (!isActive && !isPrefab) {
			finalColor = disabled;
			useCustomColor = true;
		} else if (!isActive && isPrefab) {
			const float t = 0.4f;
			finalColor.x = disabled.x * (1.0f - t) + prefabBlue.x * t;
			finalColor.y = disabled.y * (1.0f - t) + prefabBlue.y * t;
			finalColor.z = disabled.z * (1.0f - t) + prefabBlue.z * t;
			finalColor.w = 1.0f;
			useCustomColor = true;
		}

		if (useCustomColor)
			ImGui::PushStyleColor(ImGuiCol_Text, finalColor);

		if ((selectionChanged && m_forceOpen.count(e))
			|| (m_filtering && (m_visible.count(static_cast<uint32_t>(e)) > 0 || nameMatches))) {
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		}
		bool open = ImGui::TreeNodeEx((void*)(intptr_t)e, flags, "%s", name);

		if (m_scrollToEntityPending && e == m_scrollToEntity) {
			ImGui::SetScrollHereY(0.5f);
			m_scrollToEntityPending = false;
		}

		const bool toggledOpen = ImGui::IsItemToggledOpen();

		if (useCustomColor)
			ImGui::PopStyleColor();

		HandleDragSource(e, preorder);

		if (!toggledOpen && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			ImGuiIO& io = ImGui::GetIO();
			m_clickCandidate = e;
			m_clickHadCtrl = io.KeyCtrl;
			m_clickHadShift = io.KeyShift;
			m_clickThisFrame = true;
		}

		if (ImGui::BeginPopupContextItem()) {
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
		uint32_t parentEntityId = EditorScene::s_selection.GetLastClicked();
		if (!EditorScene::selectedPrefab.empty() && parentEntityId == NE::ECS::NO_ENTITY)
			parentEntityId = EditorScene::s_rootOrder[0];

		if (ImGui::MenuItem("Cut", "Ctrl+X", false, false)) {
			// TODO: implement cut
		}
		if (ImGui::MenuItem("Copy", "Ctrl+C", false, !EditorScene::s_selection.Empty())) {
			EditorScene::CopySelected();
		}
		if (ImGui::MenuItem("Paste", "Ctrl+V", false, !EditorScene::clipboard.empty())) {
			EditorScene::PasteSelected();
		}
		if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !EditorScene::s_selection.Empty())) {
			EditorScene::DuplicateSelected();
		}
		if (ImGui::MenuItem("Delete", "Del", false, !EditorScene::s_selection.Empty())) {
			std::vector<uint32_t> toDelete = BuildDeleteRoots(EditorScene::s_selection.GetSelection());
			if (!toDelete.empty()) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::DeleteEntityEvent{ toDelete }
				);
			}
		}

		ImGui::Separator();
		ImGui::MenuItem("Find References in Scene", "", false, false);

		ImGui::Separator();
		ImGui::MenuItem("Set as Default Parent", "", false, false);

		if (NE::ECS::Query::HasPrefabInstance(EditorScene::s_selection.GetPrimary())) {
			ImGui::Separator();
			if (ImGui::BeginMenu("Prefab")) {
				if (ImGui::MenuItem("Open Prefab", "", false, false)) {

				}
				if (ImGui::MenuItem("Unpack", "", false, true)) {
					NE::UnpackPrefab(EditorScene::s_selection.GetPrimary(), true);
				}
				ImGui::EndMenu();
			}

			//if (ImGui::MenuItem("Select Prefab Root", "", false, true)) {
			//	NE::ECS::Entity prefabRoot = NE::ECS::Query::GetPrefabInstanceRoot(
			//		EditorScene::s_selection.GetPrimary());
			//	EditorScene::s_selection.SetSingle(prefabRoot);
			//}
		}

		ImGui::Separator();
		if (ImGui::MenuItem("Create Entity", "", false, true)) {
			NANOEngine::Events::EventBus::Get().Dispatch(
				NANOEngine::Events::EventDomain::Editor,
				Events::CreateEmptyEntityEvent{ parentEntityId }
			);
		}

		if (ImGui::BeginMenu("3D Object")) {
			if (ImGui::MenuItem("Cube", "", false, true)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateCubeEntityEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Sphere", "", false, true)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateSphereEntityEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Capsule", "", false, true)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateCapsuleEntityEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Cylinder", "", false, true)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateCylinderEntityEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Plane", "", false, true)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreatePlaneEntityEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Quad", "", false, true)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateQuadEntityEvent{ parentEntityId }
				);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Light")) {
			if (ImGui::MenuItem("Directional Light", "", false, true)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateDirectionalLightEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Point Light", "", false, true)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreatePointLightEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Spot Light", "", false, true)) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateSpotLightEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Area Light", "", false, false)) {
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Audio")) {
			if (ImGui::MenuItem("Audio Source", "", false, false)) {

			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("UI")) {
			if (ImGui::MenuItem("Canvas")) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateUICanvasEvent{}
				);
			}
			if (ImGui::MenuItem("Text")) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateUITextEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Image")) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateUIImageEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Button")) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateUIButtonEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Panel")) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateUIPanelEvent{ parentEntityId }
				);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Slider")) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateUISliderEvent{ parentEntityId }
				);
			}
			if (ImGui::MenuItem("Toggle")) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					Events::CreateUIToggleEvent{ parentEntityId }
				);
			}
			ImGui::MenuItem("Input Field", "", false, false); // Phase 3
			ImGui::MenuItem("Scroll View", "", false, false); // Phase 3
			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("Camera", "", false, false)) {

		}
	}
}
