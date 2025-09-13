#include "HierarchyPanel.hpp"
#include <imgui/imgui.h>
#include "../Command/CommandHistory.hpp"
#include "../Command/EditorCommands.hpp"
#include "../EditorScene.hpp"
#include "Events/EventBus.hpp"
#include "../EditorEvents.hpp"
#include <ECS/Core/Entity.hpp>
#include <Engine.hpp>

namespace {
    inline Editor::EditorEntity* find_entity_by_id(uint32_t id) {
        auto it = Editor::EditorScene::s_idToStorageIndex.find(id);
        if (it == Editor::EditorScene::s_idToStorageIndex.end()) return nullptr;
        return &Editor::EditorScene::s_entities[it->second];
    }

    inline int index_in_display_order(uint32_t id) {
        // For simplicity, linear scan. If you want, keep a second id->orderIndex map.
        auto& order = Editor::EditorScene::s_displayOrder;
        for (int i = 0; i < (int)order.size(); ++i) if (order[i] == id) return i;
        return -1;
    }

    inline void move_index(std::vector<uint32_t>& v, int from, int to) {
        if (from == to || from < 0 || to < 0 || from >= (int)v.size() || to >(int)v.size()) return;
        auto tmp = v[from];
        if (from < to) std::move(v.begin() + from + 1, v.begin() + to, v.begin() + from);
        else           std::move_backward(v.begin() + to, v.begin() + from, v.begin() + from + 1);
        v[to] = tmp;
    }
}

namespace Editor {
	HierarchyPanel::HierarchyPanel() {
        EditorScene::s_entities.reserve(NE::ECS::MAX_ENTITIES);

        auto numEntt = NE::GetNumEntities();
        for (unsigned int i = 0; i < numEntt; ++i) {
            EditorScene::s_entities.push_back(EditorEntity{ i });
        }

        // Build display order (same as storage order initially)
        EditorScene::s_displayOrder.clear();
        EditorScene::s_displayOrder.reserve(EditorScene::s_entities.size());
        EditorScene::s_idToStorageIndex.clear();

        for (size_t idx = 0; idx < EditorScene::s_entities.size(); ++idx) {
            uint32_t id = EditorScene::s_entities[idx].linkedEntity;
            EditorScene::s_displayOrder.push_back(id);
            EditorScene::s_idToStorageIndex[id] = idx;
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
            if (ImGui::MenuItem("Delete", "Del", false, false)) {

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
        for (EditorEntity& entity : EditorScene::s_entities) {
            //std::string label = "Entity " + std::to_string(entity.linkedEntity);
            std::string name = "Entity"; // In future: get name from NameComponent
            std::string label = name + "##" + std::to_string(entity.linkedEntity);


            // Selectable, with tree node styling
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
            bool hasChildren = false;

            if (!hasChildren)
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            if (&entity == EditorScene::s_selectedEntity)
                flags |= ImGuiTreeNodeFlags_Selected;

            bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)entity.linkedEntity, flags, "%s", label.c_str());

            // === Click Selection ===
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                EditorScene::s_selectedEntity = &entity;
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                // Broadcast message
                NANOEngine::Events::EventBus::Get().Dispatch(NANOEngine::Events::EventDomain::Editor, SelectEntityEvent(EditorScene::s_selectedEntity->linkedEntity));
            }

            // === Right-click entity for context menu ===
            if (ImGui::BeginPopupContextItem((std::string("EntityContext") + std::to_string(entity.linkedEntity)).c_str())) {
                if (ImGui::MenuItem("Delete")) {
                    NANOEngine::Events::EventBus::Get().Dispatch(NANOEngine::Events::EventDomain::Editor, DeleteEntityEvent(EditorScene::s_selectedEntity->linkedEntity));
                    EditorScene::s_selectedEntity = nullptr;
                }
                if (ImGui::MenuItem("Rename")) {
                    // TODO: Add rename logic (inline or popup)
                }
                ImGui::EndPopup();
            }

            // === Dummy children or details ===
            if (hasChildren && opened) {
                // child nodes
                ImGui::TreePop();
            }
        }

		ImGui::End();
	}
}
