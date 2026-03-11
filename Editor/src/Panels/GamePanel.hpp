#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>

namespace Editor {
	class GamePanel final : public IPanel {
	public:
		GamePanel() = default;

		void OnImGuiRender() override;

	private:
		// UI-only preview presets. This affects how the game view is presented (letterboxed)
		// inside the panel, and how UI input is mapped via SetUIViewportBounds.
		int m_resolutionPresetIndex = 0; // 0 == 16:9 (default)
	};
}
