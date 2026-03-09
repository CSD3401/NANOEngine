#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>

namespace Editor {
	class LightingPanel : public IPanel {
	public:
		LightingPanel() = default;

		virtual void OnImGuiRender() override;

	private:
		float m_texelsPerUnit = 16.0f;
		int m_bvhLeafSize = 8;
		float m_bvhTraversalEpsilon = 1e-5f;
	};
}
