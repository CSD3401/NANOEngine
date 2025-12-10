#ifndef UI_TRANSFORM_SYSTEM_HPP
#define UI_TRANSFORM_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../src/Math/Mat4.hpp"
#include <unordered_map>
#include <vector>

namespace NE::ECS::Systems {

    class UITransformSystem : public System {
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

        UITransformSystem(ComponentManager* cm);

        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        void Init() override;
        void Update(double dt) override;
        void Exit() override;

        //=================================================================
        // Parent Management
        //=================================================================

        void SetParent(Entity child, Entity newParent);
        Entity GetEntityFromLUID(uint64_t luid) const;

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

        bool ShouldIncludeCanvasTransform(Component::UICanvas::RenderMode renderMode);

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

        void ApplyPixelPerfectSnapping(WorldTransform& transform);

        //=================================================================
        // Model Matrix Building
        //=================================================================

        Math::Mat4 BuildWorldSpaceModelMatrix(
            Entity entity,
            Entity canvasEntity,
            const Component::UIRectTransform& rect,
            const AccumulatedTransform& accumulated
        );

        //=================================================================
        // Canvas Setup
        //=================================================================

        void SetupCanvasDefaults(Entity canvasEntity, Component::UICanvas& canvas);

        //=================================================================
        // Canvas & Scaling
        //=================================================================

        float CalculateScaleFactor(const Component::UICanvas& canvas);

    private:
        struct PendingParent {
            Entity child;
            uint64_t parentLuid;
        };

        ComponentManager* m_cm;
        std::unordered_map<uint64_t, Entity> m_luidToEntity;
        std::vector<PendingParent> m_pendingParents;

        void ResolvePendingParents();
        void UpdateWorldTransforms(); // kiv
    };

} // namespace NE::ECS::Systems

#endif // UI_TRANSFORM_SYSTEM_HPP
