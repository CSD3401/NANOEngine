#pragma once

#include "../ECS/Core/ECSCoordinator.hpp"

namespace NANOEngine::ECS::Systems {
	class TransformSystem;
	class RenderSystem;
}

namespace NANOEngine::SceneManagement {

	class Scene {
	public:
		void Init();
		void Update(double dt);
		void Exit();

		ECS::ECSCoordinator& GetECSCoordinator();

	private:
		ECS::ECSCoordinator m_ecsCoordinator;
		//std::unique_ptr<ECS::Systems::TransformSystem> m_transformSystem;
		//std::unique_ptr<ECS::Systems::RenderSystem> m_renderSystem;
	};

}


