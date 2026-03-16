#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "LightmapAtlasAllocator.hpp"

#include <Math/Vec2.hpp>
#include <Math/Vec3.hpp>

namespace Editor::Lightmapping {

	inline constexpr uint32_t kInvalidRasterReceiverIndex = std::numeric_limits<uint32_t>::max();
	inline constexpr uint32_t kInvalidRasterTriangleIndex = std::numeric_limits<uint32_t>::max();

	struct LightmapBakeReceiverTriangle {
		NE::Math::Vec3 p0{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 p1{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 p2{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 shadingNormal0{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec3 shadingNormal1{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec3 shadingNormal2{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec3 geometricNormal{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec2 uv0{ 0.0f, 0.0f };
		NE::Math::Vec2 uv1{ 0.0f, 0.0f };
		NE::Math::Vec2 uv2{ 0.0f, 0.0f };
		uint32_t sourceTriangleIndex = 0;
	};

	struct LightmapBakeReceiverSnapshot {
		uint32_t entity = NE::ECS::NO_ENTITY;
		uint64_t stableId = 0;
		uint64_t rendererLuid = 0;
		std::string entityName;
		std::string modelUUID;
		std::string materialUUID;
		uint32_t subMeshIndex = 0;
		int pageIndex = -1;
		uint32_t pageWidth = 0;
		uint32_t pageHeight = 0;
		LightmapPlacement placement{};
		std::vector<LightmapBakeReceiverTriangle> triangles;
	};

	struct LightmapUvRasterizationSettings {
		uint32_t workerCount = 0;
		bool generateDebugPreviews = true;
		float edgeEpsilon = 1e-6f;
		float uvAreaEpsilon = 1e-8f;
	};

	struct LightmapUvRasterPagePreview {
		int pageIndex = -1;
		std::string pageId;
		uint32_t width = 0;
		uint32_t height = 0;
		size_t validTexelCount = 0;
		size_t allocatedInnerTexelCount = 0;
		float coverage01 = 0.0f;
		std::vector<uint8_t> validityRgba8;
		std::vector<uint8_t> ownerRgba8;
	};

	struct LightmapUvRasterPageBuffers {
		int pageIndex = -1;
		std::string pageId;
		uint32_t width = 0;
		uint32_t height = 0;
		size_t validTexelCount = 0;
		size_t allocatedInnerTexelCount = 0;
		float coverage01 = 0.0f;
		std::vector<uint8_t> validMask;
		std::vector<uint32_t> ownerReceiverIndex;
		std::vector<uint32_t> ownerTriangleIndex;
		std::vector<uint32_t> ownerSourceTriangleIndex;
		std::vector<NE::Math::Vec3> barycentrics;
		LightmapUvRasterPagePreview preview{};
	};

	struct LightmapUvRasterReceiverCoverage {
		uint32_t entity = NE::ECS::NO_ENTITY;
		uint64_t stableId = 0;
		int pageIndex = -1;
		size_t allocatedInnerTexelCount = 0;
		size_t validTexelCount = 0;
		float coverage01 = 0.0f;
	};

	struct LightmapUvRasterStats {
		size_t receiverInstanceCount = 0;
		size_t pageCount = 0;
		size_t triangleCount = 0;
		size_t degenerateUvTriangleCount = 0;
		size_t coveredTexelCount = 0;
		size_t uncoveredTexelCount = 0;
		size_t ownershipConflictCount = 0;
		size_t sameReceiverOwnershipConflictCount = 0;
		size_t crossReceiverOwnershipConflictCount = 0;
		size_t invalidBarycentricTexelCount = 0;
		size_t outOfRangeTexelCount = 0;
		size_t invalidSampleTexelCount = 0;
		size_t innerRectClampedTriangleCount = 0;
	};

	struct LightmapBakeReceiverCollectionStats {
		size_t discardedTriangleCount = 0;
		size_t outOfRangeIndexTriangleCount = 0;
		size_t nonFiniteUvTriangleCount = 0;
		size_t nonFiniteWorldPositionTriangleCount = 0;
		size_t degenerateWorldTriangleCount = 0;
		size_t receiversWithDiscardedTriangles = 0;
	};

	struct LightmapUvRasterResult {
		LightmapUvRasterizationSettings settings{};
		LightmapUvRasterStats stats{};
		std::vector<LightmapAtlasPage> pages;
		std::vector<LightmapBakeReceiverSnapshot> receivers;
		std::vector<LightmapUvRasterPageBuffers> pageBuffers;
		std::vector<LightmapUvRasterReceiverCoverage> receiverCoverage;
		std::vector<std::string> warnings;
		std::map<std::string, size_t> warningCounts;
	};

	struct LightmapRasterSample {
		uint32_t entity = NE::ECS::NO_ENTITY;
		uint64_t stableId = 0;
		uint64_t rendererLuid = 0;
		std::string entityName;
		std::string modelUUID;
		std::string materialUUID;
		uint32_t subMeshIndex = 0;
		uint32_t sourceTriangleIndex = 0;
		uint32_t receiverIndex = kInvalidRasterReceiverIndex;
		uint32_t triangleIndex = kInvalidRasterTriangleIndex;
		int pageIndex = -1;
		uint32_t texelX = 0;
		uint32_t texelY = 0;
		NE::Math::Vec3 barycentrics{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 worldPosition{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 shadingNormal{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec3 geometricNormal{ 0.0f, 1.0f, 0.0f };
	};

	std::vector<LightmapBakeReceiverSnapshot> CollectLightmapBakeReceiverSnapshots(
		const std::vector<LightmapPlacement>& placements,
		const std::vector<LightmapAtlasPage>& pages,
		std::vector<std::string>& warnings,
		LightmapBakeReceiverCollectionStats* outStats = nullptr);

	LightmapUvRasterResult RasterizeLightmapUv1Atlas(
		const std::vector<LightmapAtlasPage>& pages,
		const std::vector<LightmapBakeReceiverSnapshot>& receivers,
		const LightmapUvRasterizationSettings& settings,
		std::atomic<bool>* cancelRequested = nullptr);

	bool TryReconstructRasterSample(
		const LightmapUvRasterResult& result,
		const LightmapUvRasterPageBuffers& page,
		size_t linearIndex,
		LightmapRasterSample& outSample);

	bool RunLightmapUvRasterizerSelfCheck(std::string& outMessage);

}
