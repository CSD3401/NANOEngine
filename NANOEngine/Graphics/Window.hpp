#ifndef NANOENGINE_WINDOW_HPP
#define NANOENGINE_WINDOW_HPP

//#include <memory>
//#include <functional>
//#include <GLFW/glfw3.h>
#include "../NANOEngineAPI.hpp"
//
//namespace NANOEngine::Graphics {
//	struct WindowProperties {
//		const char* title;
//		int width;
//		int height;
//		bool fullscreen;
//		std::function<void()> setHints;
//	};
//
//	class NANOENGINE_API Window {
//	public:
//		Window(const WindowProperties& props);
//		//Window(const char* title, int width, int height, bool fullscreen);
//		~Window();
//
//		//void SwapBuffers();
//		//void SetTitle(const char* title);
//		GLFWwindow* GetWindowHandle() const;
//
//	private:
//#pragma warning(push)
//#pragma warning(disable: 4251)
//		struct Impl;
//		std::unique_ptr<Impl> m_impl;
//#pragma warning(pop)
//	};
//}


struct GLFWwindow; // Forward declare

namespace NANOEngine::Graphics {

    class NANOENGINE_API Window {
    public:
        Window();
        ~Window();

        bool Init(GLFWwindow* windowHandle);  // Pass the window after creation
        void Shutdown();

        void PollEvents();
        void SwapBuffers();

        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }
        GLFWwindow* GetNativeWindow() const { return m_WindowHandle; }

        void SetVSync(bool enabled);
        bool IsVSync() const;

    private:
        GLFWwindow* m_WindowHandle;
        int m_Width;
        int m_Height;
        bool m_VSync;
    };

}


#endif