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
#include <glfw/glfw3.h>
#include <stb_image/stb_image.h>
#include "Physics/PhysicsManager.hpp"
#include "Physics/JoltDebugRenderer.hpp"
//#include "EditorInterface/PhysicsExports.hpp"
#include "EngineState.hpp"
#include "Audio/AudioBank.hpp"
#include "SceneManagement/SceneManager.hpp"
#include "Tween/TweenManager.hpp"
#include "Core/SpdLogger.hpp"
#include "Input/InputManager.hpp"
#include "Graphics/OpenGL/GLTexture.hpp"

// Replace with forward declarations if needed
// Forward declare instead of including
// Physics as a export
//namespace NE::Physics {
//	class PhysicsManager; 
//}

namespace NE {

	static std::unique_ptr<Graphics::Window> s_window;
	static std::unique_ptr<Graphics::IRenderContext> s_renderContext;
	static std::unique_ptr<Graphics::IFrameBuffer> s_sceneFrameBuffer; // temp
	static std::unique_ptr<Graphics::IFrameBuffer> s_pickingFrameBuffer; // temp

	static SceneManagement::SceneManager gSceneManager;

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
		//Physics::Command::Init();
	}

	void LoadStartupScene() {
		gSceneManager.LoadScene("Assets/NewScene.scene");
	}

	void Run(double dt) {
		NE_PROFILE_FUNCTION();
		//s_window->PollEvents();

		Physics::PhysicsManager::Update(static_cast<float>(dt));
		//Physics::Command::Update(static_cast<float>(dt));

		gSceneManager.Update(dt);

		s_sceneFrameBuffer->Bind();
		Graphics::GraphicsManager::BeginFrame();
		gSceneManager.Render(NE::SceneManagement::RenderPass::Main);
		TweenManager::Get().Update(static_cast<float>(dt));
		Graphics::GraphicsManager::EndFrame();
		s_sceneFrameBuffer->Unbind();
		
		s_pickingFrameBuffer->Bind();
		Graphics::GraphicsManager::BeginFrame();
		gSceneManager.Render(NE::SceneManagement::RenderPass::Picking);
		Graphics::GraphicsManager::EndFrame();
		s_pickingFrameBuffer->Unbind();

		//s_renderContext->SwapBuffers();
	}

	void Shutdown() {
		NE_PROFILE_FUNCTION();
		SaveCurrentScene("Assets/NewScene.scene");
		Physics::PhysicsManager::Shutdown();
		//Physics::Command::Shutdown();

		gSceneManager.ExitScene();

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
		Serialization::JsonSceneSerializer::Serialize(*gSceneManager.GetActive(), path);
	}

	void LoadTargetScene(std::string targetPath) {
		Serialization::JsonSceneSerializer::Deserialize(*gSceneManager.GetActive(), targetPath);
	}

	void LoadShader(std::string_view filePath) {
		Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLShader>(filePath.data(), false);
	}

	void LoadTexture(std::string_view filePath) {
		Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLTexture>(filePath.data(), false);
	}

	std::shared_ptr<Graphics::OpenGL::GLTexture> GetTexture(std::string_view filePath)
	{
		return Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLTexture>(filePath.data(), false);
	}

	std::shared_ptr<Graphics::Material> GetMaterial(std::string_view path) {
		return Asset::AssetManager::GetInstance().Load<NE::Graphics::Material>(path.data(), false);
	}

	const std::vector<std::pair<std::string, std::shared_ptr<Graphics::Model>>>& GetAllModels()
	{
		return Asset::AssetManager::GetInstance().GetAssetsOfType<Graphics::Model>();
	}

	const std::vector<std::pair<std::string, std::shared_ptr<Graphics::OpenGL::GLShader>>>& GetAllShaders() {
		return Asset::AssetManager::GetInstance().GetAssetsOfType<Graphics::OpenGL::GLShader>();
	}
	
	const std::vector<std::pair<std::string, std::shared_ptr<Asset::AudioBank>>>& GetAllAudioBanks() {
		return Asset::AssetManager::GetInstance().GetAssetsOfType<Asset::AudioBank>();
	}

	


	size_t GetNumEntities() {
		return gSceneManager.GetActive()->GetECSCoordinator().GetUsedEntities().size();
	}

	// Internal use only
	SceneManagement::Scene& GetScene() {
		return *gSceneManager.GetActive();
	}

	void EditorPlay() {
		g_EngineState = EngineState::Play;
		Physics::PhysicsManager::ActivateBodies();
		//Physics::Command::ActivateBodies();
		gSceneManager.GetActive()->ScriptStart();
	}

	void EditorPause() {
		g_EngineState = EngineState::Play;
		Physics::PhysicsManager::DeactivateBodies();
		//Physics::Command::DeactivateBodies();
		gSceneManager.GetActive()->ScriptPause();
	}

	void EditorEdit() {
		g_EngineState = EngineState::Edit;
		Physics::PhysicsManager::DeactivateBodies();
		//Physics::Command::DeactivateBodies();
		gSceneManager.GetActive()->ScriptStop();
	}
}