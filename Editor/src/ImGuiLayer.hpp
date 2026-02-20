#ifndef EDITOR_IMGUILAYER_HPP
#define EDITOR_IMGUILAYER_HPP

struct GLFWwindow;

namespace Editor {
    void InitImGui(GLFWwindow* window);
    void ShutdownImGui();
    void RebuildFonts(float fontSize);
    void FlushPendingFontRebuild();
    float GetFontSize();
}

#endif // !EDITOR_IMGUILAYER_HPP