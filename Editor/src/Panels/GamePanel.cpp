#include "GamePanel.hpp"
#include <imgui/imgui.h>

namespace Editor {

	GamePanel::GamePanel(uint32_t framebuffer) : m_framebuffer(framebuffer)
	{
	}

	void GamePanel::OnImGuiRender()
	{
		ImGui::Begin("Game", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImVec2 panelPos = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

		ImGui::Image((ImTextureID)(uintptr_t)m_framebuffer, panelSize, ImVec2(0, 1), ImVec2(1, 0));

		ImGui::End();
	}
}
