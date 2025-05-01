#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>

namespace Editor {
	class HierarchyPanel : public IPanel {
	public:
		HierarchyPanel();

		virtual void OnImGuiRender() override;

	private:
	};
}
