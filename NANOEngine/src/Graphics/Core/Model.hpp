#pragma once

#include <memory>
#include <vector>
#include <string>
#include "Vertex.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "ResourceManagement/IResource.hpp"
#include "ResourceManagement/BinaryView.hpp"

namespace NE::Graphics {

    struct SubMesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::shared_ptr<IGeometryBuffer> buffer;
    };

    class Model : public Resource::IResource {
    public:
        std::vector<SubMesh> meshes;

        //NE::Math::Vec3 sphereCenterLS{ 0,0,0 };
        //float sphereRadiusLS = 0.0f;
        //bool hasSphereBoundsLS = false;

        bool Preload(Resource::BinaryView blob) override;
        void Finalize() override;

        //void ComputeModelSphereBounds();

    private:
        struct StagedSubmesh {
            const uint8_t* vdata = nullptr;
            uint32_t       vertexCount = 0;
            const uint8_t* idata = nullptr;
            uint32_t       indexCount = 0;
        };
        std::vector<StagedSubmesh> m_staged;
    };

}