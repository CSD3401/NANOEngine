#include "Application.hpp"
// Needed for once shared instance of GLFW
#define GLFW_DLL
#include "glfw/glfw3.h"
//#include "Core/Logger.hpp"
#include "Core/SpdLogger.hpp"

#include "ImGuiLayer.hpp"
#include "EditorLayer.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>
#include <Engine.hpp>
#include <Core/Profiler.hpp>
#include <imgui/widgets/imguizmo/ImGuizmo.h>
#include "Panels/AssetBrowserPanel.hpp"
#include "Panels/ScenePanel.hpp"
#include "Panels/HierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/GamePanel.hpp"
#include "Panels/HistoryPanel.hpp"
#include "Panels/ProfilerPanel.hpp"
#include "Panels/LoggerPanel.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image\stb_image.h>

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

		NE::Initialize();
		
		// Enable crash-only logging instead of continuous file logging
		SpdLogger::GetInstance().EnableCrashOnlyLogging("crash_logs/");

		// Testing the SpdLogger with some demo messages
		SPD_INFO("=== NANOEngine Application Started ===");
		SPD_DEBUG("Initialization in progress...");
		SPD_INFO("Graphics API: OpenGL");
		SPD_DEBUG("Window creation completed");
		SPD_INFO("ImGui integration active");

		GLFWimage icon;
		icon.pixels = stbi_load("icon.png", &icon.width, &icon.height, 0, 4);
			glfwSetWindowIcon(static_cast<GLFWwindow*>(NE::GetNativeWindowHandle()), 1, &icon);

		GLuint texID;
		glGenTextures(1, &texID);
		glBindTexture(GL_TEXTURE_2D, texID);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, icon.width, icon.height,
			0, GL_RGBA, GL_UNSIGNED_BYTE, icon.pixels);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glBindTexture(GL_TEXTURE_2D, 0);

		editorLayer.SetIcon(texID);
		stbi_image_free(icon.pixels);

		InitImGui(static_cast<GLFWwindow*>(NE::GetNativeWindowHandle()));

		editorLayer.AddPanel<AssetBrowserPanel>("Assets/");
		NE::LoadStartupScene();
		std::shared_ptr<ScenePanel> sp = editorLayer.AddPanel<ScenePanel>(NE::GetSceneFrameBuffer());
		editorLayer.AddPanel<GamePanel>();
		editorLayer.AddPanel<HierarchyPanel>();
		editorLayer.AddPanel<InspectorPanel>();
		//editorLayer.AddPanel<HistoryPanel>();
		editorLayer.AddPanel<ProfilerPanel>();
		editorLayer.AddPanel<LoggerPanel>();

		NE::SetEditorCamera(reinterpret_cast<void*>(sp->GetCamera()));

		SPD_INFO("=== Application initialization complete ===");
		SPD_DEBUG("All panels loaded successfully");
		SPD_INFO("Ready for user interaction");
	}

	void Application::Run()
	{
		while (!NE::WindowShouldClose()) {
			Profiler::BeginFrame();
			timer.Update(); // move to engine run
			NE::Run(timer.GetDeltaTime());
			//LOG_INFO(timer.GetFPS());
			
			//SPD_INFO(timer.GetFPS());

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGuizmo::BeginFrame();

			editorLayer.OnImGuiRender();

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				GLFWwindow* backup_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_context);
			}
			Profiler::EndFrame();
		}
	}

	void Application::Exit()
	{
		ShutdownImGui();

		NE::Shutdown();
	}
}

