#ifndef UI_DRAW_COMMAND_HPP
#define UI_DRAW_COMMAND_HPP

#include "../../Math/Vec4.hpp"
#include "Material.hpp"
#include "UIImageMeshGenerator.hpp"
#include <memory>

namespace NE::Graphics {

    class UIDrawCommand {
    public:
        // position and size
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        // rendering
        Math::Vec4 color{ 1.0f ,1.0f ,1.0f ,1.0f };
        std::shared_ptr<Material> material;
        uint64_t bindlessTextureHandle = 0;

        // layering
        int order = 0; // kiv
        uint32_t entityId = 0;

        // more specific datas
        int renderMode = 0; // 0=overlay, 1=world
        float planeDistance = 100.0f; // deprecated (was for camera mode)
        Math::Mat4 viewMatrix{};
        Math::Mat4 projMatrix{};
        Math::Mat4 modelMatrix{}; // for world mode

        // custom vertex data for complex fills
        std::vector<UIVertex> vertices;
        bool useCustomVertices = false;
    };

} // namespace NE::Graphics
#endif // END UI_DRAW_COMMAND_HPP