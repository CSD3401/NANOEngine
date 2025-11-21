#ifndef UI_IMAGE_HPP
#define UI_IMAGE_HPP

#include <string>
#include <memory>
#include "../../Graphics/Core/Material.hpp"
#include "../../Math/Vec4.hpp"

namespace NE::ECS::Component {

    struct UIImage {
        std::string luid;
        std::string textureUUID;
        std::shared_ptr<NE::Graphics::Material> material;
        NE::Math::Vec4 color{ 1.f, 1.f, 1.f, 1.f }; // tint
        int renderMode = 0;
        bool isDirty = false;
    };

} // namespace NE::ECS::Component
#endif // END UI_IMAGE_HPP