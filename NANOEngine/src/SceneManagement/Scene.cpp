#include "Scene.hpp"
#include "../../src/Graphics/Core/GraphicsManager.hpp"
#include "../ECS/Systems/TransformSystem.hpp"
#include "../ECS/Systems/RenderSystem.hpp"
#include "../ECS/Systems/LightSystem.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"


namespace NANOEngine::SceneManagement {

	void Scene::Init() {
		//m_transformSystem = std::make_unique<ECS::Systems::TransformSystem>(&m_ecsCoordinator.GetComponentManager());
		//m_renderSystem = std::make_unique<ECS::Systems::RenderSystem>(&m_ecsCoordinator.GetComponentManager());

		m_ecsCoordinator.m_transformSystem->Init();
		m_ecsCoordinator.m_lightSystem->Init();
		m_ecsCoordinator.m_renderSystem->Init();
	}


	void Scene::Update(double dt)
	{
		m_ecsCoordinator.m_transformSystem->Update(dt);
		m_ecsCoordinator.m_lightSystem->Update(dt);
		Graphics::GraphicsManager::BeginFrame();
		Graphics::GraphicsManager::DrawSkybox(); // here for now, not sure if theres a better place to put this
		m_ecsCoordinator.m_renderSystem->Update(dt);
		Graphics::GraphicsManager::EndFrame();
	}

	void Scene::RenderPicking() // TEMP hopefully can be optimized so i dont run twice
	{
		//m_ecsCoordinator.m_transformSystem->Update(0.0);
		Graphics::GraphicsManager::BeginFrame();
		m_ecsCoordinator.m_renderSystem->RenderPicking();
		Graphics::GraphicsManager::EndFrame();
	}

	void Scene::Exit()
	{
		m_ecsCoordinator.m_transformSystem->Exit();
		m_ecsCoordinator.m_lightSystem->Exit();
		m_ecsCoordinator.m_renderSystem->Exit();
	}

	ECS::ECSCoordinator& Scene::GetECSCoordinator()
	{
		return m_ecsCoordinator;
	}

}