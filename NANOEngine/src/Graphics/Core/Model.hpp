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
        std::vector<uint8_t> colliderBlob;
        uint8_t colliderType = 0;

        AABB   localAABB;
        Sphere localSphere;
    };

    class Model : public Resource::IResource {
    public:
        std::vector<SubMesh> meshes;

        bool Preload(Resource::BinaryView blob) override;
        void Finalize() override;

        bool GetSubmeshColliderBlob(uint32_t submeshIndex,
            const uint8_t*& outData,
            uint32_t& outSize,
            uint8_t& outType) const;

        static constexpr Resource::ResourceType GetStaticType() { return Resource::ResourceType::Model; }
        Resource::ResourceType GetType() const override { return GetStaticType(); }

    private:
        struct StagedSubmesh {
            const uint8_t* vdata = nullptr;
            uint32_t vertexCount = 0;

            const uint8_t* idata = nullptr;
            uint32_t indexCount = 0;

            // NEW
            const uint8_t* cdata = nullptr;
            uint32_t colliderSize = 0;
            uint8_t  colliderType = 0;
            uint8_t  vertexFlags = 0;

            AABB   localAABB;
            Sphere localSphere;
        };
        std::vector<StagedSubmesh> m_staged;

    };
}