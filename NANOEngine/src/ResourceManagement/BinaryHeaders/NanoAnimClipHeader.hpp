#pragma once
#include <cstdint>

namespace NE::Resource {

	inline constexpr uint32_t NANC_MAGIC = 0x4E414E43;
	inline constexpr int CURRENT_NANOANIMCLIP_FORMAT_VERSION = 1;

#pragma pack(push, 1)
    struct NanoAnimClipHeader {
        uint32_t magic = NANC_MAGIC;
        uint16_t version = CURRENT_NANOANIMCLIP_FORMAT_VERSION;
        uint64_t payloadBytes = 0;
    };

    //enum class NanoAnimValueType : uint8_t { Bool, Float, Vec2, Vec3, Vec4, Quat };

    //struct NanoAnimCurveRef {
    //    uint32_t keyCount = 0;
    //    uint32_t keyOffset = 0;
    //};

    //struct NanoAnimTrackEntry {
    //    uint32_t relativePathByteCount = 0;
    //    uint32_t relativePathOffset = 0;

    //    uint32_t componentTypeId = 0;
    //    uint32_t fieldId = 0;

    //    NanoAnimValueType type = NanoAnimValueType::Float;
    //    uint8_t reserved[3]{};

    //    NanoAnimCurveRef curve[4];
    //};

    //struct NanoAnimKeyF {
    //    float time;
    //    float value;
    //    float inTan;
    //    float outTan;
    //};
#pragma pack(pop)

}
