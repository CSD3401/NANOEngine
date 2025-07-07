#include "Application.hpp"
// Needed for once shared instance of GLFW
#define GLFW_DLL
#include "GLFW/glfw3.h"
#include "src/Core/Logger.hpp"
#include "ImGuiLayer.hpp"
#include "EditorLayer.hpp"
#include "src/Graphics/OpenGL/GLContext.hpp"
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>
#include <Engine.hpp>
#include <src/Core/Profiler.hpp>
#include <imgui/widgets/imguizmo/ImGuizmo.h>
#include "Panels/AssetBrowserPanel.hpp"
#include "Panels/ScenePanel.hpp"
#include "Panels/HierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/GamePanel.hpp"
#include "Panels/HistoryPanel.hpp"
#include "Panels/ProfilerPanel.hpp"

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

		InitImGui(static_cast<GLFWwindow*>(NANOEngine::GetNativeWindowHandle()));

		editorLayer.AddPanel<AssetBrowserPanel>("Assets/");
		std::shared_ptr<ScenePanel> sp = editorLayer.AddPanel<ScenePanel>(NANOEngine::GetSceneFrameBuffer());
		editorLayer.AddPanel<GamePanel>();
		editorLayer.AddPanel<HierarchyPanel>();
		editorLayer.AddPanel<InspectorPanel>();
		editorLayer.AddPanel<HistoryPanel>();
		editorLayer.AddPanel<ProfilerPanel>();

		NANOEngine::SetEditorCamera(reinterpret_cast<void*>(sp->GetCamera()));
	}

	void Application::Run()
	{
		while (!NANOEngine::WindowShouldClose()) {
			Profiler::BeginFrame();
			timer.Update(); // move to engine run
			NANOEngine::Run(timer.GetDeltaTime());
			//LOG_INFO(timer.GetFPS());
			
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

		NANOEngine::Shutdown();
	}
}

