#include "pch.h"
#include "GamePanel.hpp"
#include <imgui/imgui.h>
#include "Engine.hpp"
#include <EditorInterface/ECSExports.hpp>
#include "../EditorState.hpp"
#include <algorithm>
#include <cstdio>

namespace Editor {
	namespace {
		struct ResolutionPreset {
			const char* label;
			uint32_t width;
			uint32_t height;
		};

		constexpr ResolutionPreset kResolutionPresets[] = {
			{ "1920 x 1080 (16:9)", 1920, 1080 },
			{ "1920 x 1200 (16:10)", 1920, 1200 },
			{ "1024 x 768 (4:3)",   1024, 768  },
			{ "1080 x 1920 (16:9)", 1080, 1920 },
			{ "2560 x 1080 (21:9)", 2560, 1080 }
		};

		ImVec2 FitRectToAspect(ImVec2 avail, float aspect) {
			if (avail.x <= 0.0f || avail.y <= 0.0f || aspect <= 0.0f) {
				return ImVec2(std::max(0.0f, avail.x), std::max(0.0f, avail.y));
			}

			ImVec2 out = avail;
			out.y = out.x / aspect;
			if (out.y > avail.y) {
				out.y = avail.y;
				out.x = out.y * aspect;
			}
			return out;
		}
	}

	void GamePanel::OnImGuiRender() {
		ImGui::Begin("Game", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
			| ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar()) {
			ImGuiStyle& style = ImGui::GetStyle();

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, style.ItemSpacing.y));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 2.f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.10f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.20f));

			const int presetCount = static_cast<int>(sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0]));
			m_resolutionPresetIndex = std::clamp(m_resolutionPresetIndex, 0, presetCount - 1);

			const uint32_t currentW = NE::GetGameViewWidth();
			const uint32_t currentH = NE::GetGameViewHeight();

			for (int i = 0; i < presetCount; ++i) {
				if (kResolutionPresets[i].width == currentW && kResolutionPresets[i].height == currentH) {
					m_resolutionPresetIndex = i;
					break;
				}
			}

			char buttonLabel[64]{};
			std::snprintf(buttonLabel, sizeof(buttonLabel), "Resolution: %u x %u", currentW, currentH);

			const bool openRes = ImGui::Button(buttonLabel);
			const ImVec2 resMin = ImGui::GetItemRectMin();
			const ImVec2 resMax = ImGui::GetItemRectMax();
			if (openRes) ImGui::OpenPopup("GameResolutionPopup");

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);

			ImGui::SetNextWindowPos(ImVec2(resMin.x, resMax.y), ImGuiCond_Appearing);
			if (ImGui::BeginPopup("GameResolutionPopup")) {
				for (int i = 0; i < presetCount; ++i) {
					const bool selected = (i == m_resolutionPresetIndex);
					if (ImGui::MenuItem(kResolutionPresets[i].label, nullptr, selected)) {
						m_resolutionPresetIndex = i;
						NE::SetGameViewResolution(kResolutionPresets[i].width, kResolutionPresets[i].height);
					}
				}
				ImGui::EndPopup();
			}

			ImGui::PopStyleVar(3);
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(2);

			ImGui::EndMenuBar();
		}

		const ImVec2 contentMin = ImGui::GetCursorScreenPos();
		const ImVec2 avail = ImGui::GetContentRegionAvail();

		const uint32_t currentW = NE::GetGameViewWidth();
		const uint32_t currentH = NE::GetGameViewHeight();
		const float targetAspect = (currentH > 0) ? (static_cast<float>(currentW) / static_cast<float>(currentH)) : (16.0f / 9.0f);

		const ImVec2 imageSize = FitRectToAspect(avail, targetAspect);
		const ImVec2 padding(
			std::max(0.0f, (avail.x - imageSize.x) * 0.5f),
			std::max(0.0f, (avail.y - imageSize.y) * 0.5f)
		);

		if (avail.x > 0.0f && avail.y > 0.0f) {
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(
				contentMin,
				ImVec2(contentMin.x + avail.x, contentMin.y + avail.y),
				IM_COL32(10, 10, 10, 255)
			);
		}

		ImVec2 cursorPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cursorPos.x + padding.x, cursorPos.y + padding.y));

		const ImVec2 imagePos = ImGui::GetCursorScreenPos();

		// Convert panel position from screen coordinates to GLFW window coordinates
		// GLFW mouse coordinates are relative to the window (0,0 at top-left)
		// ImGui screen coordinates may include window position on multi-monitor setups
		ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		ImVec2 mainViewportPos = mainViewport->Pos;
		float panelPosX = imagePos.x - mainViewportPos.x;
		float panelPosY = imagePos.y - mainViewportPos.y;

		// Only feed UI interaction bounds during play/paused — in edit mode the game panel
		// is display-only and clicks should not trigger runtime UI events.
		if (g_EditorState != EditorState::Edit) {
			NE::ECS::Command::SetUIViewportBounds(
				panelPosX, panelPosY,
				imageSize.x, imageSize.y,
				static_cast<float>(currentW),
				static_cast<float>(currentH)
			);
		} else {
			NE::ECS::Command::ClearUIViewportBounds();
		}

		ImGui::Image(
			(ImTextureID)(uintptr_t)NE::GetGameColorAttachment(),
			imageSize,
			ImVec2(0, 1),
			ImVec2(1, 0)
		);

		ImGui::End();
	}
}
