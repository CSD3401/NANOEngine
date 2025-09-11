#pragma once

#include <memory>
#include <vector>
#include <string>
#include "Vertex.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "../../Asset.hpp"

namespace NE::Graphics {

    struct SubMesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::shared_ptr<IGeometryBuffer> buffer;
    };

    class Model : virtual public Asset::IAsset {
    public:
        std::vector<SubMesh> meshes;

        NE::Math::Vec3 sphereCenterLS{ 0,0,0 };
        float sphereRadiusLS = 0.0f;
        bool hasSphereBoundsLS = false;

        bool LoadFromFile(const std::string& path) override;

        void ComputeModelSphereBounds();
    };

    //std::shared_ptr<Model> LoadModel(const std::string& path);

}