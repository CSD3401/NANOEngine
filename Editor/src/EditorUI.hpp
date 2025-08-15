#include <imgui/imgui.h>
#include <string>
#include <imgui/imgui_internal.h>

namespace NE::Math {
    struct Vec3;
}

namespace Editor {
    // A pretty Vec3 control with color coding and reset buttons
    bool DrawVec3Control(const std::string& label, NE::Math::Vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f);

    // Generic float
    bool DrawFloatControl(const std::string& label, float& value, float step = 0.1f);

    // Generic int
    bool DrawIntControl(const std::string& label, int& value);
    // Checkbox (bool)
    bool DrawCheckbox(const std::string& label, bool& value);

    // String (with buffer size)
    bool DrawStringControl(const std::string& label, std::string& value, size_t bufferSize = 256);

    bool DrawAssetField(const char* label, const std::string& assetPath, const char* buttonLabel = "+", float width = 0, bool* openPopup = nullptr);

    // Enum Combo
    template<typename EnumType>
    bool DrawEnumCombo(const std::string& label, EnumType& value, const char* const* items, int itemsCount)
    {
        int val = static_cast<int>(value);
        bool changed = ImGui::Combo(label.c_str(), &val, items, itemsCount);
        if (changed) value = static_cast<EnumType>(val);
        return changed;
    }

}