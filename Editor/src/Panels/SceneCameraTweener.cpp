#include "SceneCameraTweener.hpp"
#include "Tween/TweenManager.hpp"
#include "Events/EventBus.hpp"
#include <ECS/Core/Entity.hpp>
#include <EditorInterface/ECSExports.hpp>
#include "ECS/Components/Transform.hpp"
#include "Math/Vec3.hpp"
#include <iostream>

NE::Graphics::Camera* Editor::SceneCameraTweener::sceneCamera = nullptr;
float isTweening = 0.0f;

namespace Editor {

	SceneCameraTweener::SceneCameraTweener()
	{
		NANOEngine::Events::EventBus::Get().Subscribe<SelectEntityEvent>(NANOEngine::Events::EventDomain::Editor, TweenCameraToEntity);
	}

	SceneCameraTweener::~SceneCameraTweener()
	{
		// Unsubscribe if required in the future...
	}

	void SceneCameraTweener::TweenCameraToEntity(SelectEntityEvent const& event)
	{
		if (!sceneCamera)
		{
			// Debug log: Scene Camera was not set in the SceneCameraTweener! Use SetSceneCamera() function in SceneCameraTweener during initialisation.
			return;
		}

		// If tween is already being performed on this object, return
		if (TweenManager::Get().CheckTween(sceneCamera))
		{
			return;
		}

		// Get entity
		uint32_t entity = event.selectedEntity;

		// Get transform
		NE::ECS::Component::Transform const& entityTransform = NE::ECS::Query::GetEntityTransform(entity);

		// Get position
		NE::Math::Vec3 entityPosition = entityTransform.position;

		// Get camera's look direction, and reverse it
		NE::Math::Vec3 reversedCameraLookDirection = -sceneCamera->GetForward();

		// Get scale
		NE::Math::Vec3 entityScale = entityTransform.scale;

		// Get the maximum scale value to approximate required distance
		float distance = std::max(std::max(entityScale.x, entityScale.y), entityScale.z);

		// Calculate distance
		NE::Math::Vec3 targetPosition = entityPosition + reversedCameraLookDirection * distance * tweenDistanceFactor;

		// Tween
		TweenManager::Get().StartTween(
			sceneCamera,
			&NE::Graphics::Camera::SetPosition,
			sceneCamera->GetPosition(),
			targetPosition,
			tweenDuration,
			TweenType::LINEAR
		);
	}

	void SceneCameraTweener::SetSceneCamera(NE::Graphics::Camera* camera)
	{
		sceneCamera = camera;
	}
}
