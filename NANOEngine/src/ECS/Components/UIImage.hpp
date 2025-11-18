#ifndef UI_IMAGE_HPP
#define UI_IMAGE_HPP

#include <filesystem>
#include <memory>
#include "../../Graphics/Core/Material.hpp"
#include "../../Math/Vec4.hpp"

namespace NE::ECS::Component {

    struct UIImage {
        // where to load from
        std::filesystem::path texturePath;

        // what we actually draw with
        std::shared_ptr<NE::Graphics::Material> material;

        // tint
        NE::Math::Vec4 color{ 1.f, 1.f, 1.f, 0.f };
    };

} // namespace NE::ECS::Component
#endif // END UI_IMAGE_HPP