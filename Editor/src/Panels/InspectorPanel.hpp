#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>

namespace Editor {
	class InspectorPanel : public IPanel {
	public:
		InspectorPanel();

		virtual void OnImGuiRender() override;

	private:
	};
}
