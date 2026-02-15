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
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

// Forward declarations
namespace NE::Graphics {
    class IGeometryBuffer;
    class Material;
}

namespace NE::ECS::Systems {

    class UIRenderSystem final : public System {
    public:
        //=================================================================
        // Public Structures
        //=================================================================

        struct AccumulatedTransform {
            float posX = 0.f;
            float posY = 0.f;
            float posZ = 0.f;
            float scaleX = 1.f;
            float scaleY = 1.f;
            float scaleZ = 1.f;
            float rotationX = 0.f;
            float rotationY = 0.f;
            float rotationZ = 0.f;
        };

        struct WorldTransform {
            float x = 0.f;
            float y = 0.f;
            float z = 0.f;
            float width = 0.f;
            float height = 0.f;
            float accumulatedRotationZ = 0.f;
            float accumulatedScaleX = 1.f;
            float accumulatedScaleY = 1.f;
        };

        //=================================================================
        // Lifecycle
        //=================================================================

        explicit UIRenderSystem(ComponentManager* cm);

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
        // Transform Hierarchy Functions
        //=================================================================

        AccumulatedTransform AccumulateParentTransforms(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas
        );

        // NEW: Build world matrix from accumulated transforms
        void UpdateWorldMatrix(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas
        );

        // Overload that accepts pre-computed AccumulatedTransform (avoids re-traversal)
        void UpdateWorldMatrixFromAccumulated(
            Entity entity,
            const AccumulatedTransform& accumulated
        );

        std::vector<Entity> BuildParentChain(
            Entity entity,
            Entity canvasEntity,
            Component::UICanvas::RenderMode renderMode
        );

        //=================================================================
        // World Transform Calculation
        //=================================================================

        WorldTransform CalculateWorldTransform(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas,
            const Math::Mat4* viewMatrix = nullptr,
            const Math::Mat4* projMatrix = nullptr
        );

        // Overload that accepts pre-computed AccumulatedTransform (avoids re-traversal)
        WorldTransform CalculateWorldTransformFromAccumulated(
            Entity entity,
            const Component::UICanvas& canvas,
            const AccumulatedTransform& accumulated
        );

        void ApplyPixelPerfectSnapping(WorldTransform& transform);

        //=================================================================
        // Canvas & Scaling
        //=================================================================

        float CalculateScaleFactor(const Component::UICanvas& canvas);

        //=================================================================
        // Rendering
        //=================================================================

        // NEW: Create dynamic geometry buffer for UI vertices
        std::shared_ptr<NE::Graphics::IGeometryBuffer> CreateDynamicUIGeometry(
            const std::vector<NE::Graphics::UIVertex2>& vertices
        );

        // NEW: Submit UI element through integrated GraphicsManager pipeline
        void SubmitUIElement(
            Entity entity,
            const Component::UICanvas& canvas,
            const Component::UIImage& img,
            const Component::UIRectTransform& rect,
            const std::vector<NE::Graphics::UIVertex2>& vertices
        );

        void RenderCanvasChildren(
            Entity canvasEntity,
            const Component::UICanvas& canvas,
            const Math::Mat4* viewMatrix = nullptr,
            const Math::Mat4* projMatrix = nullptr
        );

        std::vector<Entity> CollectCanvasChildren(Entity canvasEntity);

        void SortEntitiesByZOrder(std::vector<Entity>& entities);

        std::vector<NE::Graphics::UIVertex> GenerateScreenSpaceVertices(
            Entity entity,
            const WorldTransform& worldTransform,
            const Component::UIImage& img
        );

        std::vector<NE::Graphics::UIVertex> GenerateWorldSpaceVertices(
            const Component::UIImage& img
        );

        Math::Mat4 BuildWorldSpaceModelMatrix(
            Entity entity,
            Entity canvasEntity,
            const Component::UIRectTransform& rect,
            const AccumulatedTransform& accumulated
        );

        //=================================================================
        // Vertex Manipulation
        //=================================================================

        void RotateVertices2D(
            std::vector<NE::Graphics::UIVertex>& vertices,
            float pivotX,
            float pivotY,
            float rotationDegrees
        );

        //=================================================================
        // Camera Utilities
        //=================================================================

        bool GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj);

        //=================================================================
        // Text Rendering
        //=================================================================

        std::vector<Entity> CollectTextChildren(Entity canvasEntity);

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

        // NEW: Submit text element through integrated GraphicsManager pipeline
        void SubmitTextElement(
            Entity entity,
            const Component::UICanvas& canvas,
            const Component::UIText& text,
            const Component::UIRectTransform& rect,
            const std::vector<NE::Graphics::UIVertex2>& vertices,
            std::shared_ptr<NE::Graphics::FontAtlas> fontAtlas
        );

    private:
        ComponentManager* m_cm;

        // Integrated pipeline materials
        std::shared_ptr<NE::Graphics::Material> m_defaultUIMaterial;  // Default sprite material
        std::shared_ptr<NE::Graphics::Material> m_defaultTextMaterial;  // Default text material

        // Material instance pools (avoids per-frame allocation)
        std::vector<std::shared_ptr<NE::Graphics::Material>> m_spriteMaterialPool;
        std::vector<std::shared_ptr<NE::Graphics::Material>> m_textMaterialPool;
        size_t m_spriteMaterialIndex = 0;
        size_t m_textMaterialIndex = 0;

        std::shared_ptr<NE::Graphics::Material> AcquireSpriteMaterial();
        std::shared_ptr<NE::Graphics::Material> AcquireTextMaterial();

        // Geometry buffer pool (avoids per-frame VAO/VBO/EBO allocation + fixes GL object leak)
        std::vector<std::shared_ptr<NE::Graphics::IGeometryBuffer>> m_geometryPool;
        size_t m_geometryIndex = 0;

        std::shared_ptr<NE::Graphics::IGeometryBuffer> AcquireGeometryBuffer(
            const std::vector<NE::Graphics::UIVertex2>& vertices,
            const std::vector<uint32_t>& indices
        );
    };

} // namespace NE::ECS::Systems

#endif // UI_RENDER_SYSTEM_HPP