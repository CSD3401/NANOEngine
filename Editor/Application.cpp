#include "Application.hpp"
#define GLFW_DLL
#include "GLFW/glfw3.h"
#include "Core/Logger.hpp"
#include "src/ImGuiLayer.hpp"
#include "src/EditorLayer.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>

namespace Editor {
	bool Application::isRunning = true;
	EditorLayer editorLayer;

	void Application::Init() {
		timer.Start();

		//NANOEngine::Graphics::RenderAPI api = NANOEngine::Graphics::RenderAPI::OpenGL;

		//NANOEngine::Graphics::WindowProperties props;
		//props.title = "NANOEngine";
		//props.width = 1920;
		//props.height = 1080;
		//props.fullscreen = false;
		//props.setHints = [api]() {
		//	switch (api) {
		//	case NANOEngine::Graphics::RenderAPI::OpenGL:
		//		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		//		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		//		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		//		//glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
		//		break;
		//	case NANOEngine::Graphics::RenderAPI::Vulkan:
		//		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		//		break;
		//	}
		//};

		if (!glfwInit()) {
			LOG_CRITICAL("Failed to initialize GLFW");
			return;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		m_nativeWindow = glfwCreateWindow(1280, 720, "My Editor", nullptr, nullptr);

		if (!m_nativeWindow) {
			LOG_CRITICAL("Failed to create window");
			glfwTerminate();
			return;
		}
		glfwMakeContextCurrent(m_nativeWindow);

		m_appWindow = std::make_unique<NANOEngine::Graphics::Window>();
		m_appWindow->Init(m_nativeWindow);

		m_renderContext = std::make_unique<NANOEngine::Graphics::OpenGL::GLContext>();
		m_renderContext->Init(m_nativeWindow);

		InitImGui(m_nativeWindow);
	}

	void Application::Run()
	{
		while (!glfwWindowShouldClose(m_nativeWindow)) {
			glfwPollEvents();
			timer.Update();
			
			// Start new ImGui frame
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			// Render ImGui content
			editorLayer.OnImGuiRender();

			// End frame and render
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			// Update and render platform windows (multi-viewports)
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				GLFWwindow* backup_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_context);
			}


			//m_renderContext->SwapBuffers();
			//m_appWindow->SwapBuffers();
		}
	}

	void Application::Exit()
	{
		ShutdownImGui();

		glfwDestroyWindow(m_nativeWindow);
		glfwTerminate();
	}
}

