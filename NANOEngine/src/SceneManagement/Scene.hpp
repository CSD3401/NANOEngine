#pragma once

#include "../ECS/Core/ECSCoordinator.hpp"

namespace NE::SceneManagement {

	class Scene {
	public:
		void Init();
		void Update(double dt);
		void RenderPicking();
		void Exit();

		ECS::ECSCoordinator& GetECSCoordinator();

	private:
		ECS::ECSCoordinator m_ecsCoordinator;
	};

}


