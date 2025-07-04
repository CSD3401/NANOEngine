#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>
#include "src/Graphics/Core/Camera.hpp"
#include "imgui/imgui_internal.h"

namespace Editor {
	class ScenePanel : public IPanel {
	public:
		ScenePanel(uint32_t sceneFrameBuffer);

		virtual void OnImGuiRender() override;

		NANOEngine::Graphics::Camera* GetCamera();

	private:
		NANOEngine::Graphics::Camera m_editorCamera;

		float m_cameraYaw = -90.0f;  // looking along -Z
		float m_cameraPitch = 0.0f;
		float m_cameraSpeed = 5.0f;
		float m_mouseSensitivity = 0.1f;
		bool  m_rightMouseHeld = false;
		ImVec2 m_lastMousePos = { 0, 0 };
	};
}
