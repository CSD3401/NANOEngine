#pragma once
#include <string>
#include "../Assets/ModelAsset.hpp"

namespace Editor {

	class ModelSettingsEditor {
	public:
		bool LoadModelSettings(std::string filePath, std::string uuid);
		void RenderSettings();
		void Save();

	private:
		Assets::ModelAsset* m_model;
		std::string m_path;
	};

}