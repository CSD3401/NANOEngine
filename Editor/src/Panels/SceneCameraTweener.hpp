#pragma once
#include "Graphics/Core/EditorCamera.hpp"
#include "../EditorEvents.hpp"

namespace Editor {

	class SceneCameraTweener
	{
	public:
		SceneCameraTweener();
		~SceneCameraTweener();

		static void TweenCameraToEntity(SelectEntityEvent const& event);
		void SetSceneCamera(NE::Graphics::EditorCamera* camera);

		static constexpr float tweenDuration = 0.5f;
		static constexpr float tweenDistanceFactor = 10.0f;

	private:
		static NE::Graphics::EditorCamera* sceneCamera;
	};
}