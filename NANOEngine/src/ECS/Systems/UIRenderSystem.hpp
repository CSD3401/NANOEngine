#ifndef UI_RENDER_SYSTEM_HPP
#define UI_RENDER_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
#include "../Components/UIText.hpp"
#include "../src/Math/Mat4.hpp"
#include "../../Graphics/Core/UIImageMeshGenerator.hpp"
#include "../../Graphics/Core/FontAtlas.hpp"
#include "../../Graphics/Core/DrawCommand.hpp"
#include "UILayoutEngine.hpp"
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <unordered_map>

// Forward declarations
namespace NE::Graphics {
    class IGeometryBuffer;
    class Material;
}

namespace NE::ECS::Systems {

    // Batch key for grouping UI elements into single draw calls
    struct UIBatchKey {
        bool isText;
        bool isWorldSpace;
        bool enableDepthTest;
        std::optional<NE::Graphics::ScissorRect> scissorRect;
        int sortingOrder;

        bool operator<(const UIBatchKey& other) const {
            if (sortingOrder != other.sortingOrder) return sortingOrder < other.sortingOrder;
            if (isText != other.isText) return isText < other.isText;
            if (isWorldSpace != other.isWorldSpace) return isWorldSpace < other.isWorldSpace;
            if (enableDepthTest != other.enableDepthTest) return enableDepthTest < other.enableDepthTest;
            // Scissor rects can't be compared directly, so use address for determinism
            return false;
        }
    };

    // Batch data for accumulated vertices and indices
    struct UIBatch {
        std::vector<NE::Graphics::UIVertex2> vertices;
        std::vector<uint32_t> indices;
    };

    class UIRenderSystem final : public System {
    public:
        // Re-export types from UILayoutEngine for backward compatibility
        using AccumulatedTransform = UILayoutEngine::AccumulatedTransform;
        using WorldTransform = UILayoutEngine::WorldTransform;

        //=================================================================
        // Lifecycle
        //=================================================================

        explicit UIRenderSystem(ComponentManager* cm);

        void SetLayoutEngine(UILayoutEngine* engine) { m_layoutEngine = engine; }

        bool IsActiveForUI(Entity e, Entity canvasEntity) const;

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;
        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

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
        };

        // Built once per frame in Update(), keyed by canvas entity
        std::unordered_map<Entity, CanvasChildren> m_canvasChildrenMap;

        // Single O(N) pass to bucket all UI entities by their owning canvas
        void BuildCanvasChildrenMap();

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
        UILayoutEngine* m_layoutEngine = nullptr;

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

        // Geometry buffer pool (avoids per-frame VAO/VBO/EBO allocation + fixes GL object leak)
        std::vector<std::shared_ptr<NE::Graphics::IGeometryBuffer>> m_geometryPool;
        size_t m_geometryIndex = 0;

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
            const Component::UICanvas& canvas
        );
    };

} // namespace NE::ECS::Systems

#endif // UI_RENDER_SYSTEM_HPP
