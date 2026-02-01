#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>

namespace Editor {
	class GamePanel final : public IPanel {
	public:
		GamePanel() = default;

		void OnImGuiRender() override;
	};
}
