#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>
#include <memory>
#include <Graphics/Core/Material.hpp>

namespace Editor {
	class InspectorPanel : public IPanel {
	public:
		InspectorPanel();

		void OnImGuiRender() override;

	private:
		std::shared_ptr<NE::Graphics::Material> m_loadedMaterial;
		std::string m_loadedPath;
	};
}
