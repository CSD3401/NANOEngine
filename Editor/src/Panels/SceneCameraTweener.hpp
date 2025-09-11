#pragma once
#include "Graphics/Core/Camera.hpp"
#include "../EditorEvents.hpp"

namespace Editor {

	class SceneCameraTweener
	{
	public:
		SceneCameraTweener();
		~SceneCameraTweener();

		static void TweenCameraToEntity(SelectEntityEvent const& event);
		void SetSceneCamera(NE::Graphics::Camera* camera);

		static constexpr float tweenDuration = 0.5f;
		static constexpr float tweenDistanceFactor = 10.0f;

	private:
		static NE::Graphics::Camera* sceneCamera;
	};
}