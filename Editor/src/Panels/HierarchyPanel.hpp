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
		void DrawHierarchyContextMenuBody(bool canEditHierarchy, uint32_t contextEntityId);
	};
}
