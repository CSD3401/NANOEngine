#include "pch.h"
#include "LightingAsset.hpp"

#include "Core/SpdLogger.hpp"
#include "ResourceManagement/BinaryHeaders/NanoLightingHeader.hpp"
#include "Serialisation/BinaryReflection.hpp"

namespace NE::Lighting {

	bool LightingAsset::Preload(Resource::BinaryView blob) {
		if (blob.size < sizeof(Resource::NanoLightingHeader)) {
			return false;
		}

		const auto* hdr = blob.as<Resource::NanoLightingHeader>(0);
		if (!hdr) return false;
		if (hdr->magic != Resource::NLGT_MAGIC) return false;

		if (hdr->version != Resource::CURRENT_NANOLIGHTING_FORMAT_VERSION) {
			SPD_ERROR("NanoLighting version mismatch (got " << hdr->version
				<< ", expected " << Resource::CURRENT_NANOLIGHTING_FORMAT_VERSION
				<< "). Re-bake or recook the lighting asset.");
			return false;
		}

		const size_t payloadOffset = sizeof(Resource::NanoLightingHeader);
		if (blob.size < payloadOffset + static_cast<size_t>(hdr->payloadBytes)) {
			return false;
		}

		const uint8_t* it = blob.data + payloadOffset;
		const uint8_t* end = it + hdr->payloadBytes;

		LightingAssetBlob parsed{};
		if (!NE::Deserialization::FromBinary(it, end, parsed)) {
			return false;
		}

		m_data = std::move(parsed);
		return true;
	}

	void LightingAsset::Finalize() {
		// CPU-side manifest only in v1. Texture resources resolve later at runtime.
	}

}
