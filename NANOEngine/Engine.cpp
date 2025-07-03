#include "Engine.hpp"

#include <memory>
#include "Graphics/Core/Window.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/OpenGL/GLShader.hpp"
#include "Graphics/OpenGL/GLPipeline.hpp"
#include "Graphics/OpenGL/GLFrameBuffer.hpp"
#include "Core/Profiler.hpp"
#include <glad/glad.h>


// ECS
//#include "src/ECS/Core/ECSCoordinator.hpp"
//#include "src/ECS/Systems/TransformSystem.hpp"
#include "src/SceneManagement/Scene.hpp"

namespace NANOEngine {

	static std::unique_ptr<Graphics::Window> s_window;
	static std::unique_ptr<Graphics::IRenderContext> s_renderContext;
	static std::unique_ptr<Graphics::IFrameBuffer> s_sceneFrameBuffer; // temp
	//static ECS::ECSCoordinator* s_ecs;
	SceneManagement::Scene scene;

	//static Graphics::DrawCommand temp;


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
		Graphics::GraphicsManager::Init();

		//ECS::ECSCoordinator ecs;
		//s_ecs = &ecs;

		// Create simple triangle mesh (all temp stuff below)
		float vertices[] = {
			-0.5f, -0.5f, -0.5f,  // 0
			 0.5f, -0.5f, -0.5f,  // 1
			 0.5f,  0.5f, -0.5f,  // 2
			-0.5f,  0.5f, -0.5f,  // 3
			-0.5f, -0.5f,  0.5f,  // 4
			 0.5f, -0.5f,  0.5f,  // 5
			 0.5f,  0.5f,  0.5f,  // 6
			-0.5f,  0.5f,  0.5f   // 7
		};

		uint32_t indices[] = {
			// front face
			0, 1, 2, 2, 3, 0,
			// right face
			1, 5, 6, 6, 2, 1,
			// back face
			7, 6, 5, 5, 4, 7,
			// left face
			4, 0, 3, 3, 7, 4,
			// bottom face
			4, 5, 1, 1, 0, 4,
			// top face
			3, 2, 6, 6, 7, 3
		};

		//auto vb = std::make_shared<Graphics::OpenGL::GLVertexBuffer>(
		//	vertices, 
		//	static_cast<uint32_t>(sizeof(vertices)), 
		//	static_cast <uint32_t>(3 * sizeof(float)));

		//auto ib = std::make_shared<Graphics::OpenGL::GLIndexBuffer>(indices, 3);
		//auto ib = std::make_shared<Graphics::OpenGL::GLIndexBuffer>(indices, static_cast<uint32_t>(sizeof(indices) / sizeof(uint32_t)));

		//auto mesh = std::make_shared<Graphics::OpenGL::GLGeometryBuffer>(vb, ib);
		//auto mesh = LoadModel("Assets/Models/suzanne.obj"); // Replace with your model path



		//temp.mesh = mesh;
		//temp.material = material;
		//temp.transform.SetToIdentity();

		//ECS::TransformSystem transformSystem
		scene.Init();
	}

	void Run(double dt) {
		NE_PROFILE_FUNCTION();
		s_window->PollEvents();

		s_sceneFrameBuffer->Bind();
		scene.Update(dt);
		//Graphics::GraphicsManager::BeginFrame();
		//Graphics::GraphicsManager::Submit(temp);
		//Graphics::GraphicsManager::EndFrame();
		s_sceneFrameBuffer->Unbind();

		s_renderContext->SwapBuffers();	
	}

	void Shutdown() {
		NE_PROFILE_FUNCTION();
		s_sceneFrameBuffer.reset();
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

	// Internal use only
	SceneManagement::Scene& GetScene() {
		return scene;
	}

}