#include "GamePanel.hpp"
#include <imgui/imgui.h>
#include "Engine.hpp"
#include <EditorInterface/ECSExports.hpp>

namespace Editor {

	void GamePanel::OnImGuiRender()
	{
		ImGui::Begin("Game", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImVec2 panelPos = ImGui::GetCursorScreenPos();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();

		// Convert panel position from screen coordinates to GLFW window coordinates
		// GLFW mouse coordinates are relative to the window (0,0 at top-left)
		// ImGui screen coordinates may include window position on multi-monitor setups
		ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		ImVec2 mainViewportPos = mainViewport->Pos;
		float panelPosX = panelPos.x - mainViewportPos.x;
		float panelPosY = panelPos.y - mainViewportPos.y;

		// Set viewport bounds for UI interaction system
		NE::ECS::Command::SetUIViewportBounds(
			panelPosX, panelPosY,
			panelSize.x, panelSize.y,
			static_cast<float>(NE::GetUIScreenWidth()),
			static_cast<float>(NE::GetUIScreenHeight())
		);

		ImGui::Image(
			(ImTextureID)(uintptr_t)NE::GetGameColorAttachment(),
			panelSize,
			ImVec2(0, 1),
			ImVec2(1, 0)
		);

		ImGui::End();
	}
}
