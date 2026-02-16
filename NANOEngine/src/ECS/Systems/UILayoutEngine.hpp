#pragma once

#include "../Core/ComponentManager.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/Hierarchy.hpp"
#include "../../Math/Mat4.hpp"
#include <vector>
#include <cstdint>

namespace NE::ECS {

    class UILayoutEngine {
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

        struct WorldRect {
            float x = 0.f;
            float y = 0.f;
            float width = 0.f;
            float height = 0.f;
        };

        //=================================================================
        // Constructor
        //=================================================================

        explicit UILayoutEngine(ComponentManager* cm);

        //=================================================================
        // Transform Hierarchy Functions
        //=================================================================

        std::vector<Entity> BuildParentChain(
            Entity entity,
            Entity canvasEntity,
            Component::UICanvas::RenderMode renderMode
        );

        AccumulatedTransform AccumulateParentTransforms(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas
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

        WorldTransform CalculateWorldTransformFromAccumulated(
            Entity entity,
            const Component::UICanvas& canvas,
            const AccumulatedTransform& accumulated
        );

        void ApplyPixelPerfectSnapping(WorldTransform& transform);

        //=================================================================
        // World Matrix
        //=================================================================

        void UpdateWorldMatrix(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas
        );

        void UpdateWorldMatrixFromAccumulated(
            Entity entity,
            const AccumulatedTransform& acc
        );

        //=================================================================
        // Canvas & Scaling
        //=================================================================

        float CalculateScaleFactor(const Component::UICanvas& canvas);

        //=================================================================
        // Utility Helpers
        //=================================================================

        Entity FindOwningCanvas(Entity entity) const;

        void ComputeAndCacheWorldRect(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas
        );

        WorldRect GetWorldRect(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas
        );

        WorldRect CalculateContentBounds(Entity entity);

    private:
        ComponentManager* m_cm;
    };

} // namespace NE::ECS
