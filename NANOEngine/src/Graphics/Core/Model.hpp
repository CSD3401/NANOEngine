#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "Vertex.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "ResourceManagement/IResource.hpp"
#include "ResourceManagement/BinaryView.hpp"

namespace NE::Graphics {
    struct AABB {
        Math::Vec3 min{ 0,0,0 };
        Math::Vec3 max{ 0,0,0 };
    };

    struct Sphere {
        Math::Vec3 center{ 0,0,0 };
        float radius = 0.0f;
    };

    struct SubMesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::shared_ptr<IGeometryBuffer> buffer;

        AABB   localAABB;
        Sphere localSphere;
    };

    class Model : public Resource::IResource {
    public:
        std::vector<SubMesh> meshes;

        bool Preload(Resource::BinaryView blob) override;
        void Finalize() override;

        bool GetPhysicsMesh(std::vector<Math::Vec3>& outVerts,
            std::vector<uint32_t>& outIndices) const;

        static constexpr Resource::ResourceType GetStaticType() { return Resource::ResourceType::Model; }
        Resource::ResourceType GetType() const override { return GetStaticType(); }

    private:
        struct StagedSubmesh {
            const uint8_t* vdata = nullptr;
            uint32_t       vertexCount = 0;
            const uint8_t* idata = nullptr;
            uint32_t       indexCount = 0;

            AABB   localAABB;
            Sphere localSphere;
        };
        std::vector<StagedSubmesh> m_staged;

    };
}