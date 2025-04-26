#include "Application.hpp"
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

		NANOEngine::Graphics::RenderAPI api = NANOEngine::Graphics::RenderAPI::OpenGL;

		NANOEngine::Graphics::WindowProperties props;
		props.title = "NANOEngine";
		props.width = 1920;
		props.height = 1080;
		props.fullscreen = false;
		props.setHints = [api]() {
			switch (api) {
			case NANOEngine::Graphics::RenderAPI::OpenGL:
				glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
				glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
				glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
				//glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
				break;
			case NANOEngine::Graphics::RenderAPI::Vulkan:
				glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
				break;
			}
		};

		m_appWindow = std::make_unique<NANOEngine::Graphics::Window>(props);

		m_renderContext = std::make_unique<NANOEngine::Graphics::OpenGL::GLContext>();
		m_renderContext->Init(m_appWindow->GetWindowHandle());

		printf("OpenGL %s\n", glGetString(GL_VERSION));

		InitImGui(m_appWindow->GetWindowHandle());
	}

	void Application::Run()
	{
		while (isRunning) {
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


			m_renderContext->SwapBuffers();
			//m_appWindow->SwapBuffers();
		}
	}

	void Application::Exit()
	{
		ShutdownImGui();
	}
}

