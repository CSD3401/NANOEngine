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


namespace NE::SceneManagement {

	void Scene::Init() {
		// input
		m_ecsCoordinator.m_rigidbodySystem->Init();
		m_ecsCoordinator.m_colliderSystem->Init();
		m_ecsCoordinator.m_transformSystem->Init();
		m_ecsCoordinator.m_lightSystem->Init();
		m_ecsCoordinator.m_renderSystem->Init();
		m_ecsCoordinator.m_audioSystem->Init();
	}


	void Scene::Update(double dt)
	{
		m_ecsCoordinator.m_rigidbodySystem->Update(dt);
		m_ecsCoordinator.m_colliderSystem->Update(dt);
		m_ecsCoordinator.m_transformSystem->Update(dt);
		m_ecsCoordinator.m_lightSystem->Update(dt);
		Graphics::GraphicsManager::BeginFrame();
		Graphics::GraphicsManager::DrawSkybox(); // here for now, not sure if theres a better place to put this
		m_ecsCoordinator.m_renderSystem->Update(dt);
		Graphics::GraphicsManager::DrawDebugLines();
		Graphics::GraphicsManager::EndFrame();
		m_ecsCoordinator.m_audioSystem->Update(dt);
	}

	void Scene::RenderPicking() // TEMP hopefully can be optimized so i dont run twice
	{
		Graphics::GraphicsManager::BeginFrame();
		m_ecsCoordinator.m_renderSystem->RenderPicking();
		Graphics::GraphicsManager::EndFrame();
	}

	void Scene::Exit()
	{
		m_ecsCoordinator.m_rigidbodySystem->Exit();
		m_ecsCoordinator.m_colliderSystem->Exit();
		m_ecsCoordinator.m_transformSystem->Exit();
		m_ecsCoordinator.m_lightSystem->Exit();
		m_ecsCoordinator.m_renderSystem->Exit();
		m_ecsCoordinator.m_audioSystem->Exit();
	}

	ECS::ECSCoordinator& Scene::GetECSCoordinator()
	{
		return m_ecsCoordinator;
	}

}