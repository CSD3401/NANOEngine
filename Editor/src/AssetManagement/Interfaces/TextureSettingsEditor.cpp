#include "TextureSettingsEditor.hpp"

#include <imgui/imgui.h>

#include "../AssetManager.hpp"
#include "../../EditorUI.hpp"

namespace Editor {
	bool TextureSettingsEditor::LoadTextureSettings(std::string filePath, std::string uuid) {
		m_path = filePath;

		auto* record = Assets::AssetManager::GetInstance().GetRecord(uuid);

		if (!record) return false;
		if (!record->asset) return false;

		m_texture = dynamic_cast<Assets::TextureAsset*>(record->asset.get());
		if (!m_texture) return false;

		if (!m_texture->LoadImportSettings(m_path)) return false;

		return true;
	}

	void TextureSettingsEditor::RenderSettings() {
		auto& settings = m_texture->GetImportSettings(m_path);
		static bool s_dirty = false;

		static const char* TextureTypeNames[] = { "Default", "Normal Map", "Sprite" };
		static const char* TextureShapeNames[] = { "2D", "Cube", "2D Array" };
		static const char* TextureWrapMode[] = { "Repeat", "Clamp", "Mirror", "MirrorOnce", "PerAxis" };
		static const char* TextureFilterMode[] = { "Point", "Bilinear", "Trilinear" };
		static const char* AlphaSourceNames[] = { "InputTextureAlpha", "GrayscaleSource", "None" };

		// ----- Texture Type -----
		int currentType = static_cast<int>(settings.type); // assuming enum starts at 0
		if (ImGui::Combo("Texture Type", &currentType, TextureTypeNames, IM_ARRAYSIZE(TextureTypeNames))) {
			settings.type = static_cast<Assets::TexType>(currentType);
			s_dirty = true;
		}

		// ----- Shape -----
		int currentShape = static_cast<int>(settings.shape); // TextureShape enum
		if (ImGui::Combo("Texture Shape", &currentShape, TextureShapeNames, IM_ARRAYSIZE(TextureShapeNames))) {
			settings.shape = static_cast<Assets::TexShape>(currentShape);
			s_dirty = true;
		}

		// ----- sRGB -----
		bool isSRGB = settings.sRGB;
		if (Editor::DrawCheckbox("sRGB (Color Texture)", isSRGB)) {
			settings.sRGB = isSRGB;
			s_dirty = true;
		}

		// ----- Alpha Source -----
		int currentAlpha = static_cast<int>(settings.alphaSource); // AlphaSource enum
		if (ImGui::Combo("Alpha Source", &currentAlpha, AlphaSourceNames, IM_ARRAYSIZE(AlphaSourceNames))) {
			settings.alphaSource = static_cast<Assets::TexAlphaSource>(currentAlpha);
			s_dirty = true;
		}

		// ----- Advanced -----
		if (ImGui::TreeNode("Advanced")) {
			bool generateMips = settings.mips.generateMipmap;
			bool preserveCoverage = settings.mips.preserveCoverage;

			if (Editor::DrawCheckbox("Generate Mipmaps", generateMips)) {
				settings.mips.generateMipmap = generateMips;
				s_dirty = true;
			}

			if (Editor::DrawCheckbox("Preserve Coverage", preserveCoverage)) {
				settings.mips.preserveCoverage = preserveCoverage;
				s_dirty = true;
			}

			ImGui::TreePop();
		}

		// ----- Filter -----
		int currentFilter = static_cast<int>(settings.filterMode); // FilterMode enum
		if (ImGui::Combo("Filter Mode", &currentFilter, TextureFilterMode, IM_ARRAYSIZE(TextureFilterMode))) {
			settings.filterMode = static_cast<Assets::TexFilterMode>(currentFilter);
			s_dirty = true;
		}

		// ----- Wrap -----
		int currentWrap = static_cast<int>(settings.wrapMode); // WrapMode enum
		if (ImGui::Combo("Wrap Mode", &currentWrap, TextureWrapMode, IM_ARRAYSIZE(TextureWrapMode))) {
			settings.wrapMode = static_cast<Assets::TexWrapMode>(currentWrap);
			s_dirty = true;
		}

		ImGui::BeginDisabled(!s_dirty);
		if (ImGui::Button("Apply")) {
			Save();
			s_dirty = false;
		}
		ImGui::EndDisabled();
	}

	void TextureSettingsEditor::Save() {
		m_texture->SaveImportSettings(m_path);
		Assets::AssetManager::GetInstance().ReimportAsset(m_path);
	}
}