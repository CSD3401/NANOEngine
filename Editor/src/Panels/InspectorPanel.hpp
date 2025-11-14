#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>
#include <memory>
#include <Graphics/Core/Material.hpp>
#include "../../MaterialEditor.hpp"

namespace Editor {
	class InspectorPanel : public IPanel {
	public:
		InspectorPanel();

		void OnImGuiRender() override;

	private:

		void RenderTextureImportSettings();
		void RenderMaterialSettings();

		// temp implementation
		//std::shared_ptr<NE::Graphics::Material> m_loadedMaterial;
		//std::string m_loadedPath;
		std::unique_ptr<MaterialEditor> m_materialEditor;
		std::string m_lastPath;
	};
}
