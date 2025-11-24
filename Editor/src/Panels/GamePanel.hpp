#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>

namespace Editor {
	class GamePanel : public IPanel {
	public:
		GamePanel() = default;

		virtual void OnImGuiRender() override;

	private:
	};
}
