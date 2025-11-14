#include "Scene.hpp"
#include "../../src/Graphics/Core/GraphicsManager.hpp"
#include "../../src/Graphics/Core/GizmosRenderer.hpp"
#include "../ECS/Systems/TransformSystem.hpp"
#include "../ECS/Systems/RenderSystem.hpp"
#include "../ECS/Systems/LightSystem.hpp"
#include "../ECS/Systems/RigidbodySystem.hpp"
#include "../ECS/Systems/ColliderSystem.hpp"
#include "../ECS/Systems/AudioSystem.hpp"
#include "../ECS/Systems/AnimatorSystem.hpp"
#include "../Animation/TransformClipIO.hpp"
#include <filesystem>
#include "../ECS/Systems/CameraSystem.hpp"
#include "../ECS/Systems/PhysicsSystem.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "ECS/Systems/ScriptSystem.hpp"
#include "ECS/Components/NativeScript.hpp"
#include "Core/Couroutine.hpp"
#include <iostream>

static void LoadAllClipsIntoAnimator(NE::ECS::Systems::AnimatorSystem* sys) {
	namespace fs = std::filesystem;
	const char* root = "Assets/Animations";
	if (!fs::exists(root)) return;
	for (auto& e : fs::recursive_directory_iterator(root)) {
		if (e.path().extension() == ".neclip") {
			auto clip = std::make_shared<NE::Animation::TransformClip>();
			if (NE::Animation::LoadTransformClip(*clip, e.path().string())) {
				// Use the file path as the registry key
				sys->RegisterClip(e.path().string(), clip);
			}
		}
	}
}
namespace NE::SceneManagement {

	void Scene::Init() {
		// input
		m_ecsCoordinator.m_rigidbodySystem->Init();
		//m_ecsCoordinator.m_colliderSystem->Init();
		m_ecsCoordinator.m_transformSystem->Init();
		m_ecsCoordinator.m_lightSystem->Init();
		m_ecsCoordinator.m_cameraSystem->Init();
		m_ecsCoordinator.m_renderSystem->Init();
		m_ecsCoordinator.m_audioSystem->Init();
		m_ecsCoordinator.m_physicsSystem->Init();
		m_ecsCoordinator.m_scriptSystem->Init();
		m_ecsCoordinator.m_animatorSystem->Init();
		LoadAllClipsIntoAnimator(m_ecsCoordinator.m_animatorSystem.get());

	}

	void Scene::Update(double dt)
	{
		m_ecsCoordinator.m_rigidbodySystem->Update(dt);
		//m_ecsCoordinator.m_colliderSystem->Update(dt);
		m_ecsCoordinator.m_transformSystem->Update(dt);
		m_ecsCoordinator.m_lightSystem->Update(dt);
		m_ecsCoordinator.m_cameraSystem->Update(dt);
		m_ecsCoordinator.m_renderSystem->Update(dt);
		//Graphics::GraphicsManager::DrawDebugLines(); // commented out, as when included, scene::render will not render the lines and triangles as itll be cleared after drawdebuglines/triangles ends
		//Graphics::GraphicsManager::DrawDebugTriangles();
#pragma region test gizmos renderer
		//Graphics::GizmosRenderer::TestGizmosRenderer();
#pragma endregion
		m_ecsCoordinator.m_audioSystem->Update(dt);
		m_ecsCoordinator.m_physicsSystem->Update(dt);
		m_ecsCoordinator.m_scriptSystem->Update(dt);
		m_ecsCoordinator.m_animatorSystem->Update(dt);
		Engine_UpdateCoroutines(static_cast<float>(dt)); //couroutine ticks
	}

	void Scene::Render(RenderPass pass) {
		switch (pass) {
		case RenderPass::SCENE:
			Graphics::GraphicsManager::DrawFrame();
			Graphics::GraphicsManager::DrawDebugLines();
			break;
		case RenderPass::GAME:
			Graphics::GraphicsManager::DrawFrame();
			break;
		case RenderPass::SCENE_PICKING:
			Graphics::GraphicsManager::UpdatePicking();
			Graphics::GraphicsManager::enableSorting = false; // disable sorting only for picking pass

			Graphics::GraphicsManager::DrawFrame();
			Graphics::GraphicsManager::s_PickingCommands.clear();
			Graphics::GraphicsManager::enableSorting = true; // re-enable sorting
			break;
		}
	}

	void Scene::Exit() {
		m_ecsCoordinator.m_rigidbodySystem->Exit();
		//m_ecsCoordinator.m_colliderSystem->Exit();
		m_ecsCoordinator.m_transformSystem->Exit();
		m_ecsCoordinator.m_lightSystem->Exit();
		m_ecsCoordinator.m_cameraSystem->Exit();
		m_ecsCoordinator.m_renderSystem->Exit();
		m_ecsCoordinator.m_audioSystem->Exit();
		m_ecsCoordinator.m_physicsSystem->Exit();
		m_ecsCoordinator.m_scriptSystem->Exit();	
		m_ecsCoordinator.m_animatorSystem->Exit();
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

	Core::LUIDRegistry& Scene::GetLuidRegistry() {
		return m_luidRegistry;
	}

}