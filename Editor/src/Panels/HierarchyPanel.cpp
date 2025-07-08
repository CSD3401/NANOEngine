#include "HierarchyPanel.hpp"
#include <imgui/imgui.h>
#include "../Command/CommandHistory.hpp"
#include "../Command/EditorCommands.hpp"
#include "../EditorScene.hpp"
#include "src/Events/EventBus.hpp"
#include "../EditorEvents.hpp"
#include <src/ECS/Core/Entity.hpp>

namespace Editor {
	HierarchyPanel::HierarchyPanel() {
        EditorScene::s_entities.reserve(NANOEngine::ECS::MAX_ENTITIES);
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
            if (ImGui::MenuItem("Create Entity")) {
                NANOEngine::Events::EventBus::Get().Dispatch(NANOEngine::Events::EventDomain::Editor, CreateEntityEvent{});
            }
            if (ImGui::MenuItem("Create Camera")) {
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
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                EditorScene::s_selectedEntity = &entity;
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
