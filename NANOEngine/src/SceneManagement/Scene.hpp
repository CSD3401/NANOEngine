#pragma once

#include "ECS/Core/ECSCoordinator.hpp"
#include "SceneLighting.hpp"
#include "NANOEngineAPI.hpp"

namespace NE::SceneManagement {

	class Scene {
	public:
		void InitEdit();
		void InitRuntime();
		void UpdateEdit(double dt);
		void UpdateRuntime(double dt);
		void Render();
		void ExitEdit();
		void ExitRuntime();

		void ScriptStart();
		void ScriptPause();
		void ScriptStop();

		void CameraEnter();
		void CameraExit();

		NANOENGINE_API ECS::ECSCoordinator& GetECSCoordinator();
		NANOENGINE_API LightingContainer& GetLightingContainer();
		NANOENGINE_API const LightingContainer& GetLightingContainer() const;
	private:
		ECS::ECSCoordinator m_ecsCoordinator;
		LightingContainer m_lightingContainer;
	};

}



