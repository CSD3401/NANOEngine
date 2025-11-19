#ifndef UI_DRAW_COMMAND_HPP
#define UI_DRAW_COMMAND_HPP

#include "../../Math/Vec4.hpp"
#include "Material.hpp"
#include <memory>

namespace NE::Graphics {

    class UIDrawCommand {
    public:
        float x, y, z;       // top-left in pixels
        float width, height; // in pixels

        std::shared_ptr<Material> material;
        Math::Vec4 color{ 1.0f ,1.0f ,1.0f ,1.0f }; // white

        int order = 0; // kiv
        uint32_t entityId = 0;

        int renderMode = 0;

        float planeDistance = 100.0f; // for camera mode

        Math::Mat4 viewMatrix{};
        Math::Mat4 projMatrix{};
    };

} // namespace NE::Graphics
#endif // END UI_DRAW_COMMAND_HPP