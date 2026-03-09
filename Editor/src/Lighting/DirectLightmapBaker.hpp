#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "LightmapAtlasAllocator.hpp"

#include <Math/Vec3.hpp>

namespace Editor::Lightmapping {

	struct DirectLightBakeSettings {
		uint32_t workerCount = 0;
		bool rebuildBvhBeforeBake = true;
		bool generateDebugBuffers = true;
		float rayOriginBias = 1e-3f;
		float rayMinDistance = 1e-4f;
		float finiteLightDistanceEpsilon = 1e-3f;
		float previewExposure = 1.0f;
	};

	struct DirectLightBakePagePreview {
		int pageIndex = -1;
		std::string pageId;
		uint32_t width = 0;
		uint32_t height = 0;
		size_t validTexelCount = 0;
		std::vector<uint8_t> lightingRgba8;
		std::vector<uint8_t> validityRgba8;
	};

	struct DirectLightBakePageBuffers {
		int pageIndex = -1;
		std::string pageId;
		uint32_t width = 0;
		uint32_t height = 0;
		size_t validTexelCount = 0;
		std::vector<NE::Math::Vec3> lighting;
		std::vector<uint8_t> validMask;
		std::vector<uint32_t> ownerEntity;
		std::vector<uint32_t> ownerTriangle;
		std::vector<NE::Math::Vec3> worldNormal;
		DirectLightBakePagePreview preview{};
	};

	struct DirectLightBakeStats {
		size_t bakeInstanceCount = 0;
		size_t pageCount = 0;
		size_t supportedLightCount = 0;
		size_t coveredTexelCount = 0;
		size_t skippedTexelCount = 0;
		size_t raysCast = 0;
		size_t occludedRayCount = 0;
		size_t visibleRayCount = 0;
		size_t directionalLightCount = 0;
		size_t pointLightCount = 0;
		size_t spotLightCount = 0;
		double setupMs = 0.0;
		double evaluationMs = 0.0;
	};

	struct DirectLightBakeResult {
		uint64_t revision = 0;
		DirectLightBakeSettings settings{};
		DirectLightBakeStats stats{};
		std::vector<DirectLightBakePageBuffers> pages;
		std::vector<std::string> warnings;
		std::unordered_map<std::string, size_t> warningCounts;
	};

	struct DirectLightBakeSessionState {
		bool hasResult = false;
		bool isRunning = false;
		bool cancelRequested = false;
		bool lastBakeSucceeded = false;
		float progress01 = 0.0f;
		std::string statusMessage;
		std::string activeStage;
		size_t queuedInstanceCount = 0;
		size_t processedInstanceCount = 0;
		DirectLightBakeSettings settings{};
		DirectLightBakeStats liveStats{};
		std::shared_ptr<const DirectLightBakeResult> result;
	};

	DirectLightBakeSessionState GetDirectLightBakeSessionState();
	void UpdateDirectLightBakeSession();
	bool StartSceneDirectLightBake(const DirectLightBakeSettings& settings);
	void CancelSceneDirectLightBake();
	void ShutdownDirectLightBakeSession();

}
