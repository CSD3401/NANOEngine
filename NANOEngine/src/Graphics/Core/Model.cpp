#include "pch.h"
#include "Model.hpp"

#include "../OpenGL/GLVertexBuffer.hpp"
#include "../OpenGL/GLIndexBuffer.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"
#include "ResourceManagement/BinaryHeaders/NanoModelHeader.hpp"

namespace {
    struct CookVertex {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;

        float tx, ty, tz;
        float tSign;
    };
}

namespace NE::Graphics {
    bool Model::Preload(Resource::BinaryView blob) {
        if (blob.size < sizeof(Resource::NanoMeshHeader))
            return false;

        const auto* hdr = blob.as<Resource::NanoMeshHeader>(0);
        if (!hdr) return false;
        if (hdr->magic != Resource::NMOD_MAGIC) return false;

        if (hdr->version < 2 || hdr->version > Resource::CURRENT_NANOMODEL_FORMAT_VERSION) return false;

        const size_t subTableOff = sizeof(Resource::NanoMeshHeader);
        const size_t subTableSize = static_cast<size_t>(hdr->submeshCount) * sizeof(Resource::NanoSubmeshDesc);
        if (blob.size < subTableOff + subTableSize)
            return false;

        const auto* subdescs = blob.as<Resource::NanoSubmeshDesc>(subTableOff);
        if (!subdescs) return false;

        m_staged.clear();
        m_staged.reserve(hdr->submeshCount);

        for (uint16_t i = 0; i < hdr->submeshCount; ++i) {
            const auto& d = subdescs[i];

            const size_t vbytes = static_cast<size_t>(d.vertexCount) * sizeof(CookVertex);
            const uint8_t* vptr = blob.at(d.vertexDataOffset, vbytes);
            if (!vptr) return false;

            const size_t ibytes = static_cast<size_t>(d.indexCount) * sizeof(uint32_t);
            const uint8_t* iptr = blob.at(d.indexDataOffset, ibytes);
            if (!iptr) return false;

            StagedSubmesh sm{};
            sm.vdata = vptr;
            sm.vertexCount = d.vertexCount;
            sm.idata = iptr;
            sm.indexCount = d.indexCount;

            if (hdr->version >= 3 && d.colliderDataSize > 0) {
                const uint8_t* cptr = blob.at(d.colliderDataOffset, d.colliderDataSize);
                if (!cptr) return false;

                sm.cdata = cptr;
                sm.colliderSize = d.colliderDataSize;
                sm.colliderType = d.colliderType;
                sm.vertexFlags = d.vertexFlags;
            } else {
                sm.cdata = nullptr;
                sm.colliderSize = 0;
                sm.colliderType = 0;
                sm.vertexFlags = 0;
            }

            if (hdr->version >= 2) {
                sm.localAABB.min = { d.aabbMin[0], d.aabbMin[1], d.aabbMin[2] };
                sm.localAABB.max = { d.aabbMax[0], d.aabbMax[1], d.aabbMax[2] };

                sm.localSphere.radius = d.sphereRadius;
            }

            m_staged.push_back(sm);
        }

        return true;
    }

    void Model::Finalize() {
        meshes.clear();
        meshes.reserve(m_staged.size());

        for (const auto& sm : m_staged) {
            SubMesh sub{};

            const auto* cv = reinterpret_cast<const CookVertex*>(sm.vdata);
            sub.vertices.resize(sm.vertexCount);
            for (uint32_t i = 0; i < sm.vertexCount; ++i) {
                NE::Graphics::Vertex v{};
                v.Position = { cv[i].px, cv[i].py, cv[i].pz };
                v.Normal = { cv[i].nx, cv[i].ny, cv[i].nz };
                v.TexCoord = { cv[i].u,  cv[i].v };
                sub.vertices[i] = v;
            }

            const auto* idx = reinterpret_cast<const uint32_t*>(sm.idata);
            sub.indices.assign(idx, idx + sm.indexCount);

            auto vb = std::make_shared<NE::Graphics::OpenGL::GLVertexBuffer>(
                sub.vertices.data(),
                static_cast<uint32_t>(sub.vertices.size() * sizeof(NE::Graphics::Vertex)),
                sizeof(NE::Graphics::Vertex));

            auto ib = std::make_shared<NE::Graphics::OpenGL::GLIndexBuffer>(
                sub.indices.data(),
                static_cast<uint32_t>(sub.indices.size()));

            sub.buffer = std::make_shared<NE::Graphics::OpenGL::GLGeometryBuffer>(vb, ib);

            if (sm.cdata && sm.colliderSize > 0) {
                sub.colliderBlob.assign(sm.cdata, sm.cdata + sm.colliderSize);
                sub.colliderType = sm.colliderType;
            }

            sub.localAABB = sm.localAABB;
            sub.localSphere = sm.localSphere;

            meshes.push_back(std::move(sub));
        }

        m_staged.clear();
    }

    bool Model::GetSubmeshColliderBlob(uint32_t submeshIndex, const uint8_t*& outData, uint32_t& outSize, uint8_t& outType) const {
        if (submeshIndex >= meshes.size()) return false;
        const SubMesh& sm = meshes[submeshIndex];
        if (sm.colliderBlob.empty()) return false;
        outData = sm.colliderBlob.data();
        outSize = (uint32_t)sm.colliderBlob.size();
        outType = sm.colliderType;
        return true;
    }
}