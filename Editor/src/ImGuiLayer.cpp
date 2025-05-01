#include "ImGuiLayer.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

namespace Editor {
    void InitImGui(GLFWwindow* window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        //ImGui::GetStyle().TabRounding = 2.0f; // No corner rounding
        //ImGui::GetStyle().TabBorderSize = 0.0f; // No tab border
        //ImGui::GetStyle().FramePadding = ImVec2(4.0f, 2.0f); // Less vertical padding
        //ImGui::GetStyle().ItemSpacing = ImVec2(4.0f, 4.0f); // Less space between items
        ////ImGui::GetStyle().TabBarPadding = ImVec2(0, 0);
        //ImGui::GetStyle().Colors[ImGuiCol_Tab] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);       // Dark gray
        //ImGui::GetStyle().Colors[ImGuiCol_TabActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Pure black when active
        //ImGui::GetStyle().Colors[ImGuiCol_TabHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); // Slightly lighter when hovered
        ImGuiStyle& style = ImGui::GetStyle();
        style.TabRounding = 0.0f;                         // Remove round corners
        style.TabBorderSize = 0.0f;                       // Remove tab outline
        style.ItemInnerSpacing = ImVec2(6, 4);            // Reduce spacing between label and close button
        style.FramePadding = ImVec2(4, 2);                // Shrink tab vertical height

        style.Colors[ImGuiCol_Tab] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);        // Inactive tab
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); // Hovered tab
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Active tab (black)
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        style.WindowMenuButtonPosition = ImGuiDir_None;

        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);     // Dark menu bar
        style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);     // Hover background
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f); // Lighter on hover
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);  // Clicked


        
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 430");
    }

    void ShutdownImGui() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}
