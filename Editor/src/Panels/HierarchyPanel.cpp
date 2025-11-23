#include "HierarchyPanel.hpp"
#include <imgui/imgui.h>
#include "../Command/CommandHistory.hpp"
#include "../Command/EditorCommands.hpp"
#include "../EditorScene.hpp"
#include "Events/EventBus.hpp"
#include "../EditorEvents.hpp"
#include <ECS/Core/Entity.hpp>
#include <Engine.hpp>
#include <imgui/imgui_internal.h>
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/EntityMeta.hpp>

namespace Editor {
	HierarchyPanel::HierarchyPanel() {
		EditorScene::s_entities.reserve(NE::ECS::MAX_ENTITIES);

		auto numEntt = NE::GetNumEntities();
		for (unsigned int i = 0; i < numEntt; ++i) {
			EditorScene::s_entities.push_back(EditorEntity{ i });
		}
	}

	void HierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Hierarchy", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
			| ImGuiWindowFlags_MenuBar);

		//ImVec2 panelPos = ImGui::GetCursorScreenPos(); // warning unused var - RF
		//ImVec2 panelSize = ImGui::GetContentRegionAvail(); // warning unused var - RF

		if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
			ImGui::OpenPopup("HierarchyContextMenu");
		}

		if (ImGui::BeginPopupContextWindow("HierarchyContextMenu")) {
			if (ImGui::MenuItem("Cut", "Ctrl+X", false, false)) {
			}
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, false)) {
			}
			if (ImGui::MenuItem("Paste", "Ctrl+V", false, false)) {
			}
			if (ImGui::MenuItem("Paste Special", "", false, false)) {
			}
			if (ImGui::MenuItem("Rename", "", false, false)) {
			}
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, false)) {
			}
			if (ImGui::MenuItem("Delete", "Del", false, EditorScene::s_selectedEntity)) {
				NANOEngine::Events::EventBus::Get().Dispatch(NANOEngine::Events::EventDomain::Editor, DeleteEntityEvent{ EditorScene::s_selectedEntity->linkedEntity });
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Select All", "", false, false)) {
			}
			if (ImGui::MenuItem("Deselect All", "", false, false)) {
			}
			if (ImGui::MenuItem("Invert Selection", "", false, false)) {
			}
			if (ImGui::MenuItem("Select Children", "", false, false)) {
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Find References in Scene", "", false, false)) {
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Set as Default Parent", "", false, false)) {
			}
			ImGui::Separator();

			if (ImGui::MenuItem("Create Entity")) {
				NANOEngine::Events::EventBus::Get().Dispatch(NANOEngine::Events::EventDomain::Editor, CreateEntityEvent{});
				//NE::
				//Editor::EditorScene::BuildFlatHierarchy();
				// need to add into display list also currently creates but not shown in hierarchy
			}
			if (ImGui::BeginMenu("3D Object")) { // Creates a submenu with an arrow
				if (ImGui::MenuItem("Cube")) {
				}
				if (ImGui::MenuItem("Sphere")) {
				}
				if (ImGui::MenuItem("Capsule")) {
				}
				if (ImGui::MenuItem("Cylinder")) {
				}
				if (ImGui::MenuItem("Plane")) {
				}
				if (ImGui::MenuItem("Quad")) {
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Camera")) {
				//CreateCameraEntity();
			}

			ImGui::Separator();

			if (ImGui::BeginMenu("UI")) { // Creates a submenu with an arrow
				if (ImGui::MenuItem("Create Textbox")) {
					//CreateTextboxUIEntity();
				}
				if (ImGui::MenuItem("Create Image")) {
					//CreateQuadUIEntity();
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}

		// === Entity Tree ===
		//static bool s_built = false;
		//if (!s_built) { Editor::EditorScene::BuildFlatHierarchy(); s_built = true; }
		//static bool s_built = false;
		//if (!s_built) {
		//	Editor::EditorScene::BuildHierarchyFromECS();
		//	s_built = true;
		//}

		if (EditorScene::selectedPrefab != "") {
			if (ImGui::Button("<")) {
				NE::ClosePrefabScene();
				EditorScene::RebuildFromActiveScene();
				EditorScene::selectedPrefab = "";
			}

			ImGui::SameLine();
			ImGui::Text(EditorScene::selectedPrefab.c_str());
			ImGui::Separator();
		}

		static int s_lastEntityCount = -1;
		int currentCount = (int)NE::GetNumEntities();

		if (currentCount != s_lastEntityCount) {
			Editor::EditorScene::RebuildFromActiveScene();
			s_lastEntityCount = currentCount;
		}

		// ---- Drag state ----
		static uint32_t draggingId = NE::ECS::NO_ENTITY;

		static bool     previewAsChild = false;  // highlight a row to adopt as parent
		static uint32_t previewParent = NE::ECS::NO_ENTITY;

		static uint32_t previewParentForInsert = NE::ECS::NO_ENTITY; // parent whose sibling list will get the line
		static int      previewInsert = -1;         // index within that parent’s children
		static float    previewLineY = -1.0f;       // cached Y for the line
		static float    previewLineX1 = 0.f, previewLineX2 = 0.f;

		// Selection state - delay selection until we know user isn't dragging
		static uint32_t clickedEntityId = NE::ECS::NO_ENTITY;
		static Editor::EditorEntity* clickedEntity = nullptr;
		static bool clickedThisFrame = false;

		auto& childrenOf0 = Editor::EditorScene::ChildrenOf(NE::ECS::NO_ENTITY);

		ImDrawList* dl = ImGui::GetWindowDrawList();

		std::function<void(uint32_t /*parent*/, const std::vector<uint32_t>& /*siblings*/, int /*depth*/)> DrawLevel;
		DrawLevel = [&](uint32_t parent, const std::vector<uint32_t>& siblings, int depth) {
			for (int i = 0; i < (int)siblings.size(); ++i) {
				uint32_t id = siblings[i];

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
					// Active prefab -> blue
					finalColor = prefabBlue;
					useCustomColor = true;
				} else if (!isActive && !isPrefab) {
					// Inactive non-prefab -> gray
					finalColor = disabled;
					useCustomColor = true;
				} else if (!isActive && isPrefab) {
					// Inactive prefab -> "grayed-out blue" (blend disabled + blue)
					const float t = 0.4f; // 0 = fully gray, 1 = fully blue
					finalColor.x = disabled.x * (1.0f - t) + prefabBlue.x * t;
					finalColor.y = disabled.y * (1.0f - t) + prefabBlue.y * t;
					finalColor.z = disabled.z * (1.0f - t) + prefabBlue.z * t;
					finalColor.w = 1.0f;
					useCustomColor = true;
				}

				if (useCustomColor)
					ImGui::PushStyleColor(ImGuiCol_Text, finalColor);

				// -------------------------------------------------------------------
				bool open = ImGui::TreeNodeEx((void*)(uintptr_t)id, flags, "%s", label.c_str());

				if (useCustomColor)
					ImGui::PopStyleColor();

				// Optional: auto-open non-leaf by default
				// if (!isLeaf) ImGui::SetNextItemOpen(true, ImGuiCond_Once);

				// Delay selection logic - only select if not starting a drag
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
					clickedEntityId = id;
					clickedEntity = ent;
					clickedThisFrame = true;
				}

				// row rect
				ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

				// DO NOT REMOVE - Needed for tween to work
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && EditorScene::s_selectedEntity != nullptr)
				{
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
						}
						else if (y > bottomBandBeg) {
							// insert below this row (same parent)
							previewAsChild = false;
							previewParent = NE::ECS::NO_ENTITY;
							previewParentForInsert = parent;
							previewInsert = i + 1;   // after i
							previewLineY = r.Max.y;
							previewLineX1 = r.Min.x; previewLineX2 = r.Max.x;
						}
						else {
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

		ImGui::End();
	}

}