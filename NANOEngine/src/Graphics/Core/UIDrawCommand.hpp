#ifndef UI_DRAW_COMMAND_HPP
#define UI_DRAW_COMMAND_HPP

#include <memory>
#include "../../Math/Vec4.hpp"
#include "Material.hpp"

namespace NE::Graphics {

    class UIDrawCommand {
    public:
        float x, y;          // top-left in pixels
        float width, height; // in pixels
        std::shared_ptr<Material> material;
        Math::Vec4 color{ 1.0f ,1.0f ,1.0f ,1.0f };
        int order = 0;
    };

} // namespace NE::Graphics
#endif // END UI_DRAW_COMMAND_HPP