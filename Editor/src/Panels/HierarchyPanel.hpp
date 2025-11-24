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
		void DuplicateSelected();
		void CopySelected();
		void PasteSelected();

		// here for now
		std::vector<uint8_t> clipboard;
	};
}
