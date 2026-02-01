#pragma once
#include <cstdint>

namespace NE::Resource {

	inline constexpr uint32_t NMAT_MAGIC = 0x4E4D4154;
	inline constexpr int CURRENT_NANOMAT_FORMAT_VERSION = 3;

#pragma pack(push, 1)
	struct NanoMatHeader {
		uint32_t magic = NMAT_MAGIC;
		uint16_t version = CURRENT_NANOMAT_FORMAT_VERSION;

        // Pipeline state
        uint8_t  blendMode = 0;   // bool
        uint8_t  depthTest = 1;   // bool
		uint8_t  depthWrite = 1;  // bool
        uint8_t  reserved = 0;

        uint32_t cullMode = 0;   // GL enum (your int)
        uint32_t polygonMode = 0;   // GL enum (your int)

        // Render queue
        uint32_t renderQueueNameOffset = 0; // absolute file offset
        uint32_t renderQueueNameLen = 0; // bytes (no null terminator)
        int32_t renderQueueOffset = 0;    // added to base

        // Counts + sizes
        uint16_t propCount = 0;   // number of uniform-like properties
        uint16_t texCount = 0;   // reserved for future (texture UUIDs)
        uint32_t shaderNameLen = 0; // bytes

        // Offsets from file start (so layout can evolve)
        uint32_t shaderNameOffset = 0;
        uint32_t propsOffset = 0; // array of PropRecord followed by payloads

        uint8_t  hasShaderUUID = 0;     // 1 if using UUID-based lookup
        uint8_t  reserved2[3] = { 0 };
        uint32_t shaderUUIDOffset = 0;  // offset to 36-byte ASCII UUID string

        uint32_t texTableOffset = 0;    // offset to array<MatTexRecord>
	};
#pragma pack(pop)

    enum MatPropType : uint8_t { INT = 0, FLOAT = 1, VEC3 = 2, MAT4 = 3, HANDLE = 4 /*bindless/uuid later*/ };

#pragma pack(push, 1)
    struct MatPropRecord {
        uint32_t nameLen;     // bytes of UTF-8 name
        uint32_t nameOffset;  // offset to name bytes
        uint8_t  type;        // MatPropType
        uint8_t  count;       // for arrays; 1 for scalars
        uint16_t reserved = 0;
        uint32_t dataOffset;  // offset into payload region
        uint32_t dataSize;    // bytes (4, 12, 64, etc.)
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct MatTexRecord {
        uint32_t nameLen;     // e.g. "u_BaseColorTex"
        uint32_t nameOffset;  // offset to name bytes
        char     uuid[36];    // ASCII UUID (no null terminator)
    };
#pragma pack(pop)

}
