#include "Application.hpp"
// Needed for once shared instance of GLFW
#define GLFW_DLL
#include "glfw/glfw3.h"
//#include "Core/Logger.hpp"
#include "Core/SpdLogger.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include <Engine.hpp>
#include <Core/Profiler.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include <Input/InputManager.hpp>

namespace Editor {
	bool Application::isRunning = true;
	Timer Application::timer;

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

		stbi_image_free(icon.pixels);

		glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int sc, int action, int mods) {
			NE::InputManager::OnKey(key, sc, action, mods);
			});
		glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
			NE::InputManager::OnMouseButton(button, action, mods);
			});
		glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
			NE::InputManager::OnCursorPos(x, y);
			});
		glfwSetScrollCallback(window, [](GLFWwindow* w, double xoff, double yoff) {
			NE::InputManager::OnScroll(xoff, yoff);
			});
		glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int c) {
			NE::InputManager::OnCharInput((uint32_t)c);
			});

		NE::LoadStartupScene();

		SPD_INFO("=== Application initialization complete ===");
		SPD_DEBUG("All panels loaded successfully");
		SPD_INFO("Ready for user interaction");
	}

	void Application::Run() {
		while (!NE::WindowShouldClose()) {
			Profiler::BeginFrame();
			timer.Update();

			NE::InputManager::BeginFrame();
			glfwPollEvents();

			NE::Run(timer.GetDeltaTime());

			glfwSwapBuffers(static_cast<GLFWwindow*>(NE::GetNativeWindowHandle()));

			Profiler::EndFrame();
		}
	}

	void Application::Exit() {
		NE::Shutdown();
	}
}