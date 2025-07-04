#pragma once

#include <memory>
#include "Material.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"

namespace NANOEngine::Graphics {

    class Skybox {
    public:
        Skybox();
        void Draw() const;

    private:
        std::shared_ptr<IGeometryBuffer> m_Mesh;
        std::shared_ptr<Material> m_Material;
    };

}