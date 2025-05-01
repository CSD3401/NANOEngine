#include "GamePanel.hpp"
#include <imgui/imgui.h>

namespace Editor {
	GamePanel::GamePanel() {
	}

	void GamePanel::OnImGuiRender()
	{
		ImGui::Begin("Game", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImGui::End();
	}
}
