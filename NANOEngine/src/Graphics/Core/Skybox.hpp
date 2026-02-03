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
        std::shared_ptr<IPipeline> GetSkyboxPipeline() const;

    private:
        std::shared_ptr<IGeometryBuffer> m_mesh;
        std::shared_ptr<Material> m_material;
    };

}