#include "Scene.hpp"
#include "../../src/Graphics/Core/GraphicsManager.hpp"
#include "../../src/Graphics/Core/GizmosRenderer.hpp"
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
#include "ECS/Systems/UIRenderSystem.hpp"
#include "../ECS/Components/UIRectTransform.hpp"
#include "../ECS/Components/UIImage.hpp"
#include "Core/Couroutine.hpp"
#include <iostream>


namespace NE::SceneManagement {

	void Scene::CreateTestUI() {
		using ECS::Component::UIRectTransform;
		using ECS::Component::UIImage;

		std::cout << "\n=== Creating Test UI ===" << std::endl;

		// Test 1: Red Box (top-left)
		{
			ECS::Entity e = m_ecsCoordinator.CreateUIEntity();
			std::cout << "Created UI entity " << e << " (Red Box)" << std::endl;

			auto& rect = m_ecsCoordinator.GetComponent<UIRectTransform>(e);
			rect.x = 50.f;  rect.y = 50.f;
			rect.width = 200.f;  rect.height = 100.f;

			auto& img = m_ecsCoordinator.GetComponent<UIImage>(e);
			img.color = Math::Vec4{ 1.f, 0.f, 0.f, 0.8f };
			img.material = nullptr;

			std::cout << "  Position: (" << rect.x << ", " << rect.y << ")" << std::endl;
			std::cout << "  Size: " << rect.width << "x" << rect.height << std::endl;
		}

		std::cout << "Test UI creation complete!\n" << std::endl;
	}

	void Scene::Init() {
		// input
		m_ecsCoordinator.m_rigidbodySystem->Init();
		m_ecsCoordinator.m_colliderSystem->Init();
		m_ecsCoordinator.m_transformSystem->Init();
		m_ecsCoordinator.m_lightSystem->Init();
		m_ecsCoordinator.m_renderSystem->Init();
		m_ecsCoordinator.m_audioSystem->Init();
		m_ecsCoordinator.m_scriptSystem->Init();
		m_ecsCoordinator.m_uiRenderSystem->Init();

		// temp
		CreateTestUI();
	}

	void Scene::Update(double dt)
	{
		m_ecsCoordinator.m_rigidbodySystem->Update(dt);
		m_ecsCoordinator.m_colliderSystem->Update(dt);
		m_ecsCoordinator.m_transformSystem->Update(dt);
		m_ecsCoordinator.m_lightSystem->Update(dt);
		m_ecsCoordinator.m_renderSystem->Update(dt);
		//Graphics::GraphicsManager::DrawDebugLines(); // commented out, as when included, scene::render will not render the lines and triangles as itll be cleared after drawdebuglines/triangles ends
		//Graphics::GraphicsManager::DrawDebugTriangles();
#pragma region test gizmos renderer
		//Graphics::GizmosRenderer::TestGizmosRenderer();
#pragma endregion
		m_ecsCoordinator.m_audioSystem->Update(dt);
		m_ecsCoordinator.m_scriptSystem->Update(dt);
		m_ecsCoordinator.m_uiRenderSystem->Update(dt);
		Engine_UpdateCoroutines(static_cast<float>(dt)); //couroutine ticks
	}

	void Scene::Render(RenderPass pass) {
		if (pass == RenderPass::Main) {
			Graphics::GraphicsManager::BeginFrame();
			Graphics::GraphicsManager::DrawSkybox();
			Graphics::GraphicsManager::DrawFrame();
			Graphics::GraphicsManager::EndFrame();

			Graphics::GraphicsManager::DrawUI();

			//Graphics::GraphicsManager::DrawSkybox();
			//m_ecsCoordinator.m_renderSystem->Update(0.0);
			//Graphics::GraphicsManager::DrawDebugLines();
		} else if (pass == RenderPass::Picking) {
			Graphics::GraphicsManager::enableSorting = false; // disable sorting only for picking pass
			m_ecsCoordinator.m_renderSystem->RenderPicking();
			Graphics::GraphicsManager::BeginFrame();
			Graphics::GraphicsManager::DrawFrame();
			Graphics::GraphicsManager::EndFrame();

			//Graphics::GraphicsManager::DrawUIPicking();

			Graphics::GraphicsManager::enableSorting = true; // re-enable sorting
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
		m_ecsCoordinator.m_uiRenderSystem->Exit();	
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