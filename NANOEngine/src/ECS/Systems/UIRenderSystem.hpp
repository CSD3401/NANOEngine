#ifndef UI_RENDER_SYSTEM_HPP
#define UI_RENDER_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
#include "../Components/UIText.hpp"
#include "../src/Math/Mat4.hpp"
#include "../src/Math/Vec2.hpp"
#include "../src/Math/Vec3.hpp"
#include "../../Graphics/Core/UIImageMeshGenerator.hpp"
#include "../../Graphics/Core/FontAtlas.hpp"
#include "../../Graphics/Core/DrawCommand.hpp"
#include "UILayoutEngine.hpp"
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <optional>
#include <cstring>

// Forward declarations
namespace NE::Graphics {
    class IGeometryBuffer;
    class Material;
}

namespace NE::ECS {
	class ComponentManager;
	class EntityManager;
}

namespace NE::ECS::Systems {

    // Batch key for grouping UI elements into single draw calls
    struct UIBatchKey {
        bool isText;
        bool isWorldSpace;
        bool enableDepthTest;
        std::optional<NE::Graphics::ScissorRect> scissorRect;
        float sortingOrder;

        bool operator<(const UIBatchKey& other) const {
            if (sortingOrder != other.sortingOrder) return sortingOrder < other.sortingOrder;
            if (isText != other.isText) return isText < other.isText;
            if (isWorldSpace != other.isWorldSpace) return isWorldSpace < other.isWorldSpace;
            if (enableDepthTest != other.enableDepthTest) return enableDepthTest < other.enableDepthTest;

            // Compare scissor rects field-by-field
            // If both are nullopt, they are equal (fall through to return false at end)
            // If one is nullopt and the other is not, nullopt < has_value
            if (scissorRect.has_value() != other.scissorRect.has_value()) {
                return !scissorRect.has_value();  // nullopt comes first
            }
            // Both have values, compare fields
            if (scissorRect.has_value()) {
                if (scissorRect->x != other.scissorRect->x) return scissorRect->x < other.scissorRect->x;
                if (scissorRect->y != other.scissorRect->y) return scissorRect->y < other.scissorRect->y;
                if (scissorRect->width != other.scissorRect->width) return scissorRect->width < other.scissorRect->width;
                if (scissorRect->height != other.scissorRect->height) return scissorRect->height < other.scissorRect->height;
            }

            return false;  // all fields equal
        }
    };

    // Batch key for WorldSpace sprites: groups by owning canvas entity.
    // All sprites in the same world-space canvas share the same coordinate space,
    // so they can be merged into one draw call per canvas. This avoids float
    // matrix comparisons (memcmp) which broke batching on precision differences.
    struct WorldSpriteBatchKey {
        Entity canvasEntity = NO_ENTITY;

        bool operator<(const WorldSpriteBatchKey& other) const {
            return canvasEntity < other.canvasEntity;
        }
    };

    // Batch data for accumulated vertices and indices
    struct UIBatch {
        std::vector<NE::Graphics::UIVertex2> vertices;
        std::vector<uint32_t> indices;
    };

    // Per-entity text render cache (owned by UIRenderSystem, not the component)
    struct UITextCache {
        uint64_t fontAtlasHandle = 0;
        std::vector<NE::Graphics::UIVertex2> cachedVertices;
        std::string cachedText;
        float cachedFontSize = 0.0f;
        NE::Math::Vec3 cachedPos{};
        NE::Math::Vec2 cachedSize{};
        float cachedRotZ = 0.0f;
        bool hasCachedTransform = false;
    };

    class UIRenderSystem final : public System {
    public:
        // Re-export types from UILayoutEngine for backward compatibility
        using AccumulatedTransform = UILayoutEngine::AccumulatedTransform;
        using WorldTransform = UILayoutEngine::WorldTransform;

        //=================================================================
        // Lifecycle
        //=================================================================

        explicit UIRenderSystem(ComponentManager* cm, EntityManager* em);

        void SetLayoutEngine(UILayoutEngine* engine) { m_layoutEngine = engine; }

        // Batching diagnostics
        int GetFrameUIElements() const { return m_frameUIElements; }
        int GetFrameDrawCalls() const { return m_frameDrawCalls; }
        int GetFrameSpriteBatches() const { return m_frameSpriteBatches; }
        int GetFrameTextBatches() const { return m_frameTextBatches; }

        bool IsActiveForUI(Entity e, Entity canvasEntity) const;

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;
        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;
        void OnEntityActive(Entity entity) override;
        void OnEntityInactive(Entity entity) override;

    private:

        //=================================================================
        // Canvas Setup
        //=================================================================

        void SetupCanvasDefaults(Entity canvasEntity, Component::UICanvas& canvas);

        //=================================================================
        // Rendering
        //=================================================================

        // NEW: Create dynamic geometry buffer for UI vertices
        std::shared_ptr<NE::Graphics::IGeometryBuffer> CreateDynamicUIGeometry(
            const std::vector<NE::Graphics::UIVertex2>& vertices
        );

        // Submit UI element through integrated GraphicsManager pipeline
        void SubmitUIElement(
            Entity entity,
            const Component::UICanvas& canvas,
            const Component::UIImage& img,
            const Component::UIRectTransform& rect,
            const std::vector<NE::Graphics::UIVertex2>& vertices,
            const std::optional<NE::Graphics::ScissorRect>& scissor = std::nullopt
        );

        void RenderCanvasChildren(
            Entity canvasEntity,
            const Component::UICanvas& canvas,
            const Math::Mat4* viewMatrix = nullptr,
            const Math::Mat4* projMatrix = nullptr
        );

        void RenderCanvasTextChildren(
            Entity canvasEntity,
            const Component::UICanvas& canvas
        );

        void SortEntitiesByZOrder(std::vector<Entity>& entities);

        //=================================================================
        // Canvas Children Collection (single-pass)
        //=================================================================

        struct CanvasChildren {
            std::vector<Entity> images;
            std::vector<Entity> texts;
            // Pre-computed mask ancestors per entity (closest to farthest, for scissor intersection).
            // Populated top-down during CollectChildrenInOrder.
            std::unordered_map<Entity, std::vector<Entity>> maskAncestors;
        };

        // Built once per frame in Update(), keyed by canvas entity
        std::unordered_map<Entity, CanvasChildren> m_canvasChildrenMap;

        // Build canvas children map by walking each canvas's hierarchy in sibling order
        void BuildCanvasChildrenMap();
        void CollectChildrenInOrder(Entity canvasEntity, Entity node, CanvasChildren& out,
                                     std::vector<Entity> inheritedMasks = {});

        //=================================================================
        // Camera Utilities
        //=================================================================

        bool GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj);

        void RenderTextEntity(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas,
            const Math::Mat4* viewMatrix,
            const Math::Mat4* projMatrix
        );

        // NEW: Create dynamic geometry buffer for text vertices
        std::shared_ptr<NE::Graphics::IGeometryBuffer> CreateDynamicTextGeometry(
            const std::vector<NE::Graphics::UIVertex2>& vertices
        );

        // Submit text element through integrated GraphicsManager pipeline
        void SubmitTextElement(
            Entity entity,
            const Component::UICanvas& canvas,
            const Component::UIText& text,
            const Component::UIRectTransform& rect,
            const std::vector<NE::Graphics::UIVertex2>& vertices,
            std::shared_ptr<NE::Graphics::FontAtlas> fontAtlas,
            const std::optional<NE::Graphics::ScissorRect>& scissor = std::nullopt
        );

    private:
        ComponentManager* m_cm;
        EntityManager* m_em;
        UILayoutEngine* m_layoutEngine = nullptr;
        bool m_canvasMapDirty = true;

        // Text render cache keyed by entity (moved off the UIText component)
        std::unordered_map<Entity, UITextCache> m_textCache;

        // Batching diagnostics
        int m_frameDrawCalls = 0;
        int m_frameSpriteBatches = 0;
        int m_frameTextBatches = 0;
        int m_frameUIElements = 0;

        // Shared pipeline materials (one per shader type, reused for all batches)
        std::shared_ptr<NE::Graphics::Material> m_sharedSpriteMaterial;       // neuisprite — only uScreenSize
        std::shared_ptr<NE::Graphics::Material> m_sharedTextMaterial;         // neuitext — only uScreenSize
        std::shared_ptr<NE::Graphics::Material> m_sharedWorldSpriteMaterial;  // neuiworld — uView, uProj
        std::shared_ptr<NE::Graphics::Material> m_sharedWorldTextMaterial;    // neuiworldtext — uView, uProj

        // Camera matrices (stored per frame for WorldSpace rendering)
        Math::Mat4 m_currentView;
        Math::Mat4 m_currentProj;

        // Geometry buffer pool (avoids per-frame VAO/VBO/EBO allocation + fixes GL object leak)
        std::vector<std::shared_ptr<NE::Graphics::IGeometryBuffer>> m_geometryPool;
        size_t m_geometryIndex = 0;
        bool m_geometryPoolCapped = false; // set to true when pool reaches MAX_UI_GEOMETRY_POOL_SIZE

        std::shared_ptr<NE::Graphics::IGeometryBuffer> AcquireGeometryBuffer(
            const std::vector<NE::Graphics::UIVertex2>& vertices,
            const std::vector<uint32_t>& indices
        );

        // Batch rendering helper
        void SubmitBatch(
            const UIBatch& batch,
            std::shared_ptr<NE::Graphics::Material> material,
            const std::optional<NE::Graphics::ScissorRect>& scissor = std::nullopt,
            bool enableDepthTest = false
        );

        // Scissor clipping for RectMask2D
        std::optional<NE::Graphics::ScissorRect> ComputeScissorRect(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas,
            const CanvasChildren& canvasChildren
        );
    };

} // namespace NE::ECS::Systems

#endif // UI_RENDER_SYSTEM_HPP
