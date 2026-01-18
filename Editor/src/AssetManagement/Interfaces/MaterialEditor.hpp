#pragma once
#include <memory>
#include <string>
#include "Graphics/Core/Material.hpp"

namespace Editor {

	class MaterialEditor {
	public:

		bool LoadMaterial(std::string filePath, std::string uuid);
		void RefreshShader();
		void RenderSettings();
		void Save();

	private:
		std::shared_ptr<NE::Graphics::Material> m_material;
		std::string m_path;
	};

}


