#pragma once
#include <cstdint>

namespace NE::Resource {

    inline constexpr uint32_t NMESH_MAGIC = 0x3;
    inline constexpr int CURRENT_NANOMESH_FORMAT_VERSION = 1;

#pragma pack(push, 1)
    struct NanoMeshHeader {
        uint32_t magic = NMESH_MAGIC;
        uint16_t version = CURRENT_NANOMESH_FORMAT_VERSION;

    };
#pragma pack(pop)

}
