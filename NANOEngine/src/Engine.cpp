#include "Engine.hpp"

#include <memory>
#include "Graphics/Core/Window.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/OpenGL/GLShader.hpp"
#include "Graphics/OpenGL/GLPipeline.hpp"
#include "Graphics/OpenGL/GLFrameBuffer.hpp"
#include "Core/Profiler.hpp"
//#include <glad/glad.h>
#include "SceneManagement/Scene.hpp"
#include "../../src/Serialisation/JsonSceneSerializer.hpp"
#include "AssetManager.hpp"
#include <iostream>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include "Physics/PhysicsManager.hpp"
#include "Physics/JoltDebugRenderer.hpp"
#include "EngineState.hpp"

namespace NANOEngine {

	static std::unique_ptr<Graphics::Window> s_window;
	static std::unique_ptr<Graphics::IRenderContext> s_renderContext;
	static std::unique_ptr<Graphics::IFrameBuffer> s_sceneFrameBuffer; // temp
	static std::unique_ptr<Graphics::IFrameBuffer> s_pickingFrameBuffer; // temp
	static SceneManagement::Scene scene;

	void Initialize() {
		NE_PROFILE_FUNCTION();
		Graphics::WindowProperties props;
		props.Title = "NANOEngine";
		props.Width = 1920;
		props.Height = 1080;
		props.VSync = true;

		s_window = std::make_unique<Graphics::Window>(props);
		s_renderContext = std::make_unique<Graphics::OpenGL::GLContext>();
		s_renderContext->Init(s_window->GetNativeWindow());

		s_sceneFrameBuffer = std::make_unique<Graphics::OpenGL::GLFrameBuffer>(1920, 1080);
		s_pickingFrameBuffer = std::make_unique<Graphics::OpenGL::GLFrameBuffer>(1920, 1080);
		Graphics::GraphicsManager::Init();
		Physics::PhysicsManager::Init();

		//// Create simple triangle mesh (all temp stuff below)
		//float vertices[] = {
		//	-0.5f, -0.5f, -0.5f,  // 0
		//	 0.5f, -0.5f, -0.5f,  // 1
		//	 0.5f,  0.5f, -0.5f,  // 2
		//	-0.5f,  0.5f, -0.5f,  // 3
		//	-0.5f, -0.5f,  0.5f,  // 4
		//	 0.5f, -0.5f,  0.5f,  // 5
		//	 0.5f,  0.5f,  0.5f,  // 6
		//	-0.5f,  0.5f,  0.5f   // 7
		//};

		//uint32_t indices[] = {
		//	// front face
		//	0, 1, 2, 2, 3, 0,
		//	// right face
		//	1, 5, 6, 6, 2, 1,
		//	// back face
		//	7, 6, 5, 5, 4, 7,
		//	// left face
		//	4, 0, 3, 3, 7, 4,
		//	// bottom face
		//	4, 5, 1, 1, 0, 4,
		//	// top face
		//	3, 2, 6, 6, 7, 3
		//};

		scene.Init();
	}

	void Run(double dt) {
		NE_PROFILE_FUNCTION();
		s_window->PollEvents();

		Physics::PhysicsManager::Update(static_cast<float>(dt));
		s_sceneFrameBuffer->Bind();
		scene.Update(dt);
		s_sceneFrameBuffer->Unbind();
		s_pickingFrameBuffer->Bind();
		scene.RenderPicking();
		s_pickingFrameBuffer->Unbind();

		s_renderContext->SwapBuffers();
	}

	void Shutdown() {
		NE_PROFILE_FUNCTION();
		Physics::PhysicsManager::Shutdown();
		scene.Exit();

		s_sceneFrameBuffer.reset();
		s_pickingFrameBuffer.reset();
		s_renderContext->Shutdown();
		s_renderContext.reset();
		s_window.reset();
	}

	void* GetNativeWindowHandle() {
		return s_window->GetNativeWindow();
	}

	bool WindowShouldClose()
	{
		return s_window->ShouldClose();
	}

	uint32_t GetSceneFrameBuffer() {
		return s_sceneFrameBuffer->GetColorAttachment();
	}

	void SetEditorCamera(void* camera) {
		Graphics::GraphicsManager::SetCamera(reinterpret_cast<Graphics::Camera*>(camera));
	}

	uint32_t GetPickedEntity(uint32_t x, uint32_t y) {
		return Graphics::GraphicsManager::ReadPixel(s_pickingFrameBuffer.get(), x, y);
	}

	void SaveCurrentScene(std::string path) {
		Serialization::JsonSceneSerializer::Serialize(scene, path);
	}

	void LoadTargetScene(std::string targetPath) {
		Serialization::JsonSceneSerializer::Deserialize(scene, targetPath);
	}

	void LoadShader(std::string_view filePath) {
		Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLShader>(filePath.data(), false);
	}

	NANOENGINE_API std::shared_ptr<Graphics::Material> GetMaterial(std::string_view path) {
		return Asset::AssetManager::GetInstance().Load<NANOEngine::Graphics::Material>(path.data(), false);
	}

	NANOENGINE_API const std::vector<std::pair<std::string, std::shared_ptr<Graphics::Model>>>& GetAllModels()
	{
		return Asset::AssetManager::GetInstance().GetAssetsOfType<Graphics::Model>();
	}

	// Internal use only
	SceneManagement::Scene& GetScene() {
		return scene;
	}

	void EditorPlay() {
		g_EngineState = EngineState::Play;
		Physics::PhysicsManager::ActivateBodies();
	}

	void EditorPause() {
		g_EngineState = EngineState::Play;
		Physics::PhysicsManager::DeactivateBodies();
	}

	void EditorEdit() {
		g_EngineState = EngineState::Edit;
		Physics::PhysicsManager::DeactivateBodies();
	}
}