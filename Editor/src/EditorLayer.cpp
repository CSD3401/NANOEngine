#include "EditorLayer.hpp"
#include "imgui/imgui.h"
#include <imgui/imgui_internal.h>
#include <wtypes.h>
#include "Panels/AssetBrowserPanel.hpp"
#include "Engine.hpp"
#include "EditorScene.hpp"
#include <glfw/glfw3.h>
#include "Command/CommandHistory.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/AnimationPanel.hpp"
#include "Panels/AnimationRuntimePanel.hpp"
#include "Panels/AnimationGraphPanel.hpp"
#include "Panels/ProfilerPanel.hpp"
#include "Panels/LightingPanel.hpp"
#include <Core/SpdLogger.hpp>  // For SPD_INFO, SPD_DEBUG logging
#include "EngineState.hpp"  // Add this include

namespace Editor {
	void EditorLayer::OnImGuiRender() {
		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistent = true;
		bool opt_fullscreen = opt_fullscreen_persistent;

		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		if (opt_fullscreen) {
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			window_flags |= ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoNavFocus;
		}

		ImGui::Begin("Editor DockSpace", &dockspaceOpen, window_flags);

		float titleBarHeight = 0.0f;
		DrawCustomTitleBar("NANOEngine", titleBarHeight, IM_COL32(25, 25, 25, 255));

		// Calculate dockspace size to leave room for status bar at bottom
		ImVec2 dockspaceSize = ImGui::GetContentRegionAvail();
		dockspaceSize.y -= 25.0f; // Reserve 25px for status bar

		// SINGLE DockSpace call
		ImGuiID dockspace_id = ImGui::GetID("EditorDockspace");
		ImGui::DockSpace(dockspace_id, dockspaceSize, dockspace_flags);

		// === SHORTCUTS (Must be at window level to work) ===
		// Save shortcut (Ctrl+S) - Check globally, not just when window focused
		if ((ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyAlt)) {
			if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
				if (NE::IsSceneDirty()) {
					SPD_INFO("[DirtyFlag] Ctrl+S pressed - Saving scene");
					NE::SaveCurrentScene(EditorScene::currentScenePath);
				} else {
					SPD_DEBUG("[DirtyFlag] Ctrl+S pressed - No changes to save");
				}
			} else if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
				CommandHistory::GetInstance().Undo();
			} else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
				CommandHistory::GetInstance().Redo();
			}

			if (!ImGui::IsAnyItemActive() &&
				!ImGui::IsAnyItemFocused()) {
				bool canEditHierarchy = EditorScene::selectedPrefab.empty();
				if (canEditHierarchy && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
					EditorScene::DuplicateSelected();
				} else if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
					EditorScene::CopySelected();
				} else if (canEditHierarchy && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
					EditorScene::PasteSelected();
				}
			}
		}

		for (auto& panel : m_panels) {
			panel->OnImGuiRender();
		}

		// === STATUS BAR (Integrated at bottom of main window) ===
		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

		if (ImGui::BeginChild("##StatusBar", ImVec2(0, 25.0f), false, ImGuiWindowFlags_NoScrollbar)) {
			ImGui::SetCursorPosY(4.0f); // Center text vertically

			// Scene path on the left
			ImGui::Text("  %s", EditorScene::currentScenePath.c_str());

			// Dirty indicator on the right
			ImGui::SameLine();
			float rightOffset = ImGui::GetWindowWidth() - 120.0f;
			if (rightOffset > ImGui::GetCursorPosX()) {
				ImGui::SetCursorPosX(rightOffset);
			}

			bool isSceneDirty = NE::IsSceneDirty();
			if (isSceneDirty) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f)); // Orange
				ImGui::Text("* UNSAVED");
				ImGui::PopStyleColor();
			} else {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 0.3f, 1.0f)); // Green
				ImGui::Text("Saved");
				ImGui::PopStyleColor();
			}
		}
		ImGui::EndChild();

		ImGui::PopStyleColor();

		ImGui::End();  // End main dockspace window
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

		// Background
		drawList->AddRectFilled(winPos, ImVec2(winPos.x + menuBarSize.x, winPos.y + menuBarSize.y), bgColor);

		// Begin menu bar region
		ImGui::SetCursorScreenPos(winPos);
		ImGui::BeginChild("##CustomMenuBar", menuBarSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		// --- Logo (Left) ---
		ImGui::SetCursorPos(ImVec2(0.f, 2.f));
		ImGui::Image(icon, ImVec2(18.f, 18.f));
		ImGui::SameLine();

		// --- Menus (Next to logo) ---
		float logoSpace = 20.f;
		ImGui::SetCursorPosX(logoSpace);

		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, style.ItemSpacing.y));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 2.f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.10f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.20f));

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

		if (openFile)   ImGui::OpenPopup("FileMenu");
		if (openEdit)   ImGui::OpenPopup("EditMenu");
		if (openWindow) ImGui::OpenPopup("WindowMenu");

		// Seamless popup styling
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);

		// FILE MENU
		ImGui::SetNextWindowPos(ImVec2(fileMin.x, fileMax.y), ImGuiCond_Appearing);
		if (ImGui::BeginPopup("FileMenu")) {
			if (ImGui::MenuItem("New Scene")) {}
			if (ImGui::MenuItem("Open Scene")) {}
			ImGui::Separator();

			// FIXED: Single Save menu item with proper dirty check
			bool isSceneDirty = NE::IsSceneDirty();
			if (isSceneDirty) {
				if (ImGui::MenuItem("Save", "Ctrl+S")) {
					SPD_INFO("[DirtyFlag] Save menu clicked - Saving scene");
					NE::SaveCurrentScene(EditorScene::currentScenePath);
				}
			} else {
				ImGui::BeginDisabled();
				ImGui::MenuItem("Save", "Ctrl+S");
				ImGui::EndDisabled();
			}

			if (ImGui::MenuItem("Save As...")) {
				NE::SaveCurrentScene(EditorScene::currentScenePath);
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) {
				// Safety check: If playing, prompt to stop first
				if (NE::GetEngineState() == NE::EngineState::Play) {
					ImGui::OpenPopup("ExitWhilePlayingModal");
				} else {
					PostQuitMessage(0);
				}
			}
			ImGui::EndPopup();
		}

		// EDIT MENU
		ImGui::SetNextWindowPos(ImVec2(editMin.x, editMax.y), ImGuiCond_Appearing);
		if (ImGui::BeginPopup("EditMenu")) {
			if (ImGui::MenuItem("Undo", "Ctrl+Z")) CommandHistory::GetInstance().Undo();
			if (ImGui::MenuItem("Redo", "Ctrl+Y")) CommandHistory::GetInstance().Redo();
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
			if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
			if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
			ImGui::EndPopup();
		}

		// WINDOW MENU
		ImGui::SetNextWindowPos(ImVec2(winMin.x, winMax.y), ImGuiCond_Appearing);
		if (ImGui::BeginPopup("WindowMenu")) {
			if (ImGui::BeginMenu("General")) {
				if (ImGui::MenuItem("Scene", nullptr, false)) {}
				if (ImGui::MenuItem("Game", nullptr, false)) {}
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
			if (ImGui::BeginMenu("Rendering")) {
				if (ImGui::MenuItem("Lighting", nullptr, false)) {
					AddPanel<LightingPanel>();
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

		// === Exit While Playing Modal Dialog ===
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("ExitWhilePlayingModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Warning!");
			ImGui::Separator();
			ImGui::Text("The scene is currently playing.");
			ImGui::Text("You must stop play mode before exiting.");
			ImGui::Spacing();

			if (ImGui::Button("Stop and Exit", ImVec2(120, 0))) {
				NE::EditorEdit();  // Stop the scene
				ImGui::CloseCurrentPopup();
				PostQuitMessage(0);  // Then exit
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// --- History button (top-right), before window controls ---
		float buttonSize = 20.0f;
		float spacing = 0.0f;
		float totalButtonsWidth = 3 * (buttonSize + spacing);

		ImVec2 historySize = ImVec2(140.0f, 20.0f);

		float rightEdge = menuBarSize.x - totalButtonsWidth - 8.0f;
		ImVec2 pos = ImVec2(rightEdge - historySize.x, (menuBarSize.y - historySize.y) * 0.5f);
		ImGui::SetCursorPos(pos);

		auto& history = CommandHistory::GetInstance();
		const auto& undo = history.GetUndoList();
		const char* preview = undo.empty() ? "History" : undo.back()->GetName();

		ImGui::SetNextItemWidth(historySize.x);
		ImGuiComboFlags comboFlags = ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLarge;

		if (ImGui::BeginCombo("##history_dropdown", preview, comboFlags)) {
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

		// Close - with safety check for playing state
		if (ImGui::InvisibleButton("##close", ImVec2(buttonSize, buttonSize))) {
			// Safety check: If playing, prompt to stop first
			if (NE::GetEngineState() == NE::EngineState::Play) {
				ImGui::OpenPopup("ExitWhilePlayingModal");
			} else {
				PostQuitMessage(0);
			}
		}

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

				POINT cursor;
				GetCursorPos(&cursor);

				int restoreWidth = placement.rcNormalPosition.right - placement.rcNormalPosition.left;

				ShowWindow(hwnd, SW_RESTORE);

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