#include "HierarchyPanel.hpp"
#include <imgui/imgui.h>

namespace Editor {
	HierarchyPanel::HierarchyPanel() {
	}

	void HierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Hierarchy", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImVec2 panelPos = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

		ImGui::End();
	}
}
