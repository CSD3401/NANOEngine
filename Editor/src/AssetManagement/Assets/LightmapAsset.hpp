#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Core/Reflection.hpp"
#include "IAsset.hpp"
#include <Math/Vec3.hpp>
#include <Lighting/LightmapResource.hpp>

namespace Editor::Assets {

	struct LightmapAssetBakeSettings {
		uint32_t workerCount = 0;
		uint32_t dilationRadiusTexels = 0;
		bool rebuildBvhBeforeBake = true;
		bool generateDebugBuffers = true;
		float rayOriginBias = 1e-3f;
		float rayMinDistance = 1e-4f;
		float finiteLightDistanceEpsilon = 1e-3f;
		float previewExposure = 1.0f;
		float texelsPerUnit = 16.0f;
		uint32_t pageSize = 2048;
		uint32_t padding = 8;

		NE_REFLECT_BEGIN(LightmapAssetBakeSettings)
			NE_REFLECT_FIELD(workerCount),
			NE_REFLECT_FIELD(dilationRadiusTexels),
			NE_REFLECT_FIELD(rebuildBvhBeforeBake),
			NE_REFLECT_FIELD(generateDebugBuffers),
			NE_REFLECT_FIELD(rayOriginBias),
			NE_REFLECT_FIELD(rayMinDistance),
			NE_REFLECT_FIELD(finiteLightDistanceEpsilon),
			NE_REFLECT_FIELD(previewExposure),
			NE_REFLECT_FIELD(texelsPerUnit),
			NE_REFLECT_FIELD(pageSize),
			NE_REFLECT_FIELD(padding)
		NE_REFLECT_END()
	};

	struct LightmapAssetPage {
		int32_t pageIndex = -1;
		std::string pageId;
		uint32_t width = 0;
		uint32_t height = 0;
		uint64_t validTexelCount = 0;
		uint64_t allocatedInnerTexelCount = 0;
		float coverage01 = 0.0f;
		std::vector<NE::Math::Vec3> lighting;
		std::vector<uint8_t> validMask;
		std::vector<uint8_t> dilationWriteMask;

		NE_REFLECT_BEGIN(LightmapAssetPage)
			NE_REFLECT_FIELD(pageIndex),
			NE_REFLECT_FIELD(pageId),
			NE_REFLECT_FIELD(width),
			NE_REFLECT_FIELD(height),
			NE_REFLECT_FIELD(validTexelCount),
			NE_REFLECT_FIELD(allocatedInnerTexelCount),
			NE_REFLECT_FIELD(coverage01),
			NE_REFLECT_FIELD(lighting),
			NE_REFLECT_FIELD(validMask),
			NE_REFLECT_FIELD(dilationWriteMask)
		NE_REFLECT_END()
	};

	struct LightmapAssetBlob {
		uint16_t formatVersionMajor = 1;
		uint16_t formatVersionMinor = 0;
		std::string lightmapAssetId;
		std::string lightingRevisionId;
		std::string dependencySignature;
		LightmapAssetBakeSettings bakeSettings{};
		std::vector<NE::Lighting::LightmapBindingRecord> bindings;
		std::vector<LightmapAssetPage> pages;

		NE_REFLECT_BEGIN(LightmapAssetBlob)
			NE_REFLECT_FIELD(formatVersionMajor),
			NE_REFLECT_FIELD(formatVersionMinor),
			NE_REFLECT_FIELD(lightmapAssetId),
			NE_REFLECT_FIELD(lightingRevisionId),
			NE_REFLECT_FIELD(dependencySignature),
			NE_REFLECT_FIELD(bakeSettings),
			NE_REFLECT_FIELD(bindings),
			NE_REFLECT_FIELD(pages)
		NE_REFLECT_END()
	};

	class LightmapAsset final : public IAsset {
	public:
		bool Cook(const std::string& sourcePath, const std::string& outPath) const override;
		bool LoadImportSettings(const std::string& sourcePath) override;
		bool SaveImportSettings(const std::string& sourcePath) override;

		static bool LoadBlob(const std::string& sourcePath, LightmapAssetBlob& outBlob, std::string& outErrorMessage);
		static bool SaveBlob(const std::string& sourcePath, const LightmapAssetBlob& blob, std::string& outErrorMessage);
	};

}
