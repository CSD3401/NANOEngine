#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>

namespace Editor {
	class ScenePanel : public IPanel {
	public:
		ScenePanel();

		virtual void OnImGuiRender() override;

	private:
	};
}
