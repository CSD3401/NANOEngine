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
#include "../../Graphics/Core/UITextMeshGenerator.hpp"
#include "../../Graphics/Core/Font.hpp"
#include "UITransformSystem.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace NE::ECS::Systems {

    class UIRenderSystem final : public System {
    public:
        //=================================================================
        // Lifecycle
        //=================================================================

        explicit UIRenderSystem(ComponentManager* cm, UITransformSystem* transformSystem = nullptr);

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;
        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        void SetTransformSystem(UITransformSystem* transformSystem);

    private:
        ComponentManager* m_cm = nullptr;
        UITransformSystem* m_transformSystem = nullptr;

        std::unordered_map<uint32_t, std::shared_ptr<NE::Graphics::Font>> m_fontCache;

        //=================================================================
        // Rendering
        //=================================================================

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
            const UITransformSystem::WorldTransform& worldTransform,
            const Component::UIImage& img
        );

        std::vector<NE::Graphics::UIVertex> GenerateWorldSpaceVertices(
            const Component::UIImage& img,
            float width,
            float height
        );


        void SubmitDrawCommand(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas,
            const Component::UIImage& img,
            const Component::UIRectTransform& rect,
            const UITransformSystem::WorldTransform& worldTransform,
            const UITransformSystem::AccumulatedTransform& accumulated,
            std::vector<NE::Graphics::UIVertex>& vertices,
            const Math::Mat4* viewMatrix,
            const Math::Mat4* projMatrix
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
    };

} // namespace NE::ECS::Systems

#endif // UI_RENDER_SYSTEM_HPP