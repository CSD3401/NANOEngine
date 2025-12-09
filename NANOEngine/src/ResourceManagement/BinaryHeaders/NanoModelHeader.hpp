#pragma once
#include <cstdint>

namespace NE::Resource {

    inline constexpr uint32_t NMOD_MAGIC = 0x4E4D4F44;
    inline constexpr int CURRENT_NANOMODEL_FORMAT_VERSION = 1;

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
    };
#pragma pack(pop)

}
