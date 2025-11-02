#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>

namespace Editor {
	class GamePanel : public IPanel {
	public:
		GamePanel() = default;
		GamePanel(uint32_t framebuffer);

		virtual void OnImGuiRender() override;

	private:

		uint32_t m_framebuffer = static_cast<uint32_t>(-1);
	};
}
