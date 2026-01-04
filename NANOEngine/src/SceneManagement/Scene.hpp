#pragma once

#include "ECS/Core/ECSCoordinator.hpp"
#include "Core/LUIDRegistry.hpp"
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

		NANOENGINE_API ECS::ECSCoordinator& GetECSCoordinator();
		Core::LUIDRegistry& GetLuidRegistry();
	private:
		ECS::ECSCoordinator m_ecsCoordinator;
		Core::LUIDRegistry m_luidRegistry;
	};

}



