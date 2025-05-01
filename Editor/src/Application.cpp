#include "Application.hpp"
#define GLFW_DLL
#include "GLFW/glfw3.h"
#include "Core/Logger.hpp"
#include "ImGuiLayer.hpp"
#include "EditorLayer.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>
#include <Engine.hpp>

#include "Panels/AssetBrowserPanel.hpp"
#include "Panels/ScenePanel.hpp"
#include "Panels/HierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/GamePanel.hpp"

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

		NANOEngine::Initialize();

		m_nativeWindow = static_cast<GLFWwindow*>(NANOEngine::GetNativeWindowHandle());

		InitImGui(m_nativeWindow);

		editorLayer.AddPanel<AssetBrowserPanel>("Assets/");
		editorLayer.AddPanel<ScenePanel>();
		editorLayer.AddPanel<GamePanel>();
		editorLayer.AddPanel<HierarchyPanel>();
		editorLayer.AddPanel<InspectorPanel>();
	}

	void Application::Run()
	{
		while (!glfwWindowShouldClose(m_nativeWindow)) {
			glfwPollEvents();
			timer.Update();
			
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			editorLayer.OnImGuiRender();

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			// Update and render platform windows (multi-viewports)
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				GLFWwindow* backup_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_context);
			}

			glfwSwapBuffers(m_nativeWindow);
		}
	}

	void Application::Exit()
	{
		ShutdownImGui();

		glfwDestroyWindow(m_nativeWindow);
		glfwTerminate();
	}
}

