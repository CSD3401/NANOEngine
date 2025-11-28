#include "HierarchyPanel.hpp"
#include <imgui/imgui.h>
#include "../Command/CommandHistory.hpp"
#include "../Command/EditorCommands.hpp"
#include "EditorInterface/ECSExports.hpp"
#include "../EditorScene.hpp"
#include "Events/EventBus.hpp"
#include "../EditorEvents.hpp"
#include <ECS/Core/Entity.hpp>
#include <Engine.hpp>
#include <imgui/imgui_internal.h>
#include <algorithm>
#include <ECS/Components/EntityMeta.hpp>
#include "../AssetManagement/AssetManager.hpp"
#include <Math/Vec3.hpp>

namespace {
	// Lowercase helper
	std::string ToLower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });
		return s;
	}

	bool MarkVisibleRecursive(
		uint32_t id,
		const std::string& queryLower,
		std::unordered_set<uint32_t>& outVisible)
	{
		using namespace Editor;

		const auto& meta = NE::ECS::Query::GetEntityMeta(id);
		std::string nameLower = ToLower(meta.name);

		bool selfMatch = queryLower.empty()
			? true
			: (nameLower.find(queryLower) != std::string::npos);

		const auto& kids = Editor::EditorScene::ChildrenOf(id);
		bool anyChildMatch = false;
		for (uint32_t child : kids) {
			if (MarkVisibleRecursive(child, queryLower, outVisible))
				anyChildMatch = true;
		}

		if (selfMatch || anyChildMatch) {
			outVisible.insert(id);
			return true;
		}
		return false;
	}
}


namespace Editor {
	HierarchyPanel::HierarchyPanel() {
		auto numEntt = NE::GetNumEntities();
		EditorScene::s_entities.reserve(numEntt.size());
		for (auto e : numEntt) {
			EditorScene::s_entities.push_back(EditorEntity{ e });
		}
	}

	void HierarchyPanel::OnImGuiRender() {
		ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_MenuBar);

		static bool filtering = false;
		std::unordered_set<uint32_t> visible;
		if (ImGui::BeginMenuBar()) {
			static char s_searchBuf[128] = "";
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##HierarchySearch", "Search...", s_searchBuf, IM_ARRAYSIZE(s_searchBuf));

			std::string search = s_searchBuf;
			std::string searchLower = ToLower(search);
			filtering = !searchLower.empty();

			// Build visible set when filtering
			auto& childrenOf0 = Editor::EditorScene::ChildrenOf(NE::ECS::NO_ENTITY);
			if (filtering) {
				for (uint32_t root : childrenOf0)
					MarkVisibleRecursive(root, searchLower, visible);
			}

			ImGui::EndMenuBar();
		}

		bool canEditHierarchy = EditorScene::selectedPrefab.empty();

		if (ImGui::IsWindowHovered() &&
			ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
			EditorScene::s_selectedEntity != nullptr && canEditHierarchy) {

			NANOEngine::Events::EventBus::Get().Dispatch(
				NANOEngine::Events::EventDomain::Editor,
				DeleteEntityEvent{ EditorScene::s_selectedEntity->linkedEntity }
			);
		}

		if (ImGui::BeginPopupContextWindow(
			"HierarchyContextMenu",
			ImGuiPopupFlags_MouseButtonRight)) {

			DrawHierarchyContextMenuBody(canEditHierarchy, NE::ECS::NO_ENTITY);
			ImGui::EndPopup();
		}

		// === Entity Tree ===
		//static bool s_built = false;
		//if (!s_built) { Editor::EditorScene::BuildFlatHierarchy(); s_built = true; }

		if (EditorScene::selectedPrefab != "") {
			if (ImGui::Button("<")) {
				NE::ClosePrefabScene();
				EditorScene::s_selectedEntity = nullptr;
				EditorScene::RebuildFromActiveScene();
				EditorScene::selectedPrefab = "";
			}

			ImGui::SameLine();
			ImGui::Text(EditorScene::selectedPrefab.c_str());
			ImGui::SameLine();

			if (ImGui::Button("Save")) {
				NE::SavePrefabScene(EditorScene::selectedPrefab);
				std::string uuid = AssetManager::GetInstance().RetrieveUUID(EditorScene::selectedPrefab);
				NE::ReloadAllInstancesOfPrefab(uuid, EditorScene::selectedPrefab);
			}

			ImGui::Separator();
		}

		static bool s_built = false;
		if (!s_built) {
			Editor::EditorScene::BuildHierarchyFromECS();
			s_built = true;
		}

		// ---- Drag state ----
		static uint32_t draggingId = NE::ECS::NO_ENTITY;

		static bool     previewAsChild = false;  // highlight a row to adopt as parent
		static uint32_t previewParent = NE::ECS::NO_ENTITY;

		static uint32_t previewParentForInsert = NE::ECS::NO_ENTITY; // parent whose sibling list will get the line
		static int      previewInsert = -1;         // index within that parent�s children
		static float    previewLineY = -1.0f;       // cached Y for the line
		static float    previewLineX1 = 0.f, previewLineX2 = 0.f;

		// Selection state - delay selection until we know user isn't dragging
		static uint32_t clickedEntityId = NE::ECS::NO_ENTITY;
		static Editor::EditorEntity* clickedEntity = nullptr;
		static bool clickedThisFrame = false;

		auto& childrenOf0 = Editor::EditorScene::ChildrenOf(NE::ECS::NO_ENTITY);

		ImDrawList* dl = ImGui::GetWindowDrawList();

		std::function<void(uint32_t , const std::vector<uint32_t>& , int )> DrawLevel;
		DrawLevel = [&](uint32_t parent, const std::vector<uint32_t>& siblings, int depth) {
			for (int i = 0; i < (int)siblings.size(); ++i) {
				uint32_t id = siblings[i];

				if (filtering && visible.find(id) == visible.end())
					continue;

				// -------- label & selection ----------
				Editor::EditorEntity* ent = nullptr;
				for (auto& e : Editor::EditorScene::s_entities) { if (e.linkedEntity == id) { ent = &e; break; } }
				//std::string label = (ent ? ent->displayName : std::string("Entity")) + "##" + std::to_string(id);

				std::string entityName;
				const auto& meta = NE::ECS::Query::GetEntityMeta(id);
				entityName = !meta.name.empty() ? meta.name : "Entity";

				std::string label = entityName + "##" + std::to_string(id);

				const auto& kids = Editor::EditorScene::ChildrenOf(id);
				bool isLeaf = kids.empty();

				ImGuiTreeNodeFlags flags =
					ImGuiTreeNodeFlags_SpanAvailWidth |
					(isLeaf ? ImGuiTreeNodeFlags_Leaf : 0);

				if (Editor::EditorScene::s_selectedEntity && ent == Editor::EditorScene::s_selectedEntity)
					flags |= ImGuiTreeNodeFlags_Selected;

				// --- Color logic ---------------------------------------------------
				bool isActive = meta.isActive;
				bool isPrefab = !meta.prefabID.empty();

				ImVec4 baseText = ImGui::GetStyleColorVec4(ImGuiCol_Text);
				ImVec4 disabled = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
				ImVec4 prefabBlue = ImVec4(0.35f, 0.65f, 1.0f, 1.0f); // tweak to taste

				ImVec4 finalColor = baseText;
				bool useCustomColor = false;

				if (isPrefab && isActive) {
					finalColor = prefabBlue;
					useCustomColor = true;
				} else if (!isActive && !isPrefab) {
					finalColor = disabled;
					useCustomColor = true;
				} else if (!isActive && isPrefab) {
					const float t = 0.4f; // 0 = fully gray, 1 = fully blue
					finalColor.x = disabled.x * (1.0f - t) + prefabBlue.x * t;
					finalColor.y = disabled.y * (1.0f - t) + prefabBlue.y * t;
					finalColor.z = disabled.z * (1.0f - t) + prefabBlue.z * t;
					finalColor.w = 1.0f;
					useCustomColor = true;
				}

				if (useCustomColor)
					ImGui::PushStyleColor(ImGuiCol_Text, finalColor);

				if (EditorScene::s_forceOpen.contains(id) || filtering) {
					ImGui::SetNextItemOpen(true, ImGuiCond_Always);
				}

				bool open = ImGui::TreeNodeEx((void*)(uintptr_t)id, flags, "%s", label.c_str());

				if (useCustomColor)
					ImGui::PopStyleColor();

				// Delay selection logic - only select if not starting a drag
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
					clickedEntityId = id;
					clickedEntity = ent;
					clickedThisFrame = true;
				}

				// row rect
				ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

				// DO NOT REMOVE - Needed for tween to work
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && EditorScene::s_selectedEntity != nullptr) {
					// Broadcast message
					NANOEngine::Events::EventBus::Get().Dispatch(NANOEngine::Events::EventDomain::Editor, SelectEntityEvent(EditorScene::s_selectedEntity->linkedEntity));
				}

				// -------- begin drag from this row ----------
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
					draggingId = id;

					// Use HIER_DRAG_ID for hierarchy rearrangement
					   // Inspector will peek at this payload to get the entity ID
					ImGui::SetDragDropPayload("HIER_DRAG_ID", &draggingId, sizeof(uint32_t));
					ImGui::TextUnformatted(label.c_str());
					ImGui::EndDragDropSource();

					// Cancel selection since we're dragging
					clickedThisFrame = false;
				}

				// -------- hover bands while dragging ----------
				if (draggingId != NE::ECS::NO_ENTITY && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					//hadDragThisFrame = true; // warning unused var - RF

					if (ImGui::IsMouseHoveringRect(r.Min, r.Max, true)) {
						const float h = r.Max.y - r.Min.y;
						const float y = ImGui::GetIO().MousePos.y;
						const float topBandEnd = r.Min.y + 0.25f * h;
						const float bottomBandBeg = r.Max.y - 0.25f * h;

						if (y < topBandEnd) {
							// insert above this row (same parent)
							previewAsChild = false;
							previewParent = NE::ECS::NO_ENTITY;
							previewParentForInsert = parent;
							previewInsert = i;       // before i
							previewLineY = r.Min.y;
							previewLineX1 = r.Min.x; previewLineX2 = r.Max.x;
						} else if (y > bottomBandBeg) {
							// insert below this row (same parent)
							previewAsChild = false;
							previewParent = NE::ECS::NO_ENTITY;
							previewParentForInsert = parent;
							previewInsert = i + 1;   // after i
							previewLineY = r.Max.y;
							previewLineX1 = r.Min.x; previewLineX2 = r.Max.x;
						} else {
							// adopt as child of this row
							previewAsChild = true;
							previewParent = id;

							// clear sibling-line preview
							previewParentForInsert = NE::ECS::NO_ENTITY;
							previewInsert = -1;
							previewLineY = -1.f;

							// highlight this row
							dl->AddRectFilled(r.Min, r.Max, IM_COL32(255, 255, 0, 32), 4.0f);
							dl->AddRect(r.Min, r.Max, IM_COL32(255, 255, 0, 160), 4.0f, 0, 2.0f);
						}
					}
				}

				// -------- recurse if open ----------
				if (open) {
					if (!isLeaf) {
						DrawLevel(id, kids, depth + 1);
					}
					ImGui::TreePop();
				}
			}
			};

		DrawLevel(NE::ECS::NO_ENTITY, childrenOf0, 0);

		bool hierHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

		// --- preview line & auto-scroll only when hovered ---
		if (hierHovered) {
			if (draggingId != NE::ECS::NO_ENTITY && previewInsert >= 0 && previewLineY >= 0.f) {
				dl->AddLine(ImVec2(previewLineX1, previewLineY), ImVec2(previewLineX2, previewLineY), IM_COL32(255, 255, 0, 200), 2.0f);
				dl->AddLine(ImVec2(previewLineX1, previewLineY - 3), ImVec2(previewLineX1, previewLineY + 3), IM_COL32(255, 255, 0, 200), 2.0f);
				dl->AddLine(ImVec2(previewLineX2, previewLineY - 3), ImVec2(previewLineX2, previewLineY + 3), IM_COL32(255, 255, 0, 200), 2.0f);
			}

			{
				ImGuiWindow* win = ImGui::GetCurrentWindow();
				const float innerTop = win->InnerRect.Min.y;
				const float innerBot = win->InnerRect.Max.y;
				const float mouseY = ImGui::GetIO().MousePos.y;
				const float margin = 18.0f;
				const float speed = 12.0f;
				if (draggingId != NE::ECS::NO_ENTITY && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					if (mouseY < innerTop + margin) ImGui::SetScrollY(ImGui::GetScrollY() - speed);
					else if (mouseY > innerBot - margin) ImGui::SetScrollY(ImGui::GetScrollY() + speed);
				}
			}
		}

		// --- commit drag on mouse release ---
		if (draggingId != NE::ECS::NO_ENTITY && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (hierHovered) {
				if (previewAsChild && previewParent != NE::ECS::NO_ENTITY) {
					EditorScene::AttachAsChild(previewParent, draggingId, /*insertIndex*/ -1);
				} else if (previewInsert >= 0) {
					EditorScene::AttachAsChild(previewParentForInsert, draggingId, previewInsert);
				}
			}

			draggingId = NE::ECS::NO_ENTITY;
			previewAsChild = false;
			previewParent = NE::ECS::NO_ENTITY;
			previewParentForInsert = NE::ECS::NO_ENTITY;
			previewInsert = -1;
			previewLineY = -1.f;
		}

		if (clickedThisFrame && !ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
			ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			if (hierHovered) {
				EditorScene::s_selectedEntity = clickedEntity;
				EditorScene::selectedAsset = "";
			}

			clickedEntityId = NE::ECS::NO_ENTITY;
			clickedEntity = nullptr;
			clickedThisFrame = false;
		} else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			clickedThisFrame = false;
		}

		EditorScene::s_forceOpen.clear();
		ImGui::End();
	}

	void HierarchyPanel::DrawHierarchyContextMenuBody(bool canEditHierarchy, uint32_t contextEntityId) {
		// contextEntityId == NE::ECS::NO_ENTITY = clicked on empty
		//const bool hasEntity = (contextEntityId != NE::ECS::NO_ENTITY);

		const bool hasEntity = (EditorScene::s_selectedEntity != nullptr);

		if (ImGui::MenuItem("Cut", "Ctrl+X", false, hasEntity && canEditHierarchy)) {
			// TODO: implement cut
		}
		if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasEntity && canEditHierarchy)) {
			EditorScene::CopySelected();
		}
		if (ImGui::MenuItem("Paste", "Ctrl+V", false, canEditHierarchy)) {
			EditorScene::PasteSelected();
		}
		if (ImGui::MenuItem("Rename", "", false, hasEntity && canEditHierarchy)) {
			// TODO: rename
		}
		if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasEntity && canEditHierarchy)) {
			EditorScene::DuplicateSelected();
		}
		if (ImGui::MenuItem("Delete", "Del", false, hasEntity && canEditHierarchy)) {
			uint32_t idToDelete = contextEntityId;
			if (idToDelete == NE::ECS::NO_ENTITY && EditorScene::s_selectedEntity)
				idToDelete = EditorScene::s_selectedEntity->linkedEntity;

			if (idToDelete != NE::ECS::NO_ENTITY) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					DeleteEntityEvent{ idToDelete }
				);
			}
		}

		ImGui::Separator();
		ImGui::MenuItem("Select All", "", false, false);
		ImGui::MenuItem("Deselect All", "", false, false);
		ImGui::MenuItem("Invert Selection", "", false, false);
		ImGui::MenuItem("Select Children", "", false, false);

		ImGui::Separator();
		ImGui::MenuItem("Find References in Scene", "", false, false);
		ImGui::Separator();
		ImGui::MenuItem("Set as Default Parent", "", false, false);
		ImGui::Separator();

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

		ImGui::MenuItem("Camera", "", false, false);

		ImGui::Separator();

		if (ImGui::BeginMenu("UI")) {
			if (ImGui::MenuItem("Canvas")) {
				NANOEngine::Events::EventBus::Get().Dispatch(
					NANOEngine::Events::EventDomain::Editor,
					CreateUICanvasEntityEvent{}
				);
			}

			if (EditorScene::s_selectedEntity) {
				bool isCanvas = NE::ECS::Query::HasUICanvas(EditorScene::s_selectedEntity->linkedEntity);
				if (isCanvas) {
					ImGui::Separator();
					if (ImGui::MenuItem("Image")) {
						NANOEngine::Events::EventBus::Get().Dispatch(
							NANOEngine::Events::EventDomain::Editor,
							CreateUIImageEntityEvent{ EditorScene::s_selectedEntity->linkedEntity }
						);
					}
					if (ImGui::MenuItem("Text")) {
					}
					if (ImGui::MenuItem("Button")) {
					}
				}
			}


			ImGui::EndMenu();
		}
	}

}