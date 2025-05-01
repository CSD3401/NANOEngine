#include "InspectorPanel.hpp"
#include <imgui/imgui.h>

namespace Editor {
	InspectorPanel::InspectorPanel() {
	}

	void InspectorPanel::OnImGuiRender()
	{
		ImGui::Begin("Inspector", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImVec2 panelPos = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

		ImGui::End();
	}
}
