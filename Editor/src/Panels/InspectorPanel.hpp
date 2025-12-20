#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>
#include <memory>
#include <Graphics/Core/Material.hpp>
#include "../AssetManagement/Interfaces/MaterialEditor.hpp"

namespace Editor {
	class InspectorPanel : public IPanel {
	public:
		InspectorPanel();

		void OnImGuiRender() override;
	private:

		void RenderTextureImportSettings(std::string metaPath);

		void RenderModelImportSettings(const std::string& metaPath);

		std::unique_ptr<MaterialEditor> m_materialEditor;
		std::string m_lastPath;

	};
}
