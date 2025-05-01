#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>

namespace Editor {
	class GamePanel : public IPanel {
	public:
		GamePanel();

		virtual void OnImGuiRender() override;

	private:
	};
}
