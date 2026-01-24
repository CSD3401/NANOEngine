#pragma once

#include <vector>
#include <memory>
#include <imgui/imgui.h>
#include "Panels/IPanel.hpp"

typedef unsigned int ImU32;

namespace Editor {
	class EditorLayer {
	public:
		EditorLayer();

		void OnImGuiRender();

        template<typename T, typename... Args>
        std::shared_ptr<T> AddPanel(Args&&... args) {
            auto panel = std::make_shared<T>(std::forward<Args>(args)...);
            m_panels.push_back(panel);
            return panel;
        }

		void SetIcon(unsigned int _icon);
	private:
		void DrawCustomTitleBar(const char* title, float height, ImU32 bgColor);
		ImTextureID icon;
		std::vector<std::shared_ptr<IPanel>> m_panels;

		ImTextureID playIcon;
		ImTextureID pauseIcon;
		ImTextureID stopIcon;
	};
}

