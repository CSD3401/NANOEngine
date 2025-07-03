#ifndef NANOENGINE_GRAPHICS_WINDOW_HPP
#define NANOENGINE_GRAPHICS_WINDOW_HPP

#include "../../NANOEngineAPI.hpp"

struct GLFWwindow;

namespace NANOEngine::Graphics {

    struct WindowProperties {
        int Width = 1280;
        int Height = 720;
        const char* Title = "NANO Engine";
        bool VSync = true;
    };

    class Window {
    public:
        Window(const WindowProperties& props);
        ~Window();

        void PollEvents();
        void SwapBuffers();
        void SetVSync(bool enabled);
        bool IsVSync() const;
        void* GetNativeWindow() const; // return GLFWwindow*
        bool ShouldClose();

    private:
        void Init(const WindowProperties& props);
        void Shutdown();

        GLFWwindow* m_windowHandle;
        int m_width, m_height;
        bool m_vsync;
    };

}


#endif !NANOENGINE_GRAPHICS_WINDOW_HPP