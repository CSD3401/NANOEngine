#pragma once

#include "../ECS/Core/ECSCoordinator.hpp"

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

		// Dirty flag system for editor changes
		void MarkDirty();  // Changed to non-inline so we can add logging
		bool IsDirty() const { return m_isDirty; }
		void ClearDirty() { m_isDirty = false; }
		void MarkComponentsDirty();

	private:
		ECS::ECSCoordinator m_ecsCoordinator;
		bool m_isDirty = false;
	};

}



