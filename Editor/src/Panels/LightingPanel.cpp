#include "pch.h"
#include "LightingPanel.hpp"
#include <imgui/imgui.h>
#include "Engine.hpp"
#include <Graphics/Core/RenderSettings.hpp>
#include <Graphics/Core/PostProcessingSettings.hpp>
#include <Graphics/Core/GraphicsManager.hpp>
#include <EditorInterface/RendererExports.hpp>
#include "../EditorUI.hpp"
#include "../Lighting/LightmapAtlasAllocator.hpp"
#include "../Lighting/SceneBakeBVH.hpp"
#include <algorithm>


namespace Editor {
	void LightingPanel::OnImGuiRender() {
		if (ImGui::Begin("Lighting", nullptr)) {

			auto& bvhState = Editor::Lightmapping::GetSceneBakeBVHSessionState();
			if (bvhState.debugOptions.drawRootBounds || bvhState.debugOptions.drawLeafBounds) {
				Editor::Lightmapping::DrawSceneBakeBVHDebug();
			}

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

				ImGui::PushID("BloomSettings");
				if (ImGui::CollapsingHeader("Bloom##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::TextUnformatted("Bloom Tint");
					ImGui::SameLine();
					ImGui::ColorEdit3("##BloomTint", postProcessingSettings.bloomSettings.tint.Data());
					Editor::ToolTip("(Default: [1.0, 1.0, 1.0]) Tints the bloom contribution. Keep near white for natural glow; use subtle color for stylized lighting.");
					Editor::DrawFloatField("Threshold", postProcessingSettings.bloomSettings.brightThreshold, 0.05f, true);
					Editor::ToolTip("(Default: 1.0) Minimum brightness before pixels contribute to bloom. Lower = more bloom everywhere; higher = only very bright highlights.");
					Editor::DrawFloatField("Prefilter Gain", postProcessingSettings.bloomSettings.brightScale, 0.01f, true);
					Editor::ToolTip("(Default: 1.0) Scales the extracted highlights before blur. Use to boost bloom without lowering the threshold.");
					Editor::DrawFloatField("Soft Knee", postProcessingSettings.bloomSettings.softKnee, 0.01f, true);
					Editor::ToolTip("(Default: 0.5) Softens the threshold transition. 0 = hard cutoff, higher values reduce popping and flicker.");
					Editor::DrawFloatField("Radius", postProcessingSettings.bloomSettings.bloomRadius, 0.05f, true);
					Editor::ToolTip("(Default: 1.0) Controls bloom spread/blur radius across the mip chain. Higher = wider, softer glow (more expensive).");
					Editor::DrawFloatField("Intensity", postProcessingSettings.bloomSettings.bloomIntensity, 0.01f, true);
					Editor::ToolTip("(Default: 0.06) Final bloom strength added back into the scene after blur.");
				}
				ImGui::PopID();

				const char* toneMapTypeItems[] = { "Reinhard", "ReinhardExtended", "ACESApproximation", "FilmicACES", "ACESFitted" };
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

				ImGui::PushID("SSAO");
				if (ImGui::CollapsingHeader("Ambient Occlusion##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
					Editor::DrawCheckbox("Enabled", postProcessingSettings.ssaoSettings.enabled);
					Editor::DrawFloatField("Radius", postProcessingSettings.ssaoSettings.radius, 0.1f, true);
					Editor::DrawFloatField("Bias", postProcessingSettings.ssaoSettings.bias, 0.1f, true);
					Editor::DrawFloatField("Intensity", postProcessingSettings.ssaoSettings.intensity, 0.1f, true);
					Editor::DrawFloatField("Power", postProcessingSettings.ssaoSettings.power, 0.1f, true);
				}
				ImGui::PopID();

				ImGui::PushID("SSR");
				if (ImGui::CollapsingHeader("Screen Space Reflections##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
					Editor::DrawCheckbox("Enabled", postProcessingSettings.ssrSettings.enabled);
					Editor::DrawFloatField("Intensity", postProcessingSettings.ssrSettings.intensity, 0.05f, true);
					Editor::DrawFloatField("Max Distance", postProcessingSettings.ssrSettings.maxDistance, 0.5f, true);
					Editor::DrawFloatField("Thickness", postProcessingSettings.ssrSettings.thickness, 0.01f, true);
					Editor::DrawFloatField("Stride", postProcessingSettings.ssrSettings.stride, 0.01f, true);
					Editor::DrawFloatField("Roughness Cutoff", postProcessingSettings.ssrSettings.roughnessCutoff, 0.01f, true);
					Editor::DrawFloatField("Fresnel Power", postProcessingSettings.ssrSettings.fresnelPower, 0.1f, true);
					Editor::DrawFloatField("Edge Fade", postProcessingSettings.ssrSettings.edgeFade, 0.01f, true);
					Editor::DrawFloatField("Half Res Scale", postProcessingSettings.ssrSettings.halfResScale, 0.01f, true);
					ImGui::SliderInt("Tap Counts", &postProcessingSettings.ssrSettings.resolveTapCount, 0, 12);
					ImGui::SliderInt("Max Steps", &postProcessingSettings.ssrSettings.maxSteps, 8, 128);
					ImGui::SliderInt("Binary Search Steps", &postProcessingSettings.ssrSettings.binarySearchSteps, 0, 10);
					postProcessingSettings.ssrSettings.thickness = std::max(0.0f, postProcessingSettings.ssrSettings.thickness);
				}
				ImGui::PopID();

				ImGui::PushID("Vignette");
				if (ImGui::CollapsingHeader("Vignette##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
					Editor::DrawCheckbox("Enabled", postProcessingSettings.vignetteSettings.enabled);
					Editor::DrawFloatField("Intensity", postProcessingSettings.vignetteSettings.intensity, 0.1f, true);
					Editor::DrawFloatField("Radius", postProcessingSettings.vignetteSettings.radius, 0.1f, true);
					Editor::DrawFloatField("Softness", postProcessingSettings.vignetteSettings.softness, 0.1f, true);
					Editor::DrawVec3Control("Tint", postProcessingSettings.vignetteSettings.tint, 0.1f, true);
					Editor::DrawFloatField("Tint Intensity", postProcessingSettings.vignetteSettings.tintIntensity, 0.1f, true);

					// disable this temporarily until we can figure out a good way to do HDR colour picking without the ImGui HDR widget (which is very buggy and doesn't work well with our HDR colour implementation)
					//HDRColor vignetteTintHDR = {
					//	ImVec4(postProcessingSettings.vignetteSettings.tint.x,
					//			postProcessingSettings.vignetteSettings.tint.y,
					//			postProcessingSettings.vignetteSettings.tint.z,
					//			1.0f),
					//	postProcessingSettings.vignetteSettings.tintIntensity
					//};

					//if (DrawHDRColorField("Ambient Colour", vignetteTintHDR)) {
					//	postProcessingSettings.vignetteSettings.tint.x = vignetteTintHDR.color.x;
					//	postProcessingSettings.vignetteSettings.tint.y = vignetteTintHDR.color.y;
					//	postProcessingSettings.vignetteSettings.tint.z = vignetteTintHDR.color.z;
					//	postProcessingSettings.vignetteSettings.tintIntensity = vignetteTintHDR.intensity;
					//}
				}
				ImGui::PopID();

			} break;
			case 3: {
				auto& previewState = Editor::Lightmapping::GetLightmapAllocationPreviewState();

				ImGui::TextWrapped("Atlas allocation is editor-only in Part 3. This pass computes deterministic page placement and UV transforms, then writes them into per-entity LightmapBinding data.");
				ImGui::Spacing();

				ImGui::Text("Texels Per Unit");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);
				ImGui::DragFloat("##TexelsPerUnit", &m_texelsPerUnit, 0.25f, 0.25f, 256.0f, "%.2f");
				m_texelsPerUnit = std::max(0.25f, m_texelsPerUnit);

				ImGui::Text("Page Size");
				ImGui::SameLine();
				ImGui::TextDisabled("%d x %d", Editor::Lightmapping::kDefaultLightmapPageSize, Editor::Lightmapping::kDefaultLightmapPageSize);

				ImGui::Text("Padding");
				ImGui::SameLine();
				ImGui::TextDisabled("%d px", Editor::Lightmapping::kDefaultLightmapPadding);

				if (ImGui::Button("Run Allocation")) {
					Editor::Lightmapping::LightmapAllocationSettings settings{};
					settings.texelsPerUnit = m_texelsPerUnit;
					Editor::Lightmapping::RunSceneLightmapAllocation(settings);
				}

				ImGui::Spacing();
				ImGui::Separator();

				if (!previewState.hasRun) {
					ImGui::TextDisabled("Run allocation to generate atlas pages, UV transforms, and skip diagnostics.");
				} else {
					const auto& report = previewState.report;
					ImGui::Text("Eligible Entities");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", report.eligibleEntityCount);

					ImGui::Text("Allocated Entities");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", report.allocatedEntityCount);

					ImGui::Text("Skipped Entities");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", report.skippedEntityCount);

					ImGui::Text("Opted Out");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", report.optedOutEntityCount);

					ImGui::Text("Total Pages");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", report.totalPages);

					ImGui::Spacing();
					if (ImGui::CollapsingHeader("Per-Page Occupancy", ImGuiTreeNodeFlags_DefaultOpen)) {
						if (previewState.pages.empty()) {
							ImGui::TextDisabled("No atlas pages were created.");
						}

						for (const auto& page : previewState.pages) {
							const float occupancy =
								page.width > 0 && page.height > 0
								? static_cast<float>(page.usedArea) / static_cast<float>(page.width * page.height)
								: 0.0f;
							ImGui::Text("%s", page.pageId.c_str());
							ImGui::SameLine();
							ImGui::TextDisabled("%d placements", static_cast<int>(page.placements.size()));
							ImGui::ProgressBar(std::clamp(occupancy, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
						}
					}

					if (ImGui::CollapsingHeader("Allocation Warnings", ImGuiTreeNodeFlags_DefaultOpen)) {
						if (report.failureCounts.empty()) {
							ImGui::TextDisabled("No warnings. All considered entities allocated cleanly.");
						} else {
							for (const auto& [reason, count] : report.failureCounts) {
								ImGui::BulletText("%s: %d", reason.c_str(), count);
							}
						}
					}

					if (ImGui::CollapsingHeader("Example Results", ImGuiTreeNodeFlags_DefaultOpen)) {
						int shown = 0;
						for (const auto& entry : report.entries) {
							if (entry.status == Editor::Lightmapping::LightmapEntityStatusKind::Allocated) {
								continue;
							}

							ImGui::BulletText(
								"%s [%s] %s",
								entry.entityName.c_str(),
								Editor::Lightmapping::ToString(entry.status),
								entry.message.c_str());
							if (++shown >= 12) {
								break;
							}
						}

						if (shown == 0) {
							ImGui::TextDisabled("No skipped or unresolved entities to report.");
						}
					}
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::TextWrapped("Bake BVH is editor-only build-time infrastructure for direct-light baking queries. It gathers static shadow-casting scene geometry, flattens triangles into world space, and builds a CPU BVH for any-hit shadow tests plus closest-hit debug queries.");
				ImGui::Spacing();

				ImGui::Text("Leaf Size");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);
				ImGui::DragInt("##BvhLeafSize", &m_bvhLeafSize, 1.0f, 1, 32);
				m_bvhLeafSize = std::clamp(m_bvhLeafSize, 1, 32);

				ImGui::Text("Traversal Epsilon");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);
				ImGui::DragFloat("##BvhTraversalEpsilon", &m_bvhTraversalEpsilon, 1e-5f, 1e-7f, 1e-2f, "%.6f", ImGuiSliderFlags_Logarithmic);
				m_bvhTraversalEpsilon = std::clamp(m_bvhTraversalEpsilon, 1e-7f, 1e-2f);

				Editor::Lightmapping::SceneBakeBVHBuildSettings bvhSettings{};
				bvhSettings.maxLeafPrimitives = m_bvhLeafSize;
				bvhSettings.traversalEpsilon = m_bvhTraversalEpsilon;

				if (ImGui::Button("Build BVH")) {
					Editor::Lightmapping::BuildSceneBakeBVHFromCurrentScene(bvhSettings);
				}
				ImGui::SameLine();
				if (ImGui::Button("Clear BVH")) {
					Editor::Lightmapping::ClearSceneBakeBVH();
				}
				ImGui::SameLine();
				if (ImGui::Button("Run BVH Self-Check")) {
					std::string message;
					bvhState.selfCheckPassed = Editor::Lightmapping::RunSceneBakeBVHSelfCheck(message);
					bvhState.selfCheckMessage = message;
				}

				if (!bvhState.selfCheckMessage.empty()) {
					if (bvhState.selfCheckPassed) {
						ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "%s", bvhState.selfCheckMessage.c_str());
					} else {
						ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", bvhState.selfCheckMessage.c_str());
					}
				}

				if (!bvhState.statusMessage.empty()) {
					ImGui::TextWrapped("%s", bvhState.statusMessage.c_str());
				}

				if (bvhState.hasValidBVH) {
					ImGui::Spacing();
					ImGui::Text("Input Triangles");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", bvhState.stats.inputTriangleCount);

					ImGui::Text("BVH Triangles");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", bvhState.stats.triangleCount);

					ImGui::Text("Skipped Invalid Triangles");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", bvhState.stats.skippedInvalidTriangleCount);

					ImGui::Text("Nodes");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", bvhState.stats.nodeCount);

					ImGui::Text("Leaves");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", bvhState.stats.leafCount);

					ImGui::Text("Max Depth");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", bvhState.stats.maxDepth);

					ImGui::Text("Avg Prims / Leaf");
					ImGui::SameLine();
					ImGui::TextDisabled("%.2f", bvhState.stats.avgPrimitivesPerLeaf);

					ImGui::Text("Build Time");
					ImGui::SameLine();
					ImGui::TextDisabled("%.3f ms", bvhState.stats.buildMs);

					ImGui::Text("Included Renderers");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", bvhState.geometryStats.includedEntityCount);

					ImGui::Text("Skipped Renderers");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", bvhState.geometryStats.skippedEntityCount);

					ImGui::Text("Collector Skipped Triangles");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", bvhState.geometryStats.skippedTriangleCount);
				}

				if (ImGui::CollapsingHeader("BVH Debug Draw", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::BeginDisabled(!bvhState.hasValidBVH);
					ImGui::Checkbox("Draw Root Bounds", &bvhState.debugOptions.drawRootBounds);
					ImGui::Checkbox("Draw Leaf Bounds", &bvhState.debugOptions.drawLeafBounds);
					ImGui::SliderInt("Leaf Depth", &bvhState.debugOptions.leafDepth, 0, std::max(static_cast<int>(bvhState.stats.maxDepth), 0));
					Editor::ToolTip("Depth 0 shows the deepest BVH boxes. Higher values collapse the view upward toward the root, so fewer bounds are drawn.");
					ImGui::EndDisabled();

					if (!bvhState.debugStatusMessage.empty()) {
						ImGui::TextWrapped("%s", bvhState.debugStatusMessage.c_str());
					}
				}

				if (ImGui::CollapsingHeader("BVH Warnings", ImGuiTreeNodeFlags_DefaultOpen)) {
					if (bvhState.warningCounts.empty()) {
						ImGui::TextDisabled("No scene collection warnings.");
					} else {
						for (const auto& [reason, count] : bvhState.warningCounts) {
							ImGui::BulletText("%s: %zu", reason.c_str(), count);
						}
					}
				}

				if (ImGui::CollapsingHeader("BVH Warning Examples", ImGuiTreeNodeFlags_DefaultOpen)) {
					if (bvhState.warnings.empty()) {
						ImGui::TextDisabled("No warning examples available.");
					} else {
						const int maxExamples = 12;
						for (int i = 0; i < static_cast<int>(bvhState.warnings.size()) && i < maxExamples; ++i) {
							const auto& warning = bvhState.warnings[static_cast<size_t>(i)];
							ImGui::BulletText(
								"%s [%s] %s",
								warning.entityName.c_str(),
								Editor::Lightmapping::ToString(warning.reason),
								warning.message.c_str());
						}
					}
				}
			} break;
			}

		}
		ImGui::End();
	}
}
