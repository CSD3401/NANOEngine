#include "Scene.hpp"
#include "../../src/Graphics/Core/GraphicsManager.hpp"
#include "../ECS/Systems/TransformSystem.hpp"
#include "../ECS/Systems/RenderSystem.hpp"
#include "../ECS/Systems/LightSystem.hpp"
#include "../ECS/Systems/RigidbodySystem.hpp"
#include "../ECS/Systems/ColliderSystem.hpp"
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
	}

	void Scene::Update(double dt)
	{
		m_ecsCoordinator.m_rigidbodySystem->Update(dt);
		m_ecsCoordinator.m_colliderSystem->Update(dt);
		m_ecsCoordinator.m_transformSystem->Update(dt);
		m_ecsCoordinator.m_lightSystem->Update(dt);
	}

	void Scene::Render(RenderPass pass) {
		if (pass == RenderPass::Main) {
			Graphics::GraphicsManager::DrawSkybox();
			m_ecsCoordinator.m_renderSystem->Update(0.0);
			Graphics::GraphicsManager::DrawDebugLines();
		} else if (pass == RenderPass::Picking) {
			m_ecsCoordinator.m_renderSystem->RenderPicking();
		}
	}

	void Scene::Exit()
	{
		m_ecsCoordinator.m_rigidbodySystem->Exit();
		m_ecsCoordinator.m_colliderSystem->Exit();
		m_ecsCoordinator.m_transformSystem->Exit();
		m_ecsCoordinator.m_lightSystem->Exit();
		m_ecsCoordinator.m_renderSystem->Exit();
	}

	ECS::ECSCoordinator& Scene::GetECSCoordinator()
	{
		return m_ecsCoordinator;
	}

}