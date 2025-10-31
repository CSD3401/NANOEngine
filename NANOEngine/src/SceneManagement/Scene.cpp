#include "Scene.hpp"
#include "../../src/Graphics/Core/GraphicsManager.hpp"
#include "../ECS/Systems/TransformSystem.hpp"
#include "../ECS/Systems/RenderSystem.hpp"
#include "../ECS/Systems/LightSystem.hpp"
#include "../ECS/Systems/RigidbodySystem.hpp"
#include "../ECS/Systems/ColliderSystem.hpp"
#include "../ECS/Systems/AudioSystem.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "ECS/Systems/ScriptSystem.hpp"
#include "ECS/Components/NativeScript.hpp"
#include "Core/Couroutine.hpp"
#include <iostream>


namespace NE::SceneManagement {

	void Scene::Init() {
		// input
		m_ecsCoordinator.m_rigidbodySystem->Init();
		m_ecsCoordinator.m_colliderSystem->Init();
		m_ecsCoordinator.m_transformSystem->Init();
		m_ecsCoordinator.m_lightSystem->Init();
		m_ecsCoordinator.m_renderSystem->Init();
		m_ecsCoordinator.m_audioSystem->Init();
		m_ecsCoordinator.m_scriptSystem->Init();
	}

	void Scene::Update(double dt)
	{
		m_ecsCoordinator.m_rigidbodySystem->Update(dt);
		m_ecsCoordinator.m_colliderSystem->Update(dt);
		m_ecsCoordinator.m_transformSystem->Update(dt);
		m_ecsCoordinator.m_lightSystem->Update(dt);
		//Graphics::GraphicsManager::BeginFrame();
		//Graphics::GraphicsManager::DrawSkybox(); // here for now, not sure if theres a better place to put this
		//Graphics::GraphicsManager::DrawFrame();
		m_ecsCoordinator.m_renderSystem->Update(dt);
		//Graphics::GraphicsManager::DrawDebugLines();
		//Graphics::GraphicsManager::EndFrame();
		m_ecsCoordinator.m_audioSystem->Update(dt);
		m_ecsCoordinator.m_scriptSystem->Update(dt);
		Engine_UpdateCoroutines(dt); //couroutine ticks
	}

	void Scene::Render(RenderPass pass) {
		if (pass == RenderPass::Main) {
			Graphics::GraphicsManager::BeginFrame();
			Graphics::GraphicsManager::DrawFrame();
			Graphics::GraphicsManager::EndFrame();
			//Graphics::GraphicsManager::DrawSkybox();
			//m_ecsCoordinator.m_renderSystem->Update(0.0);
			Graphics::GraphicsManager::DrawDebugLines();
		} else if (pass == RenderPass::Picking) {
			m_ecsCoordinator.m_renderSystem->RenderPicking();
			Graphics::GraphicsManager::BeginFrame();
			Graphics::GraphicsManager::DrawFrame();
			Graphics::GraphicsManager::EndFrame();
		}
	}

	void Scene::Exit() {
		m_ecsCoordinator.m_rigidbodySystem->Exit();
		m_ecsCoordinator.m_colliderSystem->Exit();
		m_ecsCoordinator.m_transformSystem->Exit();
		m_ecsCoordinator.m_lightSystem->Exit();
		m_ecsCoordinator.m_renderSystem->Exit();
		m_ecsCoordinator.m_audioSystem->Exit();
		m_ecsCoordinator.m_scriptSystem->Exit();	
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

}