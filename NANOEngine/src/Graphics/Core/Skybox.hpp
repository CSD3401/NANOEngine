#pragma once

#include <memory>
#include "Material.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"

namespace NE::Graphics {
	struct RenderView;

    class Skybox {
    public:
        Skybox();

        void Draw(const RenderView& view) const;

        void Submit() const;

    private:
        std::shared_ptr<IGeometryBuffer> m_Mesh;
        std::shared_ptr<Material> m_Material;
    };

}