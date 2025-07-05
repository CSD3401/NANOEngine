#include "Scene.hpp"
#include "../../src/Graphics/Core/GraphicsManager.hpp"
#include "../ECS/Systems/TransformSystem.hpp"
#include "../ECS/Systems/RenderSystem.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"


namespace NANOEngine::SceneManagement {

	void Scene::Init() {
		//m_transformSystem = std::make_unique<ECS::Systems::TransformSystem>(&m_ecsCoordinator.GetComponentManager());
		//m_renderSystem = std::make_unique<ECS::Systems::RenderSystem>(&m_ecsCoordinator.GetComponentManager());

		m_ecsCoordinator.m_transformSystem->Init();
		m_ecsCoordinator.m_renderSystem->Init();

		uint32_t entt = m_ecsCoordinator.CreateEntity();
		ECS::Component::Transform transform;
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Transform());
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Renderer());
		entt = m_ecsCoordinator.CreateEntity();
		transform.position = { 5.f, 0.f, 0.f };
		m_ecsCoordinator.AddComponent(entt, transform);
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Renderer());
		entt = m_ecsCoordinator.CreateEntity();
		transform.position = { -5.f, 0.f, 0.f };
		m_ecsCoordinator.AddComponent(entt, transform);
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Renderer());

		entt = m_ecsCoordinator.CreateEntity();
		transform.position = { 5.f, 5.f, 0.f };
		m_ecsCoordinator.AddComponent(entt, transform);
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Renderer());

		entt = m_ecsCoordinator.CreateEntity();
		transform.position = { 0.f, 5.f, 0.f };
		m_ecsCoordinator.AddComponent(entt, transform);
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Renderer());

		entt = m_ecsCoordinator.CreateEntity();
		transform.position = { -5.f, 5.f, 0.f };
		m_ecsCoordinator.AddComponent(entt, transform);
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Renderer());

		entt = m_ecsCoordinator.CreateEntity();
		transform.position = { 5.f, 10.f, 0.f };
		m_ecsCoordinator.AddComponent(entt, transform);
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Renderer());

		entt = m_ecsCoordinator.CreateEntity();
		transform.position = { 0.f, 10.f, 0.f };
		m_ecsCoordinator.AddComponent(entt, transform);
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Renderer());

		entt = m_ecsCoordinator.CreateEntity();
		transform.position = { -5.f, 10.f, 0.f };
		m_ecsCoordinator.AddComponent(entt, transform);
		m_ecsCoordinator.AddComponent(entt, ECS::Component::Renderer());
	}


	void Scene::Update(double dt)
	{
		m_ecsCoordinator.m_transformSystem->Update(dt);
		Graphics::GraphicsManager::BeginFrame();
		m_ecsCoordinator.m_renderSystem->Update(dt);
		Graphics::GraphicsManager::EndFrame();
	}

	void Scene::Exit()
	{
		m_ecsCoordinator.m_transformSystem->Exit();
		m_ecsCoordinator.m_renderSystem->Exit();
	}

	ECS::ECSCoordinator& Scene::GetECSCoordinator()
	{
		return m_ecsCoordinator;
	}

}