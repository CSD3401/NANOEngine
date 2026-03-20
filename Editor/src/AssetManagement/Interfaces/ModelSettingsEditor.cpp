#include "pch.h"
#include "ModelSettingsEditor.hpp"

#include <algorithm>
#include <imgui/imgui.h>

#include "../AssetManager.hpp"
#include "../../EditorUI.hpp"

namespace Editor {
	bool ModelSettingsEditor::LoadModelSettings(std::string filePath, std::string uuid) {
		m_path = filePath;

		auto* record = Assets::AssetManager::GetInstance().GetRecord(uuid);

		if (!record) return false;
		if (!record->asset) return false;

		m_model = dynamic_cast<Assets::ModelAsset*>(record->asset.get());
		if (!m_model) return false;

		if (!m_model->LoadImportSettings(m_path)) return false;

		return true;
	}

	void ModelSettingsEditor::RenderSettings() {
		auto& settings = m_model->GetImportSettings();

		static int s_CurrentImportTab = 0;

		const char* tabNames[] = { "Model", "Rig", "Animation", "Materials" };
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
			bool isActive = (s_CurrentImportTab == i);

			if (isActive)
				ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
			else
				ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);

			if (ImGui::Button(tabNames[i]))
				s_CurrentImportTab = i;

			ImGui::PopStyleColor();

			if (i + 1 < tabCount)
				ImGui::SameLine();
		}

		ImGui::Separator();

		auto DrawComboEnum = [](const char* label, int& currentIndex, const char* const* names, int count) {
			ImGui::Combo(label, &currentIndex, names, count);
			};

		switch (s_CurrentImportTab) {
		case 0:
		{
			if (ImGui::CollapsingHeader("Scene##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
				// SceneImportSettings
				ImGui::DragFloat("Scale Factor", &settings.scene.scaleFactor, 0.01f, 0.0001f, 100.0f);
				Editor::DrawCheckbox("Convert Units", settings.scene.convertUnits);
				Editor::DrawCheckbox("Import Blend Shapes", settings.scene.importBlendShapes);
				Editor::DrawCheckbox("Import Cameras", settings.scene.importCameras);
				Editor::DrawCheckbox("Import Lights", settings.scene.importLights);
				Editor::DrawCheckbox("Preserve Hierarchy", settings.scene.preserveHierarchy);
			}

			if (ImGui::CollapsingHeader("Mesh##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
				// MeshImportSettings
				const char* MeshOptNames[] = { "None", "Everything", "Polygon Order", "Vertex Order" };
				int meshOptIndex = static_cast<int>(settings.mesh.meshOptimizationMode);
				DrawComboEnum("Mesh Optimization", meshOptIndex, MeshOptNames, IM_ARRAYSIZE(MeshOptNames));
				settings.mesh.meshOptimizationMode =
					static_cast<Assets::MeshImportSettings::MeshOptimizationMode>(meshOptIndex);

				Editor::DrawCheckbox("Generate Colliders", settings.mesh.generateColliders);
				Editor::DrawCheckbox("Generate Mesh LODs", settings.mesh.generateMeshLODs);

				Editor::DrawCheckbox("Keep Quads", settings.mesh.keepQuads);
				Editor::DrawCheckbox("Weld Vertices", settings.mesh.weldVertices);

				const char* IndexFormatNames[] = { "Auto", "UInt16", "UInt32" };
				int indexFmtIndex = static_cast<int>(settings.mesh.indexFormat);
				DrawComboEnum("Index Format", indexFmtIndex, IndexFormatNames, IM_ARRAYSIZE(IndexFormatNames));
				settings.mesh.indexFormat = static_cast<Assets::MeshImportSettings::IndexFormat>(indexFmtIndex);

				const char* NormalModeNames[] = { "Import", "Calculate", "None" };
				int normalIndex = static_cast<int>(settings.mesh.normalMode);
				DrawComboEnum("Normals", normalIndex, NormalModeNames, IM_ARRAYSIZE(NormalModeNames));
				settings.mesh.normalMode = static_cast<Assets::MeshImportSettings::NormalMode>(normalIndex);

				ImGui::DragFloat("Smoothing Angle", &settings.mesh.smoothingAngle, 1.0f, 0.0f, 180.0f);

				const char* TangentModeNames[] = { "Import", "Calculate (MikkTSpace)", "None" };
				int tangentIndex = static_cast<int>(settings.mesh.tangentMode);
				DrawComboEnum("Tangents", tangentIndex, TangentModeNames, IM_ARRAYSIZE(TangentModeNames));
				settings.mesh.tangentMode = static_cast<Assets::MeshImportSettings::TangentMode>(tangentIndex);

				Editor::DrawCheckbox("Swap UVs", settings.mesh.swapUVs);
				Editor::DrawCheckbox("Generate Lightmap UVs (UV1)", settings.mesh.generateLightmapUVs);
				if (settings.mesh.generateLightmapUVs) {
					int lightmapUvPaddingTexels = static_cast<int>(settings.mesh.lightmapUvPaddingTexels);
					ImGui::DragInt("Lightmap UV Padding", &lightmapUvPaddingTexels, 1.0f, 4, 64, "%d px");
					settings.mesh.lightmapUvPaddingTexels = static_cast<uint32_t>(std::clamp(lightmapUvPaddingTexels, 4, 64));
				}
			}
			break;
		}

		case 1: // ----- RIG -----
		{
			if (ImGui::CollapsingHeader("Rig##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
				const char* AnimTypeNames[] = { "None", "Generic", "Humanoid" };
				int animTypeIndex = static_cast<int>(settings.rig.animationType);
				DrawComboEnum("Animation Type", animTypeIndex, AnimTypeNames, IM_ARRAYSIZE(AnimTypeNames));
				settings.rig.animationType = static_cast<Assets::RigImportSettings::AnimationType>(animTypeIndex);

				Editor::DrawCheckbox("Strip Unused Bones", settings.rig.stripBones);
			}
			break;
		}

		case 2: // ----- ANIMATION -----
		{
			if (ImGui::CollapsingHeader("Animation##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
				Editor::DrawCheckbox("Import Animations", settings.animation.importAnimations);
				Editor::DrawCheckbox("Import Constraints", settings.animation.importConstraints);
				Editor::DrawCheckbox("Import Animated Custom Properties", settings.animation.importAnimatedCustomProperties);
				Editor::DrawCheckbox("Auto Split Clips", settings.animation.autoSplitClips);

				ImGui::DragFloat("Sample Rate", &settings.animation.sampleRate, 1.0f, 0.0f, 480.0f, "%.1f");

				Editor::DrawCheckbox("Import Root Motion", settings.animation.importRootMotion);
				Editor::DrawCheckbox("Lock Root Position XZ", settings.animation.lockRootPositionXZ);
				Editor::DrawCheckbox("Lock Root Rotation Y", settings.animation.lockRootRotationY);
			}
			break;
		}

		case 3: // ----- MATERIALS -----
		{
			if (ImGui::CollapsingHeader("Materials##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
				Editor::DrawCheckbox("Import Materials", settings.material.importMaterials);
				Editor::DrawCheckbox("Try Reuse Existing Materials", settings.material.tryReuseExistingMaterials);

				const char* MaterialModeNames[] = { "Per Submesh", "Per Mesh", "Per File" };
				int matModeIndex = static_cast<int>(settings.material.creationMode);
				DrawComboEnum("Material Creation", matModeIndex, MaterialModeNames, IM_ARRAYSIZE(MaterialModeNames));
				settings.material.creationMode =
					static_cast<Assets::MaterialImportSettings::MaterialCreationMode>(matModeIndex);
			}
			break;
		}
		}

		ImGui::Spacing();
		ImGui::Separator();

		if (ImGui::Button("Apply")) {
			Save();
		}
	}

	void ModelSettingsEditor::Save() {
		m_model->SaveImportSettings(m_path);
		Assets::AssetManager::GetInstance().ReimportAsset(m_path);
	}
}
