#include <imgui/imgui.h>
#include <string>
#include <imgui/imgui_internal.h>
#include <functional>
#include <memory>
#include "Graphics/OpenGL/GLTexture.hpp"
#include <Core/Reflection.hpp>

namespace NE::Math {
    struct Vec2;
    struct Vec3;
	struct Vec4;
}

namespace Editor {
    struct HDRColor {
        ImVec4 color;    // base color (0-1)
        float  intensity; // HDR intensity multiplier
    };

    struct FieldUIResult {
        bool changed = false;
        bool activated = false;
        bool active = false;
        bool deactivated_after_edit = false;
    };

    bool BeginPillCombo(const char* id, const char* preview);
    void EndPillCombo();

    // Generic float
    bool DrawFloatControl(const std::string& label, float& value, float step = 0.1f);
    // Generic int
    bool DrawIntControl(const std::string& label, int& value);
    // Checkbox (bool)
    bool DrawCheckbox(const std::string& label, bool& value);

    // String (with buffer size)
    bool DrawStringControl(const std::string& label, std::string& value, size_t bufferSize = 256);

	[[deprecated("Use DrawAssetField with different signature")]]
    bool DrawAssetField(const char* label, const std::string& assetPath, const char* buttonLabel = "+", float width = 0, bool* openPopup = nullptr);

    bool DrawTextureField(
        const char* label,
        const std::shared_ptr<NE::Graphics::OpenGL::GLTexture>& slotTex,
        float previewSize,
        std::function<void(const std::string&)> assignById);

    bool DrawHDRColorField(const char* label, HDRColor& hdr);

    // Enum Combo
    //template<typename EnumType>
    //bool DrawEnumCombo(const std::string& label, EnumType& value, const char* const* items, int itemsCount)
    //{
    //    int val = static_cast<int>(value);
    //    bool changed = ImGui::Combo(label.c_str(), &val, items, itemsCount);
    //    if (changed) value = static_cast<EnumType>(val);
    //    return changed;
    //}

    bool DrawEnumPillCombo(
        const char* label,
        int& currentIndex,
        const char* const* items,
        int itemsCount,
        float rightWidth = 180.0f);

    using AssetDropFn = std::function<void(const ImGuiPayload* payload)>;
    void DrawAssetField(
        const char* label,
        const std::string& assetPath,
        bool rightAligned = false,
        bool* openPopup = nullptr,
        ImVec2 size = ImVec2(0, 0),
        float plusWidth = 28.0f,
        const char* dndPayloadType = nullptr,
        AssetDropFn onDrop = nullptr
    );

    void DrawAssetField(const char* label, const std::string& assetPath, bool* openPopup = nullptr, bool rightAligned = true, ImVec2 size = { 380.f, 0.f }, float plusWidth = 38.f);

    // New Styling
    void ToolTip(const char* text);
    bool DrawFloatSliderWithField(const char* label, float& value, float min, float max, float step, bool rightAligned);
    bool DrawFloatField(const char* label, float& value, float step, bool rightAligned);
    bool DrawIntField(const char* label, int& value, bool rightAligned);
    bool DrawVec3Control(const char* label,
        NE::Math::Vec3& v,
        float label_width,
        float drag_speed_per_pixel = 0.01f,
        float reset_value = 0.0f,
        int precision = 3);

    bool DrawVec2Control(const char* label,
        NE::Math::Vec2& v,
        float label_width,
        float drag_speed_per_pixel = 0.01f,
        float reset_value = 0.0f,
        int precision = 3);

    bool DrawVec4Control(const char* label,
        NE::Math::Vec4& v,
        float label_width,
        float drag_speed_per_pixel = 0.01f,
        float reset_value = 0.0f,
		int precision = 3);
}