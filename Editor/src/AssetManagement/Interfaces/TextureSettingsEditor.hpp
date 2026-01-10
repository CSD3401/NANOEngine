#pragma once
#include <string>
#include "../Assets/TextureAsset.hpp"

namespace Editor {
	class TextureSettingsEditor {
	public:
		bool LoadTextureSettings(std::string filePath, std::string uuid);
		void RenderSettings();
		void Save();

	private:
		Assets::TextureAsset* m_texture;
		std::string m_path;
	};
}