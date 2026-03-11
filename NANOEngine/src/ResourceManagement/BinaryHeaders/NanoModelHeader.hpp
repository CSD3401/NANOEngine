#pragma once
#include <cstdint>

namespace NE::Resource {

    inline constexpr uint32_t NMOD_MAGIC = 0x4E4D4F44;
    inline constexpr int CURRENT_NANOMODEL_FORMAT_VERSION = 4;

#pragma pack(push, 1)
    struct NanoMeshHeader {
        uint32_t magic = NMOD_MAGIC;
        uint16_t version = CURRENT_NANOMODEL_FORMAT_VERSION;
        uint16_t submeshCount = 0;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct NanoSubmeshDesc {
        uint32_t vertexCount;
        uint32_t indexCount;

        uint32_t vertexDataOffset;
        uint32_t indexDataOffset;

        float aabbMin[3] = { 0,0,0 };
        float aabbMax[3] = { 0,0,0 };

        float sphereRadius = 0.0f;

        uint32_t colliderDataOffset = 0; // 0 => none
        uint32_t colliderDataSize = 0;

        uint8_t  colliderType = 0; // 0 none, 1 triMesh, 2 convexHull, 3 compound etc
        uint8_t  vertexFlags = 0; // bit0 hasTangents, etc
        uint16_t reserved = 0;
    };
#pragma pack(pop)

}
