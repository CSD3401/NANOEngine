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
#include "Panels/ScriptsPanel.hpp"
#include "Panels/ScenePanel.hpp"
#include "Panels/HierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/GamePanel.hpp"
#include "Panels/ProfilerPanel.hpp"
#include "Panels/LoggerPanel.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image\stb_image.h>
#include <Input/InputManager.hpp>
#include "Panels/AnimationPanel.hpp"
#include "Panels/AnimationRuntimePanel.hpp"
#include "Panels/AnimationGraphPanel.hpp"

namespace Editor {
	bool Application::isRunning = true;
	Timer Application::timer;
	EditorLayer editorLayer;

	void Application::Init() {
		timer.Start();

		// Enable logging BEFORE engine initialization
		SpdLogger::GetInstance().EnableFileLogging("logs/session.log");
		SpdLogger::GetInstance().EnableCrashOnlyLogging("crash_logs/");

		// Now initialize engine (logs will be captured)
		NE::Initialize();

		// Testing the SpdLogger with some demo messages
		SPD_INFO("=== NANOEngine Application Started ===");
		SPD_DEBUG("Initialization in progress...");
		SPD_INFO("Graphics API: OpenGL");
		SPD_DEBUG("Window creation completed");
		SPD_INFO("ImGui integration active");

		GLFWwindow* window = static_cast<GLFWwindow*>(NE::GetNativeWindowHandle());

		GLFWimage icon;
		icon.pixels = stbi_load("icon.png", &icon.width, &icon.height, 0, 4);
			glfwSetWindowIcon(window, 1, &icon);

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

		InitImGui(window);

		glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int sc, int action, int mods) {
			ImGui_ImplGlfw_KeyCallback(w, key, sc, action, mods);
			//if (!ImGui::GetIO().WantCaptureKeyboard)
				NE::InputManager::OnKey(key, sc, action, mods);
			});
		glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
			ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);
			//if (!ImGui::GetIO().WantCaptureMouse)
				NE::InputManager::OnMouseButton(button, action, mods);
			});
		glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
			ImGui_ImplGlfw_CursorPosCallback(w, x, y);
			//if (!ImGui::GetIO().WantCaptureMouse)
				NE::InputManager::OnCursorPos(x, y);
			});
		glfwSetScrollCallback(window, [](GLFWwindow* w, double xoff, double yoff) {
			ImGui_ImplGlfw_ScrollCallback(w, xoff, yoff);
			//if (!ImGui::GetIO().WantCaptureMouse)
				NE::InputManager::OnScroll(xoff, yoff);
			});
		glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int c) {
			ImGui_ImplGlfw_CharCallback(w, c);
			// ImGui will read it too via its backend; forwarding to engine lets your widgets read raw text if needed
			NE::InputManager::OnCharInput((uint32_t)c);
			});

		editorLayer.AddPanel<AssetBrowserPanel>("Assets/");
		editorLayer.AddPanel<ScriptsPanel>("../../../ChronoGame/Scripts/");
		NE::LoadStartupScene();
		std::shared_ptr<ScenePanel> sp = editorLayer.AddPanel<ScenePanel>(NE::GetSceneFrameBuffer());
		editorLayer.AddPanel<GamePanel>(NE::GetGameFrameBuffer());
		editorLayer.AddPanel<HierarchyPanel>();
		editorLayer.AddPanel<InspectorPanel>();
		//editorLayer.AddPanel<ProfilerPanel>();
		editorLayer.AddPanel<LoggerPanel>();
		//editorLayer.AddPanel<AnimationPanel>();
		//editorLayer.AddPanel<AnimatorRuntimePanel>();
		//editorLayer.AddPanel<AnimatorGraphPanel>();


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

			NE::InputManager::BeginFrame();
			glfwPollEvents();

			NE::Run(timer.GetDeltaTime());

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

			glfwSwapBuffers(static_cast<GLFWwindow*>(NE::GetNativeWindowHandle()));

			Profiler::EndFrame();
		}
	}

	void Application::Exit()
	{
		ShutdownImGui();

		NE::Shutdown();
	}
}

