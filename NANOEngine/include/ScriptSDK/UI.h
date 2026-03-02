/**
 * @file UI.h
 * @brief SDK-level UI component definitions for scripting
 *
 * This header defines UI component structures for script access.
 * These definitions are binary-compatible with the engine's internal UI components.
 *
 * Components provided:
 * - UICanvas: Root container for UI elements with render mode settings
 * - UIRectTransform: Position, size, anchoring for UI elements
 * - UIImage: Image rendering with texture support
 * - UIText: Text rendering with font support
 * - UIButton: Interactive button with state-based colors
 * - UISlider: Draggable value slider
 * - UIToggle: On/off toggle switch
 * - UIInputField: Text input field
 * - UIDropdown: Option selection dropdown
 *
 * Usage in scripts:
 *   #include <ScriptSDK/UI.h>
 *   // or include EngineAPI.hpp which includes this
 *
 * IMPORTANT: For UIImage texture changes, use SetUIImageTexture() rather than
 * modifying textureUUID directly, as the function handles GPU resource loading.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "Math.h"

// NANOENGINE_API macro for DLL import/export
#ifndef NANOENGINE_API
    #ifdef NANOENGINE_EXPORTS
        #define NANOENGINE_API __declspec(dllexport)
    #else
        #define NANOENGINE_API __declspec(dllimport)
    #endif
#endif

namespace NE {
namespace ECS {
namespace Component {

    //=========================================================================
    // UI CANVAS COMPONENT
    //=========================================================================

    /// UICanvas component - Root container for UI elements
    /// Controls how UI is rendered (screen space or world space)
    struct UICanvas {
        enum class RenderMode {
            SCREEN_SPACE_OVERLAY, ///< Always on top, no camera needed
            SCREEN_SPACE_CAMERA,  ///< Rendered by specific camera
            WORLD_SPACE           ///< Exists in 3D world
        };

        enum class ScaleMode {
            CONSTANT_PIXEL_SIZE,     ///< 1:1 pixel mapping
            SCALE_WITH_SCREEN_SIZE,  ///< Scale to match reference resolution
            CONSTANT_PHYSICAL_SIZE   ///< DPI-aware scaling
        };

        // LUID for serialization
        uint64_t luid;

        RenderMode renderMode = RenderMode::SCREEN_SPACE_OVERLAY;
        ScaleMode scaleMode = ScaleMode::SCALE_WITH_SCREEN_SIZE;

        // For Camera mode - distance from camera
        float planeDistance = 100.0f;

        // Reference resolution for scaling
        float referenceWidth = 1920.0f;
        float referenceHeight = 1080.0f;

        bool pixelPerfect = false;
        bool isActive = true;

        // Higher values render on top (layering of canvases)
        int sortingOrder = 0;

        // Runtime only (not serialized)
        float scaleFactor = 1.0f;
        RenderMode lastInitializedMode = RenderMode::SCREEN_SPACE_OVERLAY;
        bool hasBeenInitialized = false;
    };

    //=========================================================================
    // UI RECT TRANSFORM COMPONENT
    //=========================================================================

    /// UIRectTransform component - Layout and positioning for UI elements
    /// Uses anchor/pivot system for responsive layouts
    struct UIRectTransform {
        // Parent entity ID
        uint32_t parent = UINT32_MAX;

        // LUID for serialization
        uint64_t luid = 0;
        uint64_t parentLuid = 0;

        // Position of pivot point (in pixels, relative to anchor)
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;  ///< Z-order for layering within canvas

        // Size in pixels
        float width = 100.0f;
        float height = 100.0f;

        // Offsets (used when anchors are stretched)
        float offsetMinX = 0.0f;  ///< Left offset
        float offsetMinY = 0.0f;  ///< Bottom offset
        float offsetMaxX = 0.0f;  ///< Right offset
        float offsetMaxY = 0.0f;  ///< Top offset

        // Rotation in degrees (primarily for world space)
        float rotationX = 0.0f;
        float rotationY = 0.0f;
        float rotationZ = 0.0f;

        // Scale multipliers
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;

        // Anchor min/max (normalized 0-1, relative to parent)
        float anchorMinX = 0.5f;
        float anchorMinY = 0.5f;
        float anchorMaxX = 0.5f;
        float anchorMaxY = 0.5f;

        // Pivot point (normalized 0-1, rotation/scale origin)
        float pivotX = 0.5f;
        float pivotY = 0.5f;

        // Helper functions
        bool IsStretchedX() const { return anchorMinX != anchorMaxX; }
        bool IsStretchedY() const { return anchorMinY != anchorMaxY; }
    };

    //=========================================================================
    // UI IMAGE COMPONENT
    //=========================================================================

    /// UIImage component - Renders textures/sprites in UI
    /// Supports simple, sliced (9-slice), tiled, and filled image types
    ///
    /// NOTE: To change textures from scripts, use Command::SetUIImageTexture()
    /// rather than modifying textureUUID directly, as the function handles
    /// GPU resource loading.
    struct UIImage {
        enum class ImageType {
            SIMPLE,   ///< Standard image, no special behavior
            SLICED,   ///< 9-slice scaling (borders stay same size)
            TILED,    ///< Texture repeats to fill area
            FILLED    ///< Progressive fill (for progress bars, etc.)
        };

        enum class FillMethod {
            HORIZONTAL,   ///< Fill left-to-right or right-to-left
            VERTICAL,     ///< Fill bottom-to-top or top-to-bottom
            RADIAL_90,    ///< Fill in 90 degree arc
            RADIAL_180,   ///< Fill in 180 degree arc
            RADIAL_360    ///< Fill in full circle
        };

        enum class FillOrigin {
            // For horizontal
            LEFT = 0,
            RIGHT = 1,
            // For vertical
            BOTTOM = 0,
            TOP = 1,
            // For radial
            BOTTOM_RADIAL = 0,
            RIGHT_RADIAL = 1,
            TOP_RADIAL = 2,
            LEFT_RADIAL = 3
        };

        // === SERIALIZED FIELDS ===
        uint64_t luid;
        std::string textureUUID;   ///< UUID of texture asset
        std::string materialUUID;  ///< UUID of material (optional)
        Math::Vec4 color{ 1.f, 1.f, 1.f, 1.f };  ///< RGBA tint color (0-1 range)

        ImageType imageType = ImageType::SIMPLE;
        FillMethod fillMethod = FillMethod::HORIZONTAL;
        FillOrigin fillOrigin = FillOrigin::LEFT;

        float fillAmount = 1.0f;     ///< 0.0 = empty, 1.0 = full
        bool fillClockwise = true;   ///< For radial fills
        bool preserveAspect = false; ///< Maintain texture aspect ratio

        // 9-slice border sizes (in pixels)
        float borderLeft = 0.0f;
        float borderRight = 0.0f;
        float borderTop = 0.0f;
        float borderBottom = 0.0f;

        // For tiled images
        float pixelsPerUnitMultiplier = 1.0f;

        // === RUNTIME-ONLY FIELDS (managed by engine) ===
        uint64_t bindlessHandle = 0;  ///< GPU texture handle (internal)

        // Internal fields - DO NOT ACCESS DIRECTLY
        // These exist only to maintain binary layout compatibility with engine
        void* _internal_material_ptr[2] = { nullptr, nullptr };  ///< shared_ptr placeholder (16 bytes on x64)
        bool _internal_isDirty = false;  ///< Engine internal flag

        int renderMode = 0;  ///< Inherited from canvas (internal)
    };

    //=========================================================================
    // UI TEXT COMPONENT
    //=========================================================================

    /// UIText component - Renders text in the UI
    struct UIText {
        enum class Alignment { LEFT, CENTER, RIGHT };
        enum class VerticalAlignment { TOP, MIDDLE, BOTTOM };

        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;
        std::string text = "New Text";
        std::string fontUUID;
        float fontSize = 16.0f;
        Math::Vec4 color{ 0.0f, 0.0f, 0.0f, 1.0f };
        Alignment horizontalAlign = Alignment::LEFT;
        VerticalAlignment verticalAlign = VerticalAlignment::TOP;
        bool wordWrap = false;
        bool autoScale = false;
        float minFontSize = 10.0f;
        float maxFontSize = 100.0f;

        // === RUNTIME FIELDS (managed by engine — do not modify) ===
        bool isDirty = true;
        // Written by UIRenderSystem for UIAutoSize layout; do not modify directly
        float _cachedSizeX = 0.0f;
        float _cachedSizeY = 0.0f;
    };

    //=========================================================================
    // UI BUTTON COMPONENT
    //=========================================================================

    /// UIButton component - Clickable button with state-based visual feedback
    struct UIButton {
        enum class State { NORMAL, HOVERED, PRESSED, DISABLED };

        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;
        Math::Vec4 normalColor{ 0.8f, 0.8f, 0.8f, 1.0f };
        Math::Vec4 hoverColor{ 0.9f, 0.9f, 0.9f, 1.0f };
        Math::Vec4 pressedColor{ 0.6f, 0.6f, 0.6f, 1.0f };
        Math::Vec4 disabledColor{ 0.5f, 0.5f, 0.5f, 0.5f };
        uint32_t onClickEventId = 0;
        bool interactable = true;

        // === RUNTIME FIELDS (managed by engine) ===
        State currentState = State::NORMAL;
        bool wasClicked = false;        ///< True for one frame after click is released
        State previousState = State::NORMAL;
    };

    //=========================================================================
    // UI SLIDER COMPONENT
    //=========================================================================

    /// UISlider component - Draggable slider for numeric values
    struct UISlider {
        enum class Direction { LEFT_TO_RIGHT, RIGHT_TO_LEFT, BOTTOM_TO_TOP, TOP_TO_BOTTOM };

        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;
        float value = 0.0f;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        bool wholeNumbers = false;
        Direction direction = Direction::LEFT_TO_RIGHT;
        uint32_t fillRect = UINT32_MAX;
        uint32_t handleRect = UINT32_MAX;
        uint32_t backgroundRect = UINT32_MAX;
        bool interactable = true;

        // === RUNTIME FIELDS (managed by engine) ===
        bool isDragging = false;
        bool valueChanged = false;      ///< True for one frame when value changes
        float previousValue = 0.0f;

        // Helper functions
        float GetNormalizedValue() const {
            if (maxValue <= minValue) return 0.0f;
            return (value - minValue) / (maxValue - minValue);
        }
        void SetNormalizedValue(float normalized) {
            if (normalized < 0.0f) normalized = 0.0f;
            if (normalized > 1.0f) normalized = 1.0f;
            value = minValue + normalized * (maxValue - minValue);
            if (value < minValue) value = minValue;
            if (value > maxValue) value = maxValue;
        }
        bool IsHorizontal() const {
            return direction == Direction::LEFT_TO_RIGHT || direction == Direction::RIGHT_TO_LEFT;
        }
        bool IsReversed() const {
            return direction == Direction::RIGHT_TO_LEFT || direction == Direction::TOP_TO_BOTTOM;
        }
    };

    //=========================================================================
    // UI TOGGLE COMPONENT
    //=========================================================================

    /// UIToggle component - Boolean on/off toggle switch
    struct UIToggle {
        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;
        bool isOn = false;
        uint32_t graphic = UINT32_MAX;      ///< Checkmark graphic entity
        uint32_t background = UINT32_MAX;
        bool interactable = true;
        uint32_t toggleGroup = 0;           ///< 0 = no group, same value = radio group

        // === RUNTIME FIELDS (managed by engine) ===
        bool valueChanged = false;          ///< True for one frame when value changes
        bool previousValue = false;
        bool wasClicked = false;
    };

    //=========================================================================
    // UI INPUT FIELD COMPONENT
    //=========================================================================

    /// UIInputField component - Single/multi-line text input
    struct UIInputField {
        enum class ContentType { STANDARD, INTEGER, DECIMAL, ALPHA_NUMERIC, PASSWORD };
        enum class LineType { SINGLE_LINE, MULTI_LINE };

        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;
        std::string text;
        std::string placeholderText = "Enter text...";
        ContentType contentType = ContentType::STANDARD;
        LineType lineType = LineType::SINGLE_LINE;
        int characterLimit = 0;     ///< 0 = no limit
        char passwordChar = '*';
        bool interactable = true;
        bool readOnly = false;
        Math::Vec4 normalColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        Math::Vec4 selectedColor{ 0.88f, 0.88f, 1.0f, 1.0f };
        Math::Vec4 disabledColor{ 0.7f, 0.7f, 0.7f, 0.5f };
        Math::Vec4 textColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        Math::Vec4 placeholderColor{ 0.5f, 0.5f, 0.5f, 1.0f };
        Math::Vec4 cursorColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        float cursorWidth = 2.0f;
        float cursorBlinkRate = 0.53f;
        Math::Vec4 selectionColor{ 0.24f, 0.47f, 0.95f, 0.4f };
        uint32_t onValueChangedEventId = 0;
        uint32_t onSubmitEventId = 0;

        // === RUNTIME FIELDS (managed by engine) ===
        int cursorPosition = 0;
        int selectionStart = -1;
        int selectionEnd = -1;
        bool isFocused = false;
        float _cursorBlinkTimer = 0.0f;
        bool _cursorVisible = true;
        std::string _previousText;
    };

    //=========================================================================
    // UI DROPDOWN COMPONENT
    //=========================================================================

    /// UIDropdown component - Expandable option selection list
    struct UIDropdown {
        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;
        std::vector<std::string> options{ "Option A", "Option B", "Option C" };
        int selectedIndex = 0;
        uint32_t captionTextEntity = UINT32_MAX;    ///< UIText showing selected option
        uint32_t optionsPanelEntity = UINT32_MAX;   ///< Panel container for option list
        bool interactable = true;
        Math::Vec4 normalColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        Math::Vec4 highlightedColor{ 0.92f, 0.92f, 0.92f, 1.0f };
        Math::Vec4 pressedColor{ 0.78f, 0.78f, 0.78f, 1.0f };
        Math::Vec4 disabledColor{ 0.7f, 0.7f, 0.7f, 0.5f };
        Math::Vec4 optionNormalColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        Math::Vec4 optionHighlightedColor{ 0.85f, 0.85f, 1.0f, 1.0f };
        uint32_t onValueChangedEventId = 0;

        // === RUNTIME FIELDS (managed by engine) ===
        bool isExpanded = false;
        int hoveredOptionIndex = -1;
        int previousSelectedIndex = 0;
    };

} // namespace Component

//=========================================================================
// UI QUERY NAMESPACE - Read-only access
//=========================================================================

namespace Query {
    /// Check if entity has UICanvas component
    NANOENGINE_API bool HasUICanvas(uint32_t e);

    /// Check if entity has UIRectTransform component
    NANOENGINE_API bool HasUIRectTransform(uint32_t e);

    /// Check if entity has UIImage component
    NANOENGINE_API bool HasUIImage(uint32_t e);

    /// Check if entity has UIText component
    NANOENGINE_API bool HasUIText(uint32_t e);

    /// Check if entity has UIButton component
    NANOENGINE_API bool HasUIButton(uint32_t e);

    /// Check if entity has UISlider component
    NANOENGINE_API bool HasUISlider(uint32_t e);

    /// Check if entity has UIToggle component
    NANOENGINE_API bool HasUIToggle(uint32_t e);

    /// Check if entity has UIInputField component
    NANOENGINE_API bool HasUIInputField(uint32_t e);

    /// Check if entity has UIDropdown component
    NANOENGINE_API bool HasUIDropdown(uint32_t e);

    /// Get UICanvas component (const)
    NANOENGINE_API const Component::UICanvas& GetUICanvas(uint32_t e);

    /// Get UIRectTransform component (const)
    NANOENGINE_API const Component::UIRectTransform& GetUIRectTransform(uint32_t e);

    /// Get UIImage component (const)
    NANOENGINE_API const Component::UIImage& GetUIImage(uint32_t e);

    /// Get UIText component (const)
    NANOENGINE_API const Component::UIText& GetUIText(uint32_t e);

    /// Get UIButton component (const)
    NANOENGINE_API const Component::UIButton& GetUIButton(uint32_t e);

    /// Get UISlider component (const)
    NANOENGINE_API const Component::UISlider& GetUISlider(uint32_t e);

    /// Get UIToggle component (const)
    NANOENGINE_API const Component::UIToggle& GetUIToggle(uint32_t e);

    /// Get UIInputField component (const)
    NANOENGINE_API const Component::UIInputField& GetUIInputField(uint32_t e);

    /// Get UIDropdown component (const)
    NANOENGINE_API const Component::UIDropdown& GetUIDropdown(uint32_t e);
}

//=========================================================================
// UI COMMAND NAMESPACE - Mutable access and creation
//=========================================================================

namespace Command {
    /// Create a new UI Canvas entity
    /// @return Entity ID of the new canvas
    NANOENGINE_API uint32_t CreateUICanvasEntity();

    /// Create a new UI Image entity under a canvas
    /// @param parentCanvas The parent canvas entity ID
    /// @return Entity ID of the new image
    NANOENGINE_API uint32_t CreateUIImageEntity(uint32_t parentCanvas);

    /// Get mutable UICanvas component
    NANOENGINE_API Component::UICanvas& GetUICanvas(uint32_t e);

    /// Get mutable UIRectTransform component
    NANOENGINE_API Component::UIRectTransform& GetUIRectTransform(uint32_t e);

    /// Get mutable UIImage component
    NANOENGINE_API Component::UIImage& GetUIImage(uint32_t e);

    /// Get mutable UIText component
    NANOENGINE_API Component::UIText& GetUIText(uint32_t e);

    /// Get mutable UIButton component
    NANOENGINE_API Component::UIButton& GetUIButton(uint32_t e);

    /// Get mutable UISlider component
    NANOENGINE_API Component::UISlider& GetUISlider(uint32_t e);

    /// Get mutable UIToggle component
    NANOENGINE_API Component::UIToggle& GetUIToggle(uint32_t e);

    /// Get mutable UIInputField component
    NANOENGINE_API Component::UIInputField& GetUIInputField(uint32_t e);

    /// Get mutable UIDropdown component
    NANOENGINE_API Component::UIDropdown& GetUIDropdown(uint32_t e);

    /// Add UICanvas component to an entity
    NANOENGINE_API void AddUICanvasComponent(uint32_t e, const Component::UICanvas& c);

    /// Add UIRectTransform component to an entity
    NANOENGINE_API void AddUIRectTransformComponent(uint32_t e, const Component::UIRectTransform& c);

    /// Add UIImage component to an entity
    NANOENGINE_API void AddUIImageComponent(uint32_t e, const Component::UIImage& c);

    /// Add UIText component to an entity
    NANOENGINE_API void AddUITextComponent(uint32_t e, const Component::UIText& c);

    /// Add UIButton component to an entity
    NANOENGINE_API void AddUIButtonComponent(uint32_t e, const Component::UIButton& c);

    /// Add UISlider component to an entity
    NANOENGINE_API void AddUISliderComponent(uint32_t e, const Component::UISlider& c);

    /// Add UIToggle component to an entity
    NANOENGINE_API void AddUIToggleComponent(uint32_t e, const Component::UIToggle& c);

    /// Add UIInputField component to an entity
    NANOENGINE_API void AddUIInputFieldComponent(uint32_t e, const Component::UIInputField& c);

    /// Add UIDropdown component to an entity
    NANOENGINE_API void AddUIDropdownComponent(uint32_t e, const Component::UIDropdown& c);

    //=========================================================================
    // TEXTURE SWAPPING UTILITIES
    //=========================================================================

    /// Swap the texture on a UIImage component
    /// This handles loading the texture and updating the bindless handle
    /// @param imageEntity Entity with UIImage component
    /// @param textureUUID UUID of the new texture asset
    /// @return true if successful, false if entity doesn't have UIImage or texture failed to load
    NANOENGINE_API bool SetUIImageTexture(uint32_t imageEntity, const char* textureUUID);

    /// Swap texture and material on a UIImage component
    /// @param imageEntity Entity with UIImage component
    /// @param textureUUID UUID of the new texture asset
    /// @param materialUUID UUID of the new material asset (can be empty for default)
    /// @return true if successful
    NANOENGINE_API bool SetUIImageTextureAndMaterial(uint32_t imageEntity, const char* textureUUID, const char* materialUUID);

    /// Set the color tint on a UIImage component
    /// @param imageEntity Entity with UIImage component
    /// @param r Red component (0-1)
    /// @param g Green component (0-1)
    /// @param b Blue component (0-1)
    /// @param a Alpha component (0-1)
    NANOENGINE_API void SetUIImageColor(uint32_t imageEntity, float r, float g, float b, float a);

    /// Set the fill amount on a UIImage component (for FILLED image type)
    /// @param imageEntity Entity with UIImage component
    /// @param fillAmount Fill amount (0.0 = empty, 1.0 = full)
    NANOENGINE_API void SetUIImageFillAmount(uint32_t imageEntity, float fillAmount);

    //=========================================================================
    // UITEXT HELPERS
    //=========================================================================

    /// Set the text string on a UIText component
    NANOENGINE_API void SetUIText(uint32_t e, const char* text);

    /// Set the color on a UIText component
    NANOENGINE_API void SetUITextColor(uint32_t e, float r, float g, float b, float a);

    /// Get the text string from a UIText component (returns nullptr if not present)
    NANOENGINE_API const char* GetUITextString(uint32_t e);

    //=========================================================================
    // UIBUTTON HELPERS
    //=========================================================================

    /// Returns true if the button was clicked this frame
    NANOENGINE_API bool WasButtonClicked(uint32_t e);

    /// Returns true if the button is currently hovered
    NANOENGINE_API bool IsButtonHovered(uint32_t e);

    /// Returns true if the button is currently pressed
    NANOENGINE_API bool IsButtonPressed(uint32_t e);

    /// Enable or disable button interactability
    NANOENGINE_API void SetButtonInteractable(uint32_t e, bool interactable);

    /// Returns the button's interactable state
    NANOENGINE_API bool IsButtonInteractable(uint32_t e);

    //=========================================================================
    // UITOGGLE HELPERS
    //=========================================================================

    /// Returns the toggle's current on/off state
    NANOENGINE_API bool IsToggleOn(uint32_t e);

    /// Set the toggle's on/off state
    NANOENGINE_API void SetToggleOn(uint32_t e, bool value);

    /// Returns true if the toggle value changed this frame
    NANOENGINE_API bool ToggleValueChanged(uint32_t e);

    /// Enable or disable toggle interactability
    NANOENGINE_API void SetToggleInteractable(uint32_t e, bool interactable);

    //=========================================================================
    // UISLIDER HELPERS
    //=========================================================================

    /// Get the slider's current value
    NANOENGINE_API float GetSliderValue(uint32_t e);

    /// Set the slider's current value (clamped to min/max)
    NANOENGINE_API void SetSliderValue(uint32_t e, float value);

    /// Get the slider's normalized value (0.0 - 1.0)
    NANOENGINE_API float GetSliderNormalizedValue(uint32_t e);

    /// Set the slider value from a normalized value (0.0 - 1.0)
    NANOENGINE_API void SetSliderNormalizedValue(uint32_t e, float normalized);

    /// Set the slider's min and max value range
    NANOENGINE_API void SetSliderMinMax(uint32_t e, float minVal, float maxVal);

    /// Returns true if the slider value changed this frame
    NANOENGINE_API bool SliderValueChanged(uint32_t e);

    /// Enable or disable slider interactability
    NANOENGINE_API void SetSliderInteractable(uint32_t e, bool interactable);

    //=========================================================================
    // UIINPUTFIELD HELPERS
    //=========================================================================

    /// Get the input field's current text (returns nullptr if not present)
    NANOENGINE_API const char* GetInputFieldText(uint32_t e);

    /// Set the input field's text
    NANOENGINE_API void SetInputFieldText(uint32_t e, const char* text);

    /// Returns true if the input field is currently focused
    NANOENGINE_API bool IsInputFieldFocused(uint32_t e);

    /// Enable or disable input field interactability
    NANOENGINE_API void SetInputFieldInteractable(uint32_t e, bool interactable);

    //=========================================================================
    // UIDROPDOWN HELPERS
    //=========================================================================

    /// Get the currently selected option index
    NANOENGINE_API int GetDropdownSelectedIndex(uint32_t e);

    /// Set the selected option index
    NANOENGINE_API void SetDropdownSelectedIndex(uint32_t e, int index);

    /// Get the number of options in the dropdown
    NANOENGINE_API int GetDropdownOptionCount(uint32_t e);

    /// Enable or disable dropdown interactability
    NANOENGINE_API void SetDropdownInteractable(uint32_t e, bool interactable);
}

} // namespace ECS
} // namespace NE
