#include "HistoryPanel.hpp"
#include <imgui/imgui.h>

namespace Editor {
	void HistoryPanel::OnImGuiRender() {
        ImGui::Begin("Command History", nullptr);

        auto& history = CommandHistory::GetInstance();

        // Undo List
        ImGui::Text("Undo Stack:");
        const auto& undo = history.GetUndoList();
        if (undo.empty()) {
            ImGui::TextDisabled("  <empty>");
        } else {
            for (int i = static_cast<int>(undo.size()) - 1; i >= 0; --i) {
                ImGui::BulletText("#%d - %s", i + 1, undo[i]->GetName());
            }
        }

        // Redo List
        ImGui::Separator();
        ImGui::Text("Redo Stack:");
        const auto& redo = history.GetRedoList();
        if (redo.empty()) {
            ImGui::TextDisabled("  <empty>");
        } else {
            for (int i = static_cast<int>(redo.size()) - 1; i >= 0; --i) {
                ImGui::BulletText("#%d - %s", i + 1, redo[i]->GetName());
            }
        }

        if (ImGui::Button("Undo")) history.Undo();
        ImGui::SameLine();
        if (ImGui::Button("Redo")) history.Redo();

        ImGui::End();
	}
}
