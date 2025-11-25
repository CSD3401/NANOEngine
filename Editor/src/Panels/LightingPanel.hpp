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
	};
}
