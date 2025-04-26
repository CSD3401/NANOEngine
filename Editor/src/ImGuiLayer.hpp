#ifndef EDITOR_IMGUILAYER_HPP
#define EDITOR_IMGUILAYER_HPP

struct GLFWwindow;

namespace Editor {
    void InitImGui(GLFWwindow* window);
    void ShutdownImGui();
}

#endif // !EDITOR_IMGUILAYER_HPP