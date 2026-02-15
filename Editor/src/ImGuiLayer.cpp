#include "ImGuiLayer.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <imgui/widgets/imsearch/imsearch.h>

namespace Editor {
    static float s_fontSize = 15.0f;
    static float s_pendingFontSize = 0.0f; // 0 means no rebuild pending
    void InitImGui(GLFWwindow* window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImSearch::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        //ImGui::StyleColorsDark();



        // testing of visuals of imgui (not final)
        // 
        // 
        //ImGui::GetStyle().TabRounding = 2.0f; // No corner rounding
        //ImGui::GetStyle().TabBorderSize = 0.0f; // No tab border
        //ImGui::GetStyle().FramePadding = ImVec2(4.0f, 2.0f); // Less vertical padding
        //ImGui::GetStyle().ItemSpacing = ImVec2(4.0f, 4.0f); // Less space between items
        ////ImGui::GetStyle().TabBarPadding = ImVec2(0, 0);
        //ImGui::GetStyle().Colors[ImGuiCol_Tab] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);       // Dark gray
        //ImGui::GetStyle().Colors[ImGuiCol_TabActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Pure black when active
        //ImGui::GetStyle().Colors[ImGuiCol_TabHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); // Slightly lighter when hovered
        
        // Previous version
        //ImGuiStyle& style = ImGui::GetStyle();
        //style.TabRounding = 0.0f;                         // Remove round corners
        //style.TabBorderSize = 0.0f;                       // Remove tab outline
        //style.ItemInnerSpacing = ImVec2(6, 4);            // Reduce spacing between label and close button
        //style.FramePadding = ImVec2(4, 2);                // Shrink tab vertical height

        //style.Colors[ImGuiCol_Tab] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);        // Inactive tab
        //style.Colors[ImGuiCol_TabHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); // Hovered tab
        //style.Colors[ImGuiCol_TabActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Active tab (black)
        //style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        //style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        //style.WindowMenuButtonPosition = ImGuiDir_None;

        //style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);     // Dark menu bar
        //style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);     // Hover background
        //style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f); // Lighter on hover
        //style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);  // Clicked
        

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Base colors
        colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

        colors[ImGuiCol_Text] = ImVec4(0.83f, 0.83f, 0.83f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

        // Frames (inputs, buttons)
        colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

        // Buttons
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);

        // Headers (TreeNodes, Selectables)
        colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

        // Tabs
        colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

        // Accent (slider grab, checkmark, resize grip)
        colors[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.35f, 0.70f, 1.00f, 1.00f);

        // Borders
        colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

        // Layout tuning
        //style.WindowRounding = 6.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 6.0f;
        style.TabRounding = 4.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 4.0f;

        style.FramePadding = ImVec2(8, 4);
        style.ItemSpacing = ImVec2(8, 6);
        style.ScrollbarSize = 14.0f;

        io.Fonts->Clear();

        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 2;
        config.PixelSnapH = false;

        ImFont* uiFont = io.Fonts->AddFontFromFileTTF(
            "Library/Fonts/NotoSans-Regular.ttf",
            s_fontSize,
            &config
        );

        io.FontDefault = uiFont;

        ImGui_ImplGlfw_InitForOpenGL(window, false);
        ImGui_ImplOpenGL3_Init("#version 430");
    }

    void RebuildFonts(float fontSize) {
        s_pendingFontSize = fontSize;
    }

    void FlushPendingFontRebuild() {
        if (s_pendingFontSize == 0.0f) return;

        s_fontSize = s_pendingFontSize;
        s_pendingFontSize = 0.0f;

        ImGuiIO& io = ImGui::GetIO();

        ImGui_ImplOpenGL3_DestroyFontsTexture();

        io.Fonts->Clear();

        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 2;
        config.PixelSnapH = false;

        ImFont* uiFont = io.Fonts->AddFontFromFileTTF(
            "Library/Fonts/NotoSans-Regular.ttf",
            s_fontSize,
            &config
        );

        io.FontDefault = uiFont;
        io.Fonts->Build();

        ImGui_ImplOpenGL3_CreateFontsTexture();
    }

    float GetFontSize() {
        return s_fontSize;
    }

    void ShutdownImGui() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImSearch::DestroyContext();
        ImGui::DestroyContext();
    }
}
