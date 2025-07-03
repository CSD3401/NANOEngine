#pragma once
#include "IPanel.hpp"
#include "../Command/CommandHistory.hpp"

namespace Editor {
	class HistoryPanel final : public IPanel {
	public:
		void OnImGuiRender() override;
	};
}
