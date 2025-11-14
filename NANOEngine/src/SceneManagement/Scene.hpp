#pragma once

#include "ECS/Core/ECSCoordinator.hpp"
#include "Core/LUIDRegistry.hpp"

namespace NE::SceneManagement {
	enum class RenderPass { SCENE, SCENE_PICKING, GAME };

	class Scene {
	public:
		void Init();
		void Update(double dt);
		void Render(RenderPass pass);
		void Exit();

		void ScriptStart();
		void ScriptPause();
		void ScriptStop();

		ECS::ECSCoordinator& GetECSCoordinator();
		Core::LUIDRegistry& GetLuidRegistry();

	private:
		ECS::ECSCoordinator m_ecsCoordinator;
		Core::LUIDRegistry m_luidRegistry;
	};

}


