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

		ImVec2 panelPos = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

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
                Editor::EditorScene::BuildFlatHierarchy();
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
        //for (EditorEntity& entity : EditorScene::s_entities) {
        //    //std::string label = "Entity " + std::to_string(entity.linkedEntity);
        //    std::string name = "Entity"; // In future: get name from NameComponent
        //    std::string label = name + "##" + std::to_string(entity.linkedEntity);


        //    // Selectable, with tree node styling
        //    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        //    bool hasChildren = false;

        //    if (!hasChildren)
        //        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        //    if (&entity == EditorScene::s_selectedEntity)
        //        flags |= ImGuiTreeNodeFlags_Selected;

        //    bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)entity.linkedEntity, flags, "%s", label.c_str());

        //    // === Click Selection ===
        //    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        //    {
        //        EditorScene::s_selectedEntity = &entity;
        //    }

        //    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        //    {
        //        // Broadcast message
        //        NANOEngine::Events::EventBus::Get().Dispatch(NANOEngine::Events::EventDomain::Editor, SelectEntityEvent(EditorScene::s_selectedEntity->linkedEntity));
        //    }

        //    // === Right-click entity for context menu ===
        //    if (ImGui::BeginPopupContextItem((std::string("EntityContext") + std::to_string(entity.linkedEntity)).c_str())) {
        //        if (ImGui::MenuItem("Delete")) {
        //            NANOEngine::Events::EventBus::Get().Dispatch(NANOEngine::Events::EventDomain::Editor, DeleteEntityEvent(EditorScene::s_selectedEntity->linkedEntity));
        //            EditorScene::s_selectedEntity = nullptr;
        //        }
        //        if (ImGui::MenuItem("Rename")) {
        //            // TODO: Add rename logic (inline or popup)
        //        }
        //        ImGui::EndPopup();
        //    }

        //    // === Dummy children or details ===
        //    if (hasChildren && opened) {
        //        // child nodes
        //        ImGui::TreePop();
        //    }
        //}
        static bool s_built = false;
        if (!s_built) { Editor::EditorScene::BuildFlatHierarchy(); s_built = true; }

        // Local drag state
        static uint32_t draggingId = NE::ECS::NO_ENTITY;
        //static uint32_t dragParent = 0;
        static int      previewInsert = -1;
        static bool     hadDragThisFrame = false;

        // A small DFS that only draws one level (roots) for now; parenting later
        auto& roots = Editor::EditorScene::ChildrenOf(0);

        std::vector<ImRect> rowRects;
        rowRects.reserve((int)roots.size());

        hadDragThisFrame = false;
        for (int i = 0; i < (int)roots.size(); ++i) {
            uint32_t id = roots[i];

            Editor::EditorEntity* ent = nullptr;
            for (auto& e : Editor::EditorScene::s_entities) { if (e.linkedEntity == id) { ent = &e; break; } }
            std::string label = (ent ? ent->displayName : std::string("Entity")) + "##" + std::to_string(id);

            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_SpanAvailWidth |
                ImGuiTreeNodeFlags_Leaf |
                ImGuiTreeNodeFlags_NoTreePushOnOpen;

            if (Editor::EditorScene::s_selectedEntity && ent == Editor::EditorScene::s_selectedEntity)
                flags |= ImGuiTreeNodeFlags_Selected;

            // Row
            ImGui::TreeNodeEx((void*)(uintptr_t)id, flags, "%s", label.c_str());
            //ImGui::PushID((int)id); // ok even if id==0
            //ImGui::TreeNodeEx("##row", flags, "%s", label.c_str()); // label still unique via "##"
            //ImGui::PopID();

            // Selection
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                Editor::EditorScene::s_selectedEntity = ent;

            // Record this row's screen rect
            rowRects.emplace_back(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

            // Begin drag from this row
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                draggingId = id;
                ImGui::SetDragDropPayload("HIER_DRAG_ID", &draggingId, sizeof(uint32_t));
                ImGui::TextUnformatted(label.c_str());
                ImGui::EndDragDropSource();
            }

            // While dragging, compute the preview insertion gap using simple hover hit-test
            if (draggingId != NE::ECS::NO_ENTITY && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                hadDragThisFrame = true;
                const ImRect& r = rowRects.back();
                if (ImGui::IsMouseHoveringRect(r.Min, r.Max, true)) {
                    const float midY = 0.5f * (r.Min.y + r.Max.y);
                    previewInsert = (ImGui::GetIO().MousePos.y < midY) ? i : (i + 1);
                }
            }
        }

        // If dragging and not hovering any row, allow dropping before first / after last.
        if (draggingId != NE::ECS::NO_ENTITY && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !rowRects.empty()) {
            ImGuiWindow* win = ImGui::GetCurrentWindow();
            const float mouseY = ImGui::GetIO().MousePos.y;

            // Only if mouse is inside the list's inner region; tweak if your layout differs
            const float top = win->InnerRect.Min.y;
            const float bot = win->InnerRect.Max.y;

            if (mouseY < rowRects.front().Min.y && mouseY >= top) {
                previewInsert = 0;
            } else if (mouseY > rowRects.back().Max.y && mouseY <= bot) {
                previewInsert = (int)rowRects.size();
            }
        }

        // Draw the insertion line preview
        if (draggingId != NE::ECS::NO_ENTITY && previewInsert >= 0 && !rowRects.empty()) {
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // X span (match your rows; adjust if you have icons/indent)
            float x1 = rowRects.front().Min.x;
            float x2 = rowRects.front().Max.x;

            // Y position at the chosen gap
            float y = 0.0f;
            if (previewInsert == 0) {
                y = rowRects.front().Min.y;
            } else if (previewInsert >= (int)rowRects.size()) {
                y = rowRects.back().Max.y;
            } else {
                y = rowRects[previewInsert].Min.y;
            }

            // Line + small end caps
            dl->AddLine(ImVec2(x1, y), ImVec2(x2, y), IM_COL32(255, 255, 0, 200), 2.0f);
            dl->AddLine(ImVec2(x1, y - 3), ImVec2(x1, y + 3), IM_COL32(255, 255, 0, 200), 2.0f);
            dl->AddLine(ImVec2(x2, y - 3), ImVec2(x2, y + 3), IM_COL32(255, 255, 0, 200), 2.0f);

            // Gentle auto-scroll while dragging near edges
            ImGuiWindow* win = ImGui::GetCurrentWindow();
            const float innerTop = win->InnerRect.Min.y;
            const float innerBot = win->InnerRect.Max.y;
            const float mouseY = ImGui::GetIO().MousePos.y;
            const float margin = 18.0f;  // scroll zone
            const float speed = 12.0f;  // px/frame

            if (mouseY < innerTop + margin) {
                ImGui::SetScrollY(ImGui::GetScrollY() - speed);
            } else if (mouseY > innerBot - margin) {
                ImGui::SetScrollY(ImGui::GetScrollY() + speed);
            }
        }

        // Commit the reorder ONCE when the mouse is released
        if (draggingId != NE::ECS::NO_ENTITY && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (previewInsert >= 0) {
                // parent = 0 for roots; when you add parenting, pass the real parent
                Editor::EditorScene::ReorderWithinSiblings(/*parent*/0u, /*child*/draggingId, /*insertIndex*/previewInsert);
            }
            // reset drag state
            draggingId = NE::ECS::NO_ENTITY;
            previewInsert = -1;
        }

        // If a drag started but this frame didn't detect any drag (e.g., payload canceled), clear preview next frame
        if (!hadDragThisFrame && ImGui::IsMouseDragging(ImGuiMouseButton_Left) == false && draggingId == NE::ECS::NO_ENTITY) {
            previewInsert = -1;
        }

        //if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        //    draggingId = 0;
        //    dragParent = 0;
        //    previewInsert = -1;
        //}

		ImGui::End();
	}
}


// Find displayName from s_entities (flat storage already exists in your code)
// Linear scan is OK for now; you can add an id->index map later.
//auto* ent = (Editor::EditorEntity*)nullptr;
//for (auto& e : Editor::EditorScene::s_entities) { if (e.linkedEntity == id) { ent = &e; break; } } // (your s_entities) 
//std::string label = (ent ? ent->displayName : std::string("Entity")) + "##" + std::to_string(id);

//ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

//if (Editor::EditorScene::s_selectedEntity && ent == Editor::EditorScene::s_selectedEntity)
//    flags |= ImGuiTreeNodeFlags_Selected;

//// Draw the row
//ImGui::TreeNodeEx((void*)(uintptr_t)id, flags, "%s", label.c_str());

//// Selection
//if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
//    Editor::EditorScene::s_selectedEntity = ent;
//}

//// Begin drag
//if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
//    draggingId = id;
//    dragParent = 0; // roots for now (will be nodes[id].parent after parenting)
//    ImGui::SetDragDropPayload("HIER_ROWINDEX_ID", &draggingId, sizeof(uint32_t));
//    ImGui::TextUnformatted(label.c_str());
//    ImGui::EndDragDropSource();
//}

//// Drop target: compute whether we are dropping "above" or "below"
//// and translate to an insert index.
//if (ImGui::BeginDragDropTarget()) {
//    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("HIER_ROWINDEX_ID")) {
//        uint32_t srcId = *(const uint32_t*)p->Data;
//        // hit test using item rect
//        ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
//        float midY = 0.5f * (r.Min.y + r.Max.y);
//        const bool dropAbove = (ImGui::GetIO().MousePos.y < midY);
//        // where would it land if we release here?
//        previewInsert = dropAbove ? i : i + 1;
//        // Commit immediately on accept? Prefer committing on mouse release:
//        // We'll commit when mouse is released and payload is delivered, which is now.
//        Editor::EditorScene::ReorderWithinSiblings(/*parent*/0, srcId, previewInsert);
//    }
//    ImGui::EndDragDropTarget();
//}

// Optional: draw a thin separator line at previewInsert for nicer UX