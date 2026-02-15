#include "Scene.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/Core/GizmosRenderer.hpp"
#include "ECS/Systems/TransformSystem.hpp"
#include "ECS/Systems/RenderSystem.hpp"
#include "ECS/Systems/LightSystem.hpp"
#include "ECS/Systems/RigidbodySystem.hpp"
#include "ECS/Systems/ColliderSystem.hpp"
#include "ECS/Systems/AudioSystem.hpp"
#include "ECS/Systems/AnimatorSystem.hpp"
#include "ECS/Systems/HierarchySystem.hpp"
#include "ECS/Systems/CameraSystem.hpp"
#include "ECS/Systems/ScriptSystem.hpp"
#include "ECS/Systems/UIRenderSystem.hpp"
#include "ECS/Systems/UIEventSystem.hpp"
#include "ECS/Systems/UITransformSystem.hpp"
#include "ECS/Systems/CharacterControllerSystem.hpp"
#include "ECS/Systems/DecalProjectorSystem.hpp"
#include "../Animation/TransformClipIO.hpp"
#include "Core/Couroutine.hpp"
#include "Physics/PhysicsManager.hpp"

namespace NE::SceneManagement {

	void Scene::InitEdit() {
		m_ecsCoordinator.m_hierarchySystem->Init();
		m_ecsCoordinator.m_transformSystem->Init();
		m_ecsCoordinator.m_lightSystem->Init();
		m_ecsCoordinator.m_cameraSystem->Init();
		//m_ecsCoordinator.m_colliderSystem->Init();
		m_ecsCoordinator.m_renderSystem->Init();
		m_ecsCoordinator.m_decalProjectorSystem->Init();
		m_ecsCoordinator.m_audioSystem->Init();
		m_ecsCoordinator.m_scriptSystem->Init();
		m_ecsCoordinator.m_uiTransformSystem->Init();
		m_ecsCoordinator.m_uiEventSystem->Init();
		m_ecsCoordinator.m_uiRenderSystem->Init();

		m_ecsCoordinator.m_animatorSystem->Init();
	}

	void Scene::InitRuntime() {
		m_ecsCoordinator.m_hierarchySystem->Init();
		m_ecsCoordinator.m_transformSystem->Init();
		m_ecsCoordinator.m_rigidbodySystem->Init();
		m_ecsCoordinator.m_characterControllerSystem->Init();
		m_ecsCoordinator.m_lightSystem->Init();
		m_ecsCoordinator.m_cameraSystem->Init();
		m_ecsCoordinator.m_colliderSystem->Init();
		m_ecsCoordinator.m_renderSystem->Init();
		m_ecsCoordinator.m_decalProjectorSystem->Init();
		m_ecsCoordinator.m_audioSystem->Init();
		m_ecsCoordinator.m_scriptSystem->Init();
		m_ecsCoordinator.m_uiTransformSystem->Init();
		m_ecsCoordinator.m_uiEventSystem->Init();
		m_ecsCoordinator.m_uiRenderSystem->Init();

		m_ecsCoordinator.m_animatorSystem->Init();
	}

	void Scene::UpdateEdit(double dt) {
		m_ecsCoordinator.m_hierarchySystem->Update(dt);
		m_ecsCoordinator.m_transformSystem->Update(dt);
		m_ecsCoordinator.m_lightSystem->Update(dt);
		m_ecsCoordinator.m_cameraSystem->Update(dt);
		m_ecsCoordinator.m_colliderSystem->Update(dt);
		m_ecsCoordinator.m_renderSystem->Update(dt);
        m_ecsCoordinator.m_decalProjectorSystem->Update(dt);
		m_ecsCoordinator.m_audioSystem->Update(dt);

		m_ecsCoordinator.m_uiEventSystem->Update(dt);
		m_ecsCoordinator.m_uiRenderSystem->Update(dt);
		//m_ecsCoordinator.m_animatorSystem->Update(dt);
		m_ecsCoordinator.m_scriptSystem->Update(dt);
		Engine_UpdateCoroutines(static_cast<float>(dt));
	}

	void Scene::UpdateRuntime(double dt) {
		Physics::PhysicsManager::GetInstance().Update(dt);
		m_ecsCoordinator.m_rigidbodySystem->Update(dt);
		m_ecsCoordinator.m_characterControllerSystem->Update(dt);
		m_ecsCoordinator.m_transformSystem->Update(dt);
		m_ecsCoordinator.m_lightSystem->Update(dt);
		m_ecsCoordinator.m_cameraSystem->Update(dt);
		m_ecsCoordinator.m_renderSystem->Update(dt);
        m_ecsCoordinator.m_decalProjectorSystem->Update(dt);

		m_ecsCoordinator.m_audioSystem->Update(dt);

		m_ecsCoordinator.m_uiEventSystem->Update(dt);
		m_ecsCoordinator.m_uiRenderSystem->Update(dt);
		m_ecsCoordinator.m_animatorSystem->Update(dt);
		m_ecsCoordinator.m_scriptSystem->Update(dt);
		Engine_UpdateCoroutines(static_cast<float>(dt));
	}

	void Scene::Render() {
		Graphics::GraphicsManager::BeginFrame();
		Graphics::GraphicsManager::DrawFrame();
		//Graphics::GraphicsManager::DrawAllDebugGeometry();
		Graphics::GraphicsManager::EndFrame();
		// NOTE: DrawUI() removed - all UI (images + text) now uses integrated GraphicsManager pipeline
	}

	void Scene::ExitEdit() {
		m_ecsCoordinator.m_transformSystem->Exit();
		m_ecsCoordinator.m_lightSystem->Exit();
		m_ecsCoordinator.m_cameraSystem->Exit();
		m_ecsCoordinator.m_colliderSystem->Exit();
		m_ecsCoordinator.m_renderSystem->Exit();
        m_ecsCoordinator.m_decalProjectorSystem->Exit();
		m_ecsCoordinator.m_audioSystem->Exit();
		m_ecsCoordinator.m_scriptSystem->Exit();
		m_ecsCoordinator.m_uiEventSystem->Exit();
		m_ecsCoordinator.m_uiRenderSystem->Exit();
		m_ecsCoordinator.m_animatorSystem->Exit();
	}

	void Scene::ExitRuntime() {
		m_ecsCoordinator.m_rigidbodySystem->Exit();
		m_ecsCoordinator.m_transformSystem->Exit();
		m_ecsCoordinator.m_lightSystem->Exit();
		m_ecsCoordinator.m_cameraSystem->Exit();
		m_ecsCoordinator.m_colliderSystem->Exit();
		m_ecsCoordinator.m_renderSystem->Exit();
        m_ecsCoordinator.m_decalProjectorSystem->Exit();
		m_ecsCoordinator.m_audioSystem->Exit();
		m_ecsCoordinator.m_scriptSystem->Exit();
		m_ecsCoordinator.m_uiEventSystem->Exit();
		m_ecsCoordinator.m_uiRenderSystem->Exit();
		m_ecsCoordinator.m_animatorSystem->Exit();
		Physics::PhysicsManager::GetInstance().OnStop();
	}

	void Scene::ScriptStart() {
		m_ecsCoordinator.m_scriptSystem->StartScripts();
	}

	void Scene::ScriptPause() {
		m_ecsCoordinator.m_scriptSystem->PauseScripts();
	}

	void Scene::ScriptStop() {
		m_ecsCoordinator.m_scriptSystem->StopScripts();
	}

	ECS::ECSCoordinator& Scene::GetECSCoordinator() {
		return m_ecsCoordinator;
	}

	void Scene::CameraEnter() {
		m_ecsCoordinator.m_cameraSystem->Init();
	}

	void Scene::CameraExit() {
		m_ecsCoordinator.m_cameraSystem->Exit();
	}

}
