#include "EditorLayer.hpp"
#include "imgui/imgui.h"
#include <imgui/imgui_internal.h>
#include <wtypes.h>
#include "Panels/AssetBrowserPanel.hpp"
#include "Engine.hpp"
#include "../src/EditorScene.hpp"
#include <glfw/glfw3.h>
#include "Command/CommandHistory.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/AnimationPanel.hpp"
#include "Panels/AnimationRuntimePanel.hpp"
#include "Panels/AnimationGraphPanel.hpp"
#include "Panels/ProfilerPanel.hpp"

namespace Editor {
	void EditorLayer::OnImGuiRender() {
        static bool dockspaceOpen = true;
        static bool opt_fullscreen_persistent = true;
        bool opt_fullscreen = opt_fullscreen_persistent;
        //static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        
        // Honestly have no clue what are some of these flags do but documentation says need so :/
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar;

        if (opt_fullscreen) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        ImGui::Begin("Editor DockSpace", &dockspaceOpen, window_flags);

        float titleBarHeight = 0.0f;
        DrawCustomTitleBar("NANOEngine", titleBarHeight, IM_COL32(25, 25, 25, 255));

        //const ImGuiViewport* viewport = ImGui::GetMainViewport();
        //float menuBarHeight = ImGui::GetFrameHeight();
        //float totalTopBarHeight = titleBarHeight + menuBarHeight;

        //ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + totalTopBarHeight));
        //ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - totalTopBarHeight));

        ImGuiID dockspace_id = ImGui::GetID("EditorDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        for (auto& panel : m_panels) {
            panel->OnImGuiRender();
        }

        ImGui::End();
	}

    void EditorLayer::SetIcon(unsigned int _iconID) {
        icon = reinterpret_cast<ImTextureID>(reinterpret_cast<void*>((uintptr_t)_iconID));
    }

    void EditorLayer::DrawCustomTitleBar(const char* title, float height, ImU32 bgColor) {
        height; title;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        float barHeight = 20.f;
        ImVec2 menuBarSize = ImVec2(ImGui::GetWindowWidth(), barHeight);

        HWND hwnd = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
        hwnd;
        // Background
        drawList->AddRectFilled(winPos, ImVec2(winPos.x + menuBarSize.x, winPos.y + menuBarSize.y), bgColor);

        // Begin menu bar region
        ImGui::SetCursorScreenPos(winPos);
        ImGui::BeginChild("##CustomMenuBar", menuBarSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // --- Logo (Left) ---
        ImGui::SetCursorPos(ImVec2(0.f, 2.f));
        ImGui::Image(icon, ImVec2(18.f,18.f));
        ImGui::SameLine();

        // --- Menus (Next to logo) ---
        float logoSpace = 20.f; // logo width + padding
        ImGui::SetCursorPosX(logoSpace);

        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, style.ItemSpacing.y));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 2.f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));             // idle: fully transparent
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.10f));  // hover: light wash
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.20f));   // active: slightly stronger

        bool openFile = ImGui::Button("File");
        ImVec2 fileMin = ImGui::GetItemRectMin();
        ImVec2 fileMax = ImGui::GetItemRectMax();

        ImGui::SameLine();
        bool openEdit = ImGui::Button("Edit");
        ImVec2 editMin = ImGui::GetItemRectMin();
        ImVec2 editMax = ImGui::GetItemRectMax();

        ImGui::SameLine();
        bool openWindow = ImGui::Button("Window");
        ImVec2 winMin = ImGui::GetItemRectMin();
        ImVec2 winMax = ImGui::GetItemRectMax();

        // Open the popups when clicked (don’t set pos here)
        if (openFile)   ImGui::OpenPopup("FileMenu");
        if (openEdit)   ImGui::OpenPopup("EditMenu");
        if (openWindow) ImGui::OpenPopup("WindowMenu");

        // Seamless popup styling
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);

        // FILE – set pos right before BeginPopup for THIS popup
        ImGui::SetNextWindowPos(ImVec2(fileMin.x, fileMax.y), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("FileMenu")) {
            if (ImGui::MenuItem("New Scene")) {}
            if (ImGui::MenuItem("Open Scene")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {}
            ImGui::EndPopup();
        }

        // EDIT
        ImGui::SetNextWindowPos(ImVec2(editMin.x, editMax.y), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("EditMenu")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) CommandHistory::GetInstance().Undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) CommandHistory::GetInstance().Redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")){}
            if (ImGui::MenuItem("Copy", "Ctrl+C")){}
            if (ImGui::MenuItem("Paste", "Ctrl+V")){}
            ImGui::EndPopup();
        }

        // WINDOW
        ImGui::SetNextWindowPos(ImVec2(winMin.x, winMax.y), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("WindowMenu")) {


            if (ImGui::BeginMenu("General")) {
                if (ImGui::MenuItem("Scene", nullptr, false)) {

                }
                if (ImGui::MenuItem("Game", nullptr, false)) {

                }
                if (ImGui::MenuItem("Inspector", nullptr, false)) {
                    AddPanel<InspectorPanel>();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Analysis")) {
                if (ImGui::MenuItem("Profiler", nullptr, false)) {
                    AddPanel<ProfilerPanel>();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Animation")) {
                if (ImGui::MenuItem("Animation", nullptr, false)) {
                    AddPanel<AnimationPanel>();
                }
                if (ImGui::MenuItem("Animator", nullptr, false)) {
                    AddPanel<AnimatorGraphPanel>();
                }
                if (ImGui::MenuItem("Animation Runtime", nullptr, false)) {
                    AddPanel<AnimatorRuntimePanel>();
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(3);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);

        // TEST SHORTCUTS
        //if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z)) {
        //    CommandHistory::GetInstance().Undo();
        //}

        //if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y)) {
        //    CommandHistory::GetInstance().Redo();
        //}

        // --- History button (top-right), before the window controls ---
        // Reserve width for the three window buttons you already place on the far right
        float buttonSize = 20.0f;
        float spacing = 0.0f;
        float totalButtonsWidth = 3 * (buttonSize + spacing);

        ImVec2 historySize = ImVec2(140.0f, 20.0f);

        float rightEdge = menuBarSize.x - totalButtonsWidth - 8.0f; // 8px padding
        ImVec2 pos = ImVec2(rightEdge - historySize.x, (menuBarSize.y - historySize.y) * 0.5f);
        ImGui::SetCursorPos(pos);

        // show the latest undo label as preview text
        auto& history = CommandHistory::GetInstance();
        const auto& undo = history.GetUndoList();
        const char* preview =
            undo.empty() ? "History" : undo.back()->GetName();

        // Style it to feel like a button (no arrow, we draw "▾" ourselves in the label)
        ImGui::SetNextItemWidth(historySize.x);
        ImGuiComboFlags comboFlags = ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLarge;

        if (ImGui::BeginCombo("##history_dropdown", preview, comboFlags)) {
            // Optional header
            ImGui::TextUnformatted("Undo Stack");
            ImGui::Separator();

            if (undo.empty()) {
                ImGui::TextDisabled("  <empty>");
            } else {
                for (int i = static_cast<int>(undo.size()) - 1; i >= 0; --i)
                    ImGui::BulletText("#%d - %s", i + 1, undo[i]->GetName());
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Redo Stack");
            ImGui::Separator();

            const auto& redo = history.GetRedoList();
            if (redo.empty()) {
                ImGui::TextDisabled("  <empty>");
            } else {
                for (int i = static_cast<int>(redo.size()) - 1; i >= 0; --i)
                    ImGui::BulletText("#%d - %s", i + 1, redo[i]->GetName());
            }

            ImGui::Spacing();
            if (ImGui::Button("Undo")) history.Undo();
            ImGui::SameLine();
            if (ImGui::Button("Redo")) history.Redo();

            ImGui::EndCombo();
        }

        // --- Window Controls (Far right) ---
        //float buttonSize = 32.0f; // Slightly smaller for style
        //float spacing = 0.0f;
        //float totalButtonsWidth = 3 * (buttonSize + spacing);

        // Move cursor to the far right, but centered in the bar
        ImGui::SetCursorPos(ImVec2(menuBarSize.x - totalButtonsWidth, 0.f));

        ImVec2 btnPos = ImGui::GetCursorScreenPos();

        // Minimize
        if (ImGui::InvisibleButton("##minimize", ImVec2(buttonSize, buttonSize)))
            ShowWindow(hwnd, SW_MINIMIZE);

        if (ImGui::IsItemHovered()) {
            drawList->AddRectFilled(btnPos, ImVec2(btnPos.x + buttonSize, btnPos.y + buttonSize), IM_COL32(200, 50, 50, 255), 2.0f);
            drawList->AddText(ImVec2(btnPos.x + 6.5f, btnPos.y + 3.5f), IM_COL32(200, 200, 200, 255), "-");
        } else {
            drawList->AddText(ImVec2(btnPos.x + 6.5f, btnPos.y + 3.5f), IM_COL32(200, 200, 200, 255), "-");
        }

        ImGui::SameLine(0, spacing);
        btnPos.x += buttonSize + spacing;

        // Maximize/Restore
        WINDOWPLACEMENT placement = { sizeof(WINDOWPLACEMENT) };
        GetWindowPlacement(hwnd, &placement);
        bool isMaximized = (placement.showCmd == SW_MAXIMIZE);

        if (ImGui::InvisibleButton("##maximize", ImVec2(buttonSize, buttonSize)))
            ShowWindow(hwnd, isMaximized ? SW_RESTORE : SW_MAXIMIZE);

        if (ImGui::IsItemHovered()) {
            drawList->AddRectFilled(btnPos, ImVec2(btnPos.x + buttonSize, btnPos.y + buttonSize), IM_COL32(200, 50, 50, 255), 2.0f);
            drawList->AddText(ImVec2(btnPos.x + 6.5f, btnPos.y + 3.5f), IM_COL32(200, 200, 200, 255), "O");
        } else {
            drawList->AddText(ImVec2(btnPos.x + 6.5f, btnPos.y + 3.5f), IM_COL32(200, 200, 200, 255), "O");
        }

        ImGui::SameLine(0, spacing);
        btnPos.x += buttonSize + spacing;

        // Close
        if (ImGui::InvisibleButton("##close", ImVec2(buttonSize, buttonSize)))
            PostQuitMessage(0);

        if (ImGui::IsItemHovered()) {
            drawList->AddRectFilled(btnPos, ImVec2(btnPos.x + buttonSize, btnPos.y + buttonSize), IM_COL32(200, 50, 50, 255), 2.0f);
            drawList->AddText(ImVec2(btnPos.x + 6.5f, btnPos.y + 3.5f), IM_COL32(200, 200, 200, 255), "X");
        } else {
            drawList->AddText(ImVec2(btnPos.x + 6.5f, btnPos.y + 3.5f), IM_COL32(200, 60, 60, 255), "X");
        }

		// For dragging of OS window
        ImGui::SetCursorScreenPos(winPos);
        ImGui::InvisibleButton("##drag_window", menuBarSize);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            static bool restoringFromMaximized = false;

            if (isMaximized && !restoringFromMaximized) {
                restoringFromMaximized = true;

                // Save cursor offset from top-left
                POINT cursor;
                GetCursorPos(&cursor);

                int restoreWidth = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
                //int restoreHeight = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;

                ShowWindow(hwnd, SW_RESTORE);

                // Adjust window position so cursor remains at same relative offset
                SetWindowPos(hwnd, nullptr,
                    cursor.x - restoreWidth / 2,
                    cursor.y - 8, 0, 0,
                    SWP_NOSIZE | SWP_NOZORDER);
            } else {
                restoringFromMaximized = false;

                ImVec2 dragDelta = ImGui::GetMouseDragDelta();
                RECT rect;
                GetWindowRect(hwnd, &rect);

                SetWindowPos(hwnd, nullptr,
                    rect.left + static_cast<int>(dragDelta.x),
                    rect.top + static_cast<int>(dragDelta.y),
                    0, 0, SWP_NOSIZE | SWP_NOZORDER);

                ImGui::ResetMouseDragDelta();
            }
        }

        ImGui::EndChild();
    }

}
