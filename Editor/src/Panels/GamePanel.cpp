#include "GamePanel.hpp"
#include <imgui/imgui.h>
#include "Engine.hpp"
#include <iostream>

namespace Editor {

	void GamePanel::OnImGuiRender() {
		ImGui::Begin("Game", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImVec2 panelPos = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

		ImGui::Image(
			(ImTextureID)(uintptr_t)NE::GetGameColorAttachment(),
			panelSize, 
			ImVec2(0, 1), 
			ImVec2(1, 0)
		);

		ImGui::End();
	}
}
