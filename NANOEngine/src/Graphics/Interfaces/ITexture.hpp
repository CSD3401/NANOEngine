#ifndef NANOENGINE_GRAPHICS_ITEXTURE_HPP
#define NANOENGINE_GRAPHICS_ITEXTURE_HPP

#include <cstdint>
#include "../../Asset.hpp"
#include "ResourceManagement/IResource.hpp"

namespace NE::Graphics {

    class ITexture : public virtual Resource::IResource {
    public:
        virtual ~ITexture() = default;
        virtual uint64_t GetBindlessHandle() const = 0;
        virtual void MakeResident() = 0;
    };

}

#endif // !NANOENGINE_GRAPHICS_ITEXTURE_HPP
