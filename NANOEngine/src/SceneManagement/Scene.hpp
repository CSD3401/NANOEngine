#pragma once

#include "ECS/Core/ECSCoordinator.hpp"
#include "Core/LUIDRegistry.hpp"

namespace NE::SceneManagement {

	class Scene {
	public:
		void Init();
		void Update(double dt);
		void Render();
		void Exit();

		void ScriptStart();
		void ScriptPause();
		void ScriptStop();

		ECS::ECSCoordinator& GetECSCoordinator();
		Core::LUIDRegistry& GetLuidRegistry();

		// Dirty flag system for editor changes
		void MarkDirty();  // Changed to non-inline so we can add logging
		bool IsDirty() const { return m_isDirty; }
		void ClearDirty() { m_isDirty = false; }
		void MarkComponentsDirty();

	private:
		ECS::ECSCoordinator m_ecsCoordinator;
		bool m_isDirty = false;
		Core::LUIDRegistry m_luidRegistry;
	};

}



