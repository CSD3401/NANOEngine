#pragma once

#include "ECS/Core/ECSCoordinator.hpp"

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

		ECS::ECSCoordinator& GetECSCoordinator();
	private:
		ECS::ECSCoordinator m_ecsCoordinator;
	};

}



