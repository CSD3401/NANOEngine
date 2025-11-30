#include "LightingPanel.hpp"
#include <imgui/imgui.h>
#include "Engine.hpp"
#include <Graphics/Core/RenderSettings.hpp>
#include <Graphics/Core/PostProcessingSettings.hpp>
#include <EditorInterface/RendererExports.hpp>
#include "../EditorUI.hpp"


namespace Editor {

    // Call this every frame where you want the panel:

	void LightingPanel::OnImGuiRender() {
		ImGui::Begin("Lighting", nullptr,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		NE::Graphics::RenderSettings& renderSettings = NE::Renderer::Command::GetRenderSettings();

		HDRColor ambientHdr = {
			ImVec4(renderSettings.ambientColour.x, 
				renderSettings.ambientColour.y, 
				renderSettings.ambientColour.z, 
				1.0f), 
				renderSettings.ambientIntensity
		};
		
		static int s_currentSettingsTab = 0;

		const char* tabNames[] = { "Scene", "Environment", "Post Processing", "Baked Lightmaps" };
		constexpr int tabCount = IM_ARRAYSIZE(tabNames);

		ImGuiStyle& style = ImGui::GetStyle();
		float fullWidth = ImGui::GetContentRegionAvail().x;

		float totalButtonsWidth = 0.0f;
		for (int i = 0; i < tabCount; ++i) {
			ImVec2 textSize = ImGui::CalcTextSize(tabNames[i]);
			float btnWidth = textSize.x + style.FramePadding.x * 2.0f;
			totalButtonsWidth += btnWidth;
			if (i + 1 < tabCount)
				totalButtonsWidth += style.ItemInnerSpacing.x;
		}

		float cursorX = (fullWidth - totalButtonsWidth) * 0.5f;
		if (cursorX < 0.0f) cursorX = 0.0f;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);

		for (int i = 0; i < tabCount; ++i) {
			bool isActive = (s_currentSettingsTab == i);

			if (isActive)
				ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
			else
				ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);

			if (ImGui::Button(tabNames[i]))
				s_currentSettingsTab = i;

			ImGui::PopStyleColor();

			if (i + 1 < tabCount)
				ImGui::SameLine();
		}

		ImGui::Separator();

		switch (s_currentSettingsTab) {
		case 0: {

		} break;
		case 1: {
			if (ImGui::CollapsingHeader("Environment##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Text("Skybox Material [In Development]");
				ImGui::Text("Sun Source [In Development]");
				ImGui::Spacing();

				ImGui::Text("Realtime Shadow Color [In Development]");
				ImGui::Spacing();

				ImGui::Text("Environment Lighting");
				ImGui::Indent(10.f);

				const char* envSourceItems[] = { "Skybox", "Gradient", "Color" };
				int envSource = static_cast<int>(renderSettings.envSource);

				ImGui::Text("Source");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(150.0f);
				if (ImGui::Combo("##EnvSource", &envSource, envSourceItems, IM_ARRAYSIZE(envSourceItems))) {
					renderSettings.envSource =
						static_cast<NE::Graphics::RenderSettings::EnvSource>(envSource);
				}

				if (DrawHDRColorField("Ambient Colour", ambientHdr)) {
					renderSettings.ambientColour.x = ambientHdr.color.x;
					renderSettings.ambientColour.y = ambientHdr.color.y;
					renderSettings.ambientColour.z = ambientHdr.color.z;
					renderSettings.ambientIntensity = ambientHdr.intensity;
				}
				ImGui::Unindent(10.f);
			}

			if (ImGui::CollapsingHeader("Other Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
				bool fogEnabled = renderSettings.fogEnabled;
				if (DrawCheckbox("Enable Fog", fogEnabled)) {
					renderSettings.fogEnabled = fogEnabled;
				}

				if (fogEnabled) {
					ImGui::Indent(10.0f);
					// Fog colour
					ImGui::TextUnformatted("Fog Colour");
					ImGui::SameLine();
					ImGui::ColorEdit3("##FogColour", renderSettings.fogColour.Data());

					// Fog mode
					const char* fogModeItems[] = { "Linear", "Exponential", "Exponential Squared" };
					int fogMode = static_cast<int>(renderSettings.fogMode);

					ImGui::TextUnformatted("Mode");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::Combo("##FogMode", &fogMode, fogModeItems, IM_ARRAYSIZE(fogModeItems))) {
						renderSettings.fogMode =
							static_cast<NE::Graphics::RenderSettings::FogMode>(fogMode);
					}

					switch (renderSettings.fogMode) {
					case NE::Graphics::RenderSettings::FogMode::Linear:
						DrawFloatControl("Start", renderSettings.fogStart, 0.1f);
						DrawFloatControl("End", renderSettings.fogEnd, 0.1f);
						break;
					case NE::Graphics::RenderSettings::FogMode::Exponential:
					case NE::Graphics::RenderSettings::FogMode::ExponentialSquared:
						DrawFloatControl("Density", renderSettings.fogDensity, 0.01f);
						break;
					}
					ImGui::Unindent(10.0f);
				}
			}
		} break;
		case 2: {
			NE::Graphics::PostProcessingSettings& postProcessingSettings = NE::Renderer::Command::GetPostProcessingSettings();

			if (ImGui::CollapsingHeader("Bloom##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::TextUnformatted("Bloom Tint");
				ImGui::SameLine();
				ImGui::ColorEdit3("##BloomTint", postProcessingSettings.bloomSettings.tint.Data());
				Editor::DrawFloatField("Bright Threshold", postProcessingSettings.bloomSettings.brightThreshold, 0.1f, true);
				Editor::DrawFloatField("Bright Scale", postProcessingSettings.bloomSettings.brightScale, 0.1f, true);
				Editor::DrawFloatField("Bright Soft Knee", postProcessingSettings.bloomSettings.softKnee, 0.1f, true);
				Editor::DrawFloatField("Up Sample Intensity", postProcessingSettings.bloomSettings.bloomRadius, 0.1f, true);
				Editor::DrawFloatField("Bloom Strength", postProcessingSettings.bloomSettings.bloomIntensity, 0.1f, true);
			}

			const char* toneMapTypeItems[] = { "Reinhard", "ReinhardExtended", "ACESApproximation", "FilmicACES" };
			int toneMapType = static_cast<int>(postProcessingSettings.bloomSettings.toneMapType);

			if (ImGui::CollapsingHeader("Tone-Mapping##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::TextUnformatted("Type");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(150.0f);
				if (ImGui::Combo("##ToneMapType", &toneMapType, toneMapTypeItems, IM_ARRAYSIZE(toneMapTypeItems))) {
					postProcessingSettings.bloomSettings.toneMapType =
						static_cast<NE::Graphics::BloomSettings::ToneMapType>(toneMapType);
				}

				Editor::DrawFloatField("Tonemap Exposure", postProcessingSettings.bloomSettings.exposure, 0.1f, true);
			}

			if (ImGui::CollapsingHeader("SSAO##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
				Editor::DrawCheckbox("Enabled", postProcessingSettings.ssaoSettings.enabled);
				Editor::DrawFloatField("Radius", postProcessingSettings.ssaoSettings.radius, 0.1f, true);
				Editor::DrawFloatField("Bias", postProcessingSettings.ssaoSettings.bias, 0.1f, true);
				Editor::DrawFloatField("Intensity", postProcessingSettings.ssaoSettings.intensity, 0.1f, true);
				Editor::DrawFloatField("Power", postProcessingSettings.ssaoSettings.power, 0.1f, true);
			}

		} break;
		case 3: {

		} break;
		}

		ImGui::End();
	}
}
