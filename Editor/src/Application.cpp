#include "Application.hpp"
// Needed for one shared instance of GLFW
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
#include <stb_image/stb_image.h>
#include <Input/InputManager.hpp>
#include "EditorScene.hpp"
#include "AssetManagement/AssetManager.hpp"
#include "Serialization/Serializer.hpp"

namespace Editor {
	bool Application::isRunning = true;
	Timer Application::timer;
	EditorLayer editorLayer;
	static bool s_showUnsavedChangesPopup = false;

	void Application::Init() {
		timer.Start();

		// Enable logging BEFORE engine initialization
		SpdLogger::GetInstance().EnableFileLogging("logs/session.log");
		SpdLogger::GetInstance().EnableCrashOnlyLogging("crash_logs/");

		// Now initialize engine (logs will be captured)
		NE::Initialize();

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

		glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
			if (EditorScene::isDirty) {
				glfwSetWindowShouldClose(w, GLFW_FALSE);
				s_showUnsavedChangesPopup = true;
			}
			});

		editorLayer.AddPanel<AssetBrowserPanel>("Assets/");
		editorLayer.AddPanel<ScriptsPanel>("../../../ChronoGame/Scripts/");
		Deserialization::JSON::DeserializeUserSettings();
		if (EditorScene::s_currentScenePath.empty() || EditorScene::s_currentSceneUUID.empty()) {
			EditorScene::s_currentScenePath = "Assets/NewScene.scene";
			EditorScene::s_currentSceneUUID = Assets::AssetManager::GetInstance().GetRecordBySource(EditorScene::s_currentScenePath)->id;
		}
		if (!NE::LoadScene(EditorScene::s_currentSceneUUID)) {
			SPD_ERROR("Failed to load scene binary, attempting fallback deserialization.");
			NE::CreateSceneFallback(EditorScene::s_currentSceneUUID);
			Deserialization::JSON::DeserializeScene(EditorScene::s_currentScenePath);
			NE::StartSceneFallback();
		}
		EditorScene::isDirty = false;
		std::shared_ptr<ScenePanel> sp = editorLayer.AddPanel<ScenePanel>();
		editorLayer.AddPanel<GamePanel>();
		editorLayer.AddPanel<HierarchyPanel>();
		editorLayer.AddPanel<InspectorPanel>();
		editorLayer.AddPanel<ProfilerPanel>();
		editorLayer.AddPanel<LoggerPanel>();

		NE::SetEditorCamera(reinterpret_cast<void*>(&EditorScene::m_editorCamera));
		Deserialization::JSON::DeserializeProjectSettings();
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

			// Handle unsaved changes popup
			if (s_showUnsavedChangesPopup) {
				ImGui::OpenPopup("Unsaved Changes");
				s_showUnsavedChangesPopup = false;
			}

			if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("You have unsaved changes. Do you want to save first?");
				ImGui::Separator();

				if (ImGui::Button("Save and Close", ImVec2(120, 0))) {
					// Save the scene
					auto sceneAsset = dynamic_cast<Assets::SceneAsset*>(Assets::AssetManager::GetInstance().GetRecord(EditorScene::s_currentSceneUUID)->asset.get());
					sceneAsset->SaveScene(EditorScene::s_currentScenePath);
					EditorScene::isDirty = false;
					ImGui::CloseCurrentPopup();
					glfwSetWindowShouldClose(static_cast<GLFWwindow*>(NE::GetNativeWindowHandle()), GLFW_TRUE);
				}
				ImGui::SameLine();
				if (ImGui::Button("Close Without Saving", ImVec2(160, 0))) {
					EditorScene::isDirty = false;
					ImGui::CloseCurrentPopup();
					glfwSetWindowShouldClose(static_cast<GLFWwindow*>(NE::GetNativeWindowHandle()), GLFW_TRUE);
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) {
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

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

	void Application::Exit() {
		ShutdownImGui();

		Serialization::JSON::SerializeProjectSettings();
		Serialization::JSON::SerializeUserSettings();

		NE::Shutdown();
	}
}