#include "pch.h"
#include "LightingPanel.hpp"
#include <imgui/imgui.h>
#include "Engine.hpp"
#include <Graphics/Core/RenderSettings.hpp>
#include <Graphics/Core/PostProcessingSettings.hpp>
#include <Graphics/Core/GraphicsManager.hpp>
#include <EditorInterface/RendererExports.hpp>
#include "../EditorUI.hpp"
#include "../Lighting/DirectLightmapBaker.hpp"
#include "../Lighting/LightmapAtlasAllocator.hpp"
#include "../Lighting/SceneBakeBVH.hpp"
#include <algorithm>
#include <glad/glad.h>


namespace Editor {
	LightingPanel::~LightingPanel() {
		ReleasePreviewTextures();
		Lightmapping::ShutdownDirectLightBakeSession();
	}

	void LightingPanel::ReleasePreviewTextures() {
		for (auto& [_, textures] : m_previewTextures) {
			if (textures.lightingTexture != 0) {
				glDeleteTextures(1, &textures.lightingTexture);
				textures.lightingTexture = 0;
			}
			if (textures.validityTexture != 0) {
				glDeleteTextures(1, &textures.validityTexture);
				textures.validityTexture = 0;
			}
			if (textures.ownerTexture != 0) {
				glDeleteTextures(1, &textures.ownerTexture);
				textures.ownerTexture = 0;
			}
		}
		m_previewTextures.clear();
	}

	void LightingPanel::SyncPreviewTextures(const Editor::Lightmapping::DirectLightBakeSessionState& sessionState) {
		if (!sessionState.hasResult || !sessionState.result) {
			if (m_cachedBakeRevision != 0) {
				ReleasePreviewTextures();
				m_cachedBakeRevision = 0;
			}
			return;
		}

		if (m_cachedBakeRevision == sessionState.result->revision) {
			return;
		}

		ReleasePreviewTextures();
		m_cachedBakeRevision = sessionState.result->revision;

		auto uploadTexture = [](const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height) -> unsigned int {
			if (rgba.empty() || width == 0 || height == 0) {
				return 0;
			}

			unsigned int texture = 0;
			glGenTextures(1, &texture);
			if (texture == 0) {
				return 0;
			}

			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
			return texture;
		};

		const auto rasterResult = sessionState.result->rasterResult;
		for (const auto& page : sessionState.result->pages) {
			PreviewTextureSet textures{};
			textures.lightingTexture =
				uploadTexture(page.preview.lightingRgba8, page.preview.width, page.preview.height);
			if (rasterResult) {
				const auto rasterPageIt = std::find_if(
					rasterResult->pageBuffers.begin(),
					rasterResult->pageBuffers.end(),
					[&](const auto& rasterPage) { return rasterPage.pageId == page.pageId; });
				if (rasterPageIt != rasterResult->pageBuffers.end()) {
					textures.validityTexture =
						uploadTexture(rasterPageIt->preview.validityRgba8, rasterPageIt->preview.width, rasterPageIt->preview.height);
					textures.ownerTexture =
						uploadTexture(rasterPageIt->preview.ownerRgba8, rasterPageIt->preview.width, rasterPageIt->preview.height);
				}
			}
			if (textures.validityTexture == 0) {
				textures.validityTexture =
					uploadTexture(page.preview.validityRgba8, page.preview.width, page.preview.height);
			}
			m_previewTextures[page.pageId] = textures;
		}
	}

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
				Editor::Lightmapping::UpdateDirectLightBakeSession();
				auto& previewState = Editor::Lightmapping::GetLightmapAllocationPreviewState();
				const auto directBakeState = Editor::Lightmapping::GetDirectLightBakeSessionState();
				SyncPreviewTextures(directBakeState);

				//ImGui::TextWrapped("Atlas allocation is editor-only in Part 3. This pass computes deterministic page placement and UV transforms, then writes them into per-entity LightmapBinding data.");
				//ImGui::Spacing();

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

				//ImGui::TextWrapped("Bake BVH is editor-only build-time infrastructure for direct-light baking queries. It gathers static shadow-casting scene geometry, flattens triangles into world space, and builds a CPU BVH for any-hit shadow tests plus closest-hit debug queries.");
				//ImGui::Spacing();

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

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::TextWrapped("Direct light baking rasterizes UV1 triangles into atlas texels, reconstructs world-space samples from barycentrics, evaluates direct Lambert lighting for supported lights, and uses the bake BVH for any-hit shadow visibility.");
				ImGui::Spacing();

				ImGui::Text("Worker Count");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);
				ImGui::DragInt("##DirectBakeWorkerCount", &m_directBakeWorkerCount, 1.0f, 0, 64);
				m_directBakeWorkerCount = std::clamp(m_directBakeWorkerCount, 0, 64);

				ImGui::Text("Ray Origin Bias");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);
				ImGui::DragFloat("##DirectBakeRayBias", &m_directBakeRayBias, 1e-4f, 1e-5f, 1e-1f, "%.5f", ImGuiSliderFlags_Logarithmic);
				m_directBakeRayBias = std::clamp(m_directBakeRayBias, 1e-5f, 1e-1f);

				ImGui::Text("Ray Min Distance");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);
				ImGui::DragFloat("##DirectBakeRayMinDistance", &m_directBakeRayMinDistance, 1e-5f, 1e-6f, 1e-2f, "%.6f", ImGuiSliderFlags_Logarithmic);
				m_directBakeRayMinDistance = std::clamp(m_directBakeRayMinDistance, 1e-6f, 1e-2f);

				ImGui::Text("Finite-Light Epsilon");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);
				ImGui::DragFloat("##DirectBakeFiniteLightEpsilon", &m_directBakeFiniteLightEpsilon, 1e-4f, 1e-5f, 1e-1f, "%.5f", ImGuiSliderFlags_Logarithmic);
				m_directBakeFiniteLightEpsilon = std::clamp(m_directBakeFiniteLightEpsilon, 1e-5f, 1e-1f);

				ImGui::Text("Preview Exposure");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);
				ImGui::DragFloat("##DirectBakePreviewExposure", &m_directBakePreviewExposure, 0.05f, 0.1f, 16.0f, "%.2f");
				m_directBakePreviewExposure = std::clamp(m_directBakePreviewExposure, 0.1f, 16.0f);

				Editor::Lightmapping::DirectLightBakeSettings bakeSettings{};
				bakeSettings.workerCount = static_cast<uint32_t>(m_directBakeWorkerCount);
				bakeSettings.rebuildBvhBeforeBake = true;
				bakeSettings.generateDebugBuffers = true;
				bakeSettings.rayOriginBias = m_directBakeRayBias;
				bakeSettings.rayMinDistance = m_directBakeRayMinDistance;
				bakeSettings.finiteLightDistanceEpsilon = m_directBakeFiniteLightEpsilon;
				bakeSettings.previewExposure = m_directBakePreviewExposure;

				if (ImGui::Button("Start Direct Bake")) {
					Editor::Lightmapping::StartSceneDirectLightBake(bakeSettings);
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel Direct Bake")) {
					Editor::Lightmapping::CancelSceneDirectLightBake();
				}
				ImGui::SameLine();
				if (ImGui::Button("Run Raster Self-Check")) {
					m_rasterSelfCheckPassed = Editor::Lightmapping::RunLightmapUvRasterizerSelfCheck(m_rasterSelfCheckMessage);
				}

				if (!m_rasterSelfCheckMessage.empty()) {
					if (m_rasterSelfCheckPassed) {
						ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "%s", m_rasterSelfCheckMessage.c_str());
					} else {
						ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s", m_rasterSelfCheckMessage.c_str());
					}
				}

				if (!directBakeState.statusMessage.empty()) {
					ImGui::TextWrapped("%s", directBakeState.statusMessage.c_str());
				}

				if (directBakeState.isRunning) {
					ImGui::ProgressBar(directBakeState.progress01, ImVec2(-1.0f, 0.0f));
					ImGui::Text("Active Stage");
					ImGui::SameLine();
					ImGui::TextDisabled("%s", directBakeState.activeStage.c_str());
				}

				if (directBakeState.hasResult) {
					const auto& bakeResult = *directBakeState.result;
					const auto& stats = bakeResult.stats;

					ImGui::Spacing();
					ImGui::Text("Bake Instances");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.bakeInstanceCount);

					ImGui::Text("Supported Lights");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.supportedLightCount);

					ImGui::Text("Raster Triangles");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.rasterTriangleCount);

					ImGui::Text("Degenerate UV Tris");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.rasterDegenerateUvTriangleCount);

					ImGui::Text("Covered Texels");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.coveredTexelCount);

					ImGui::Text("Uncovered Texels");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.rasterUncoveredTexelCount);

					ImGui::Text("Ownership Conflicts");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.rasterOwnershipConflictCount);

					ImGui::Text("Invalid Barycentrics");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.rasterInvalidBarycentricTexelCount);

					ImGui::Text("Invalid Samples");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.rasterInvalidSampleTexelCount);

					ImGui::Text("Clamped Triangles");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.rasterInnerRectClampedTriangleCount);

					ImGui::Text("Skipped Texels");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.skippedTexelCount);

					ImGui::Text("Rays Cast");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.raysCast);

					ImGui::Text("Visible Rays");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.visibleRayCount);

					ImGui::Text("Occluded Rays");
					ImGui::SameLine();
					ImGui::TextDisabled("%zu", stats.occludedRayCount);

					ImGui::Text("Setup Time");
					ImGui::SameLine();
					ImGui::TextDisabled("%.3f ms", stats.setupMs);

					ImGui::Text("Raster Time");
					ImGui::SameLine();
					ImGui::TextDisabled("%.3f ms", stats.rasterizationMs);

					ImGui::Text("Evaluation Time");
					ImGui::SameLine();
					ImGui::TextDisabled("%.3f ms", stats.evaluationMs);

					if (bakeResult.rasterResult && ImGui::CollapsingHeader("Raster Coverage", ImGuiTreeNodeFlags_DefaultOpen)) {
						for (const auto& page : bakeResult.rasterResult->pageBuffers) {
							const float coverage =
								page.allocatedInnerTexelCount > 0
								? static_cast<float>(page.validTexelCount) / static_cast<float>(page.allocatedInnerTexelCount)
								: 0.0f;
							ImGui::BulletText(
								"%s: %zu / %zu texels (%.1f%%)",
								page.pageId.c_str(),
								page.validTexelCount,
								page.allocatedInnerTexelCount,
								coverage * 100.0f);
						}
					}

					if (ImGui::CollapsingHeader("Direct Bake Warnings", ImGuiTreeNodeFlags_DefaultOpen)) {
						if (bakeResult.warningCounts.empty()) {
							ImGui::TextDisabled("No bake warnings recorded.");
						} else {
							for (const auto& [warning, count] : bakeResult.warningCounts) {
								ImGui::BulletText("%s x%zu", warning.c_str(), count);
							}
						}
					}

					if (ImGui::CollapsingHeader("Page Previews", ImGuiTreeNodeFlags_DefaultOpen)) {
						for (const auto& page : bakeResult.pages) {
							const float coverage =
								page.width > 0 && page.height > 0
								? static_cast<float>(page.validTexelCount) / static_cast<float>(page.width * page.height)
								: 0.0f;
							ImGui::Text("%s", page.pageId.c_str());
							ImGui::SameLine();
							ImGui::TextDisabled("%ux%u", page.width, page.height);
							ImGui::ProgressBar(std::clamp(coverage, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));

							const auto previewIt = m_previewTextures.find(page.pageId);
							if (previewIt != m_previewTextures.end()) {
								const auto& textures = previewIt->second;
								if (textures.lightingTexture != 0) {
									ImGui::Image((ImTextureID)(intptr_t)textures.lightingTexture, ImVec2(192.0f, 192.0f));
									if (textures.validityTexture != 0) {
										ImGui::SameLine();
										ImGui::Image((ImTextureID)(intptr_t)textures.validityTexture, ImVec2(192.0f, 192.0f));
									}
									if (textures.ownerTexture != 0) {
										ImGui::SameLine();
										ImGui::Image((ImTextureID)(intptr_t)textures.ownerTexture, ImVec2(192.0f, 192.0f));
									}
								}
							}
						}
					}
				}
			} break;
			}

		}
		ImGui::End();
	}
}
