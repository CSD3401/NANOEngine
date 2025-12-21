#pragma once

#include "ECS/Core/ECSCoordinator.hpp"
#include "Core/LUIDRegistry.hpp"
#include "NANOEngineAPI.hpp"

namespace NE::SceneManagement {

	class Scene {
	public:
		void Init();
		void UpdateEdit(double dt);
		void UpdateRuntime(double dt);
		void Render();
		void Exit();

		void ScriptStart();
		void ScriptPause();
		void ScriptStop();

		NANOENGINE_API ECS::ECSCoordinator& GetECSCoordinator();
		Core::LUIDRegistry& GetLuidRegistry();
	private:
		ECS::ECSCoordinator m_ecsCoordinator;
		Core::LUIDRegistry m_luidRegistry;
	};

}



