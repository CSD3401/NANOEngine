#pragma once

#include <vector>
#include <memory>
#include "Panels/IPanel.hpp"

typedef unsigned int ImU32;

namespace Editor {
	class EditorLayer {
	public:
		void OnImGuiRender();

        template<typename T, typename... Args>
        std::shared_ptr<T> AddPanel(Args&&... args) {
            auto panel = std::make_shared<T>(std::forward<Args>(args)...);
            m_panels.push_back(panel);
            return panel;
        }

	private:
		void DrawCustomTitleBar(const char* title, float height, ImU32 bgColor);

		std::vector<std::shared_ptr<IPanel>> m_panels;
	};
}

