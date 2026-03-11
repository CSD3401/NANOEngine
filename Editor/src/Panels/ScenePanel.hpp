#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>
#include "imgui/imgui_internal.h"
#include "SceneCameraTweener.hpp"
#include <Math/Vec3.hpp>

namespace Editor {
	class ScenePanel : public IPanel {
	public:
		ScenePanel();

		virtual void OnImGuiRender() override;
	private:
		void SelectEntitiesInRect(ImVec2 startScreen,
			ImVec2 endScreen,
			ImVec2 panelPos,
			ImVec2 panelSize);

		NE::Math::Vec3 m_lastMoveDir{ 0.0f, 0.0f, 0.0f };
		// Accel behaviour
		float m_cameraAcceleration = 1.5f;  // units/sec^2
		float m_cameraDeceleration = 10.f;  // units/sec^2
		float m_currentMoveSpeed = 0.0f;

		float m_mouseSensitivity = 0.1f;
		bool  m_rightMouseHeld = false;
		ImVec2 m_lastMousePos = { 0, 0 };

		float m_fov;
		float m_aspectRatio;
		float m_nearPlane;
		float m_farPlane;

		bool  m_dragSelecting = false;
		ImVec2 m_dragStartScreen = {};
		ImVec2 m_dragEndScreen = {};

		SceneCameraTweener sceneCameraTweener;

		bool   m_wrapIgnoreNextDelta = false;
		//POINT  m_cursorScreenPosBeforeLook = { 0,0 }; // optional, restore on release
	};
}
