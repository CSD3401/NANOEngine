#pragma once

#include "../ECS/Core/ECSCoordinator.hpp"

namespace NE::SceneManagement {
	enum class RenderPass { Main, Picking };

	class Scene {
	public:
		void Init();
		void Update(double dt);
		void Render(RenderPass pass);
		void Exit();

		ECS::ECSCoordinator& GetECSCoordinator();

	private:
		ECS::ECSCoordinator m_ecsCoordinator;
	};

}


