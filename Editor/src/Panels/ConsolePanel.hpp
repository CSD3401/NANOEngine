#pragma once

#include "IPanel.hpp"

namespace Editor {
	class ConsolePanel final : public IPanel {
	public:
		ConsolePanel();

		virtual void OnImGuiRender() override;
	};
}
