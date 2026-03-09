#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "BakeGeometryCollector.hpp"

#include <Math/Vec2.hpp>
#include <Math/Vec3.hpp>

namespace Editor::Lightmapping {

	struct BakeRay {
		NE::Math::Vec3 origin{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 direction{ 0.0f, 0.0f, -1.0f };
		float tMin = 0.0f;
		float tMax = 0.0f;
	};

	struct BakeHit {
		bool hit = false;
		float t = 0.0f;
		NE::Math::Vec2 barycentrics{ 0.0f, 0.0f };
		uint32_t primitiveIndex = 0;
		NE::Math::Vec3 position{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 geometricNormal{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec3 shadingNormal{ 0.0f, 1.0f, 0.0f };
		uint32_t entity = 0;
		uint64_t rendererLuid = 0;
		std::string modelUUID;
		std::string materialUUID;
		uint32_t subMeshIndex = 0;
	};

	struct SceneBakeBVHBuildSettings {
		int maxLeafPrimitives = 8;
		float traversalEpsilon = 1e-5f;
	};

	struct SceneBakeBVHStats {
		size_t inputTriangleCount = 0;
		size_t triangleCount = 0;
		size_t skippedInvalidTriangleCount = 0;
		size_t nodeCount = 0;
		size_t leafCount = 0;
		size_t maxDepth = 0;
		float avgPrimitivesPerLeaf = 0.0f;
		double buildMs = 0.0;
	};

	struct SceneBakeBVHDebugOptions {
		bool drawRootBounds = false;
		bool drawLeafBounds = false;
		int leafDepth = 0;
	};

	struct SceneBakeBVHSessionState {
		bool hasValidBVH = false;
		SceneBakeBVHBuildSettings settings{};
		BakeGeometryCollectionStats geometryStats{};
		SceneBakeBVHStats stats{};
		std::vector<BakeGeometryWarning> warnings;
		std::map<std::string, size_t> warningCounts;
		SceneBakeBVHDebugOptions debugOptions{};
		std::string statusMessage;
		std::string selfCheckMessage;
		std::string debugStatusMessage;
		bool selfCheckPassed = false;
		bool debugGeometryCacheValid = false;
		bool cachedDrawRootBounds = false;
		bool cachedDrawLeafBounds = false;
		int cachedLeafDepth = -1;
		std::vector<NE::Math::Vec3> cachedRootDebugLines;
		std::vector<NE::Math::Vec3> cachedLeafDebugLines;
	};

	class SceneBakeBVH {
	public:
		bool Build(const BakeGeometryCollection& collection, const SceneBakeBVHBuildSettings& settings);
		void Clear();

		bool IsValid() const { return !m_nodes.empty() && !m_primitives.empty(); }
		const SceneBakeBVHStats& GetStats() const { return m_stats; }
		const std::vector<BakeTriangle>& GetPrimitives() const { return m_primitives; }
		const std::vector<BakePrimitiveSource>& GetSources() const { return m_sources; }

		bool AnyHit(const BakeRay& ray) const;
		bool ClosestHit(const BakeRay& ray, BakeHit& outHit) const;
		bool AppendDebugLineVertices(
			bool includeRoot,
			bool includeLeafs,
			int leafDepth,
			std::vector<NE::Math::Vec3>& outRoot,
			std::vector<NE::Math::Vec3>& outLeafs,
			std::string& outDebugMessage) const;

	private:
		static constexpr uint32_t kInvalidNodeIndex = UINT32_MAX;

		struct Node {
			BakeAABB bounds{};
			uint32_t leftChild = kInvalidNodeIndex;
			uint32_t rightChild = kInvalidNodeIndex;
			uint32_t firstPrimitive = 0;
			uint32_t primitiveCount = 0;
			bool isLeaf = false;
		};

		uint32_t BuildRecursive(uint32_t start, uint32_t count, uint32_t depth, const SceneBakeBVHBuildSettings& settings);
		bool BuildHit(const BakeRay& ray, uint32_t primitiveIndex, float t, float u, float v, BakeHit& outHit) const;
		bool TryGetValidChildren(uint32_t nodeIndex, uint32_t& outLeftChild, uint32_t& outRightChild) const;

		std::vector<Node> m_nodes;
		std::vector<BakeTriangle> m_primitives;
		std::vector<BakePrimitiveSource> m_sources;
		SceneBakeBVHStats m_stats{};
		SceneBakeBVHBuildSettings m_buildSettings{};
	};

	SceneBakeBVHSessionState& GetSceneBakeBVHSessionState();
	bool BuildSceneBakeBVHFromCurrentScene(const SceneBakeBVHBuildSettings& settings);
	void ClearSceneBakeBVH();
	bool HasValidSceneBakeBVH();
	bool SceneBakeBVHAnyHit(const BakeRay& ray);
	bool SceneBakeBVHClosestHit(const BakeRay& ray, BakeHit& outHit);
	void DrawSceneBakeBVHDebug();
	bool RunSceneBakeBVHSelfCheck(std::string& outMessage);

}
