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

    /// Get UICanvas component (const)
    NANOENGINE_API const Component::UICanvas& GetUICanvas(uint32_t e);

    /// Get UIRectTransform component (const)
    NANOENGINE_API const Component::UIRectTransform& GetUIRectTransform(uint32_t e);

    /// Get UIImage component (const)
    NANOENGINE_API const Component::UIImage& GetUIImage(uint32_t e);
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

    /// Add UICanvas component to an entity
    NANOENGINE_API void AddUICanvasComponent(uint32_t e, const Component::UICanvas& c);

    /// Add UIRectTransform component to an entity
    NANOENGINE_API void AddUIRectTransformComponent(uint32_t e, const Component::UIRectTransform& c);

    /// Add UIImage component to an entity
    NANOENGINE_API void AddUIImageComponent(uint32_t e, const Component::UIImage& c);

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
}

} // namespace ECS
} // namespace NE
