#include "EditorUI.hpp"
#include <Math/Vec3.hpp>

namespace Editor {
    // A pretty Vec3 control with color coding and reset buttons
    bool DrawVec3Control(const std::string& label, NANOEngine::Math::Vec3& values, float resetValue, float columnWidth)
    {
        bool changed = false;
        //ImGuiIO& io = ImGui::GetIO();
        //auto boldFont = io.Fonts->Fonts[0]; // Optionally use bold for labels

        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text("%s", label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0,0 });

        float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

        // X
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
        if (ImGui::Button("X", buttonSize)) { values.x = resetValue; changed = true; }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        changed |= ImGui::DragFloat("##X", &values.x, 0.1f); ImGui::PopItemWidth(); ImGui::SameLine();

        // Y
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
        if (ImGui::Button("Y", buttonSize)) { values.y = resetValue; changed = true; }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        changed |= ImGui::DragFloat("##Y", &values.y, 0.1f); ImGui::PopItemWidth(); ImGui::SameLine();

        // Z
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        if (ImGui::Button("Z", buttonSize)) { values.z = resetValue; changed = true; }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        changed |= ImGui::DragFloat("##Z", &values.z, 0.1f); ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::Columns(1);
        ImGui::PopID();
        return changed;
    }

    // Generic float
    bool DrawFloatControl(const std::string& label, float& value, float step)
    {
        bool changed = false;
        ImGui::Text("%s", label.c_str());
        ImGui::SameLine();
        changed = ImGui::DragFloat(("##" + label).c_str(), &value, step);
        return changed;
    }

    // Generic int
    bool DrawIntControl(const std::string& label, int& value)
    {
        bool changed = false;
        ImGui::Text("%s", label.c_str());
        ImGui::SameLine();
        changed = ImGui::DragInt(("##" + label).c_str(), &value);
        return changed;
    }

    // Checkbox (bool)
    bool DrawCheckbox(const std::string& label, bool& value)
    {
        return ImGui::Checkbox(label.c_str(), &value);
    }

    // String (with buffer size)
    bool DrawStringControl(const std::string& label, std::string& value, size_t bufferSize)
    {
        char buffer[256];
        strncpy_s(buffer, value.c_str(), bufferSize);
        buffer[bufferSize - 1] = 0;
        bool changed = ImGui::InputText(label.c_str(), buffer, bufferSize);
        if (changed) value = buffer;
        return changed;
    }

    bool DrawAssetField(const char* label, const std::string& assetPath, const char* buttonLabel, float width, bool* openPopup)
    {
        bool changed = false;

        ImGui::Text("%s", label); ImGui::SameLine();

        ImGui::PushID(label);

        // Draw a box (Child frame) for visual grouping like Unity
        if (width == 0) width = ImGui::GetContentRegionAvail().x;
        ImGui::BeginChild("AssetBox", ImVec2(width, 28), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Text (centered vertically)
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(assetPath.empty() ? "<None>" : assetPath.c_str());
        ImGui::SameLine();

        std::string popupName = std::string("AssetPicker_") + label;

        // Button (on right side)
        float btnWidth = 28.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth - ImGui::GetStyle().FramePadding.x);
        if (ImGui::Button(buttonLabel, ImVec2(btnWidth, 0))) {
            // Open your asset picker here!
            // set a flag or call a callback, depending on your UI
			//std::string assetPickerPopupName = std::string("AssetPicker_") + label;
            //ImGui::OpenPopup(("AssetPicker_" + std::string(label)).c_str());
            //ImGui::OpenPopup("AssetPicker_Model");
            //changed = true; // or open a popup, etc.
            *openPopup = true;
        }

        ImGui::EndChild();

        ImGui::PopID();
        return changed;
    }

}