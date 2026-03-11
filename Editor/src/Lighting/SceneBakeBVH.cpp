#include "pch.h"
#include "SceneBakeBVH.hpp"

#include <chrono>
#include <cmath>
#include <limits>

#include <Core/SpdLogger.hpp>
#include <EditorInterface/RendererExports.hpp>

namespace Editor::Lightmapping {
	namespace {
		constexpr float kDirectionEpsilon = 1e-8f;
		constexpr float kTriangleAreaEpsilon = 1e-8f;
		constexpr size_t kMaxDebugLeafBoxes = 20000;
		constexpr size_t kMaxDebugTraversalNodes = 4000000;

		std::unique_ptr<SceneBakeBVH> g_sceneBakeBVH;

		float Component(const NE::Math::Vec3& value, int axis) {
			switch (axis) {
			case 1: return value.y;
			case 2: return value.z;
			default: return value.x;
			}
		}

		bool IsFiniteVec3(const NE::Math::Vec3& value) {
			return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
		}

		bool IsFiniteBounds(const BakeAABB& bounds) {
			return IsFiniteVec3(bounds.min) && IsFiniteVec3(bounds.max) &&
				bounds.min.x <= bounds.max.x &&
				bounds.min.y <= bounds.max.y &&
				bounds.min.z <= bounds.max.z;
		}

		BakeAABB InvalidBounds() {
			BakeAABB bounds{};
			const float maxValue = std::numeric_limits<float>::max();
			bounds.min = { maxValue, maxValue, maxValue };
			bounds.max = { -maxValue, -maxValue, -maxValue };
			return bounds;
		}

		BakeAABB UnionBounds(const BakeAABB& lhs, const BakeAABB& rhs) {
			if (!IsFiniteBounds(lhs)) {
				return rhs;
			}
			if (!IsFiniteBounds(rhs)) {
				return lhs;
			}

			BakeAABB bounds{};
			bounds.min.x = std::min(lhs.min.x, rhs.min.x);
			bounds.min.y = std::min(lhs.min.y, rhs.min.y);
			bounds.min.z = std::min(lhs.min.z, rhs.min.z);
			bounds.max.x = std::max(lhs.max.x, rhs.max.x);
			bounds.max.y = std::max(lhs.max.y, rhs.max.y);
			bounds.max.z = std::max(lhs.max.z, rhs.max.z);
			return bounds;
		}

		float BoundsDiagonalLength(const BakeAABB& bounds) {
			if (!IsFiniteBounds(bounds)) {
				return 0.0f;
			}

			const NE::Math::Vec3 extent = bounds.max - bounds.min;
			const float length = extent.Length();
			return std::isfinite(length) ? length : 0.0f;
		}

		bool IsValidTriangle(const BakeTriangle& triangle) {
			return triangle.area > kTriangleAreaEpsilon &&
				std::isfinite(triangle.area) &&
				IsFiniteVec3(triangle.p0) &&
				IsFiniteVec3(triangle.p1) &&
				IsFiniteVec3(triangle.p2) &&
				IsFiniteVec3(triangle.centroid) &&
				IsFiniteVec3(triangle.geometricNormal) &&
				IsFiniteBounds(triangle.bounds);
		}

		int LargestExtentAxis(const BakeAABB& bounds) {
			const NE::Math::Vec3 extent = bounds.max - bounds.min;
			if (extent.y > extent.x && extent.y >= extent.z) {
				return 1;
			}
			if (extent.z > extent.x && extent.z >= extent.y) {
				return 2;
			}
			return 0;
		}

		bool IntersectAABB(
			const BakeAABB& bounds,
			const BakeRay& ray,
			float currentMaxT,
			float& outNearT) {
			float tMin = ray.tMin;
			float tMax = currentMaxT;

			for (int axis = 0; axis < 3; ++axis) {
				const float origin = Component(ray.origin, axis);
				const float direction = Component(ray.direction, axis);
				const float minValue = Component(bounds.min, axis);
				const float maxValue = Component(bounds.max, axis);

				if (std::fabs(direction) <= kDirectionEpsilon) {
					if (origin < minValue || origin > maxValue) {
						return false;
					}
					continue;
				}

				const float invDirection = 1.0f / direction;
				float axisNear = (minValue - origin) * invDirection;
				float axisFar = (maxValue - origin) * invDirection;
				if (axisNear > axisFar) {
					std::swap(axisNear, axisFar);
				}

				tMin = std::max(tMin, axisNear);
				tMax = std::min(tMax, axisFar);
				if (tMin > tMax) {
					return false;
				}
			}

			outNearT = tMin;
			return true;
		}

		bool IntersectTriangle(
			const BakeTriangle& triangle,
			const BakeRay& ray,
			float epsilon,
			float& outT,
			float& outU,
			float& outV) {
			const NE::Math::Vec3 edge1 = triangle.p1 - triangle.p0;
			const NE::Math::Vec3 edge2 = triangle.p2 - triangle.p0;
			const NE::Math::Vec3 pvec = ray.direction.Cross(edge2);
			const float det = edge1.Dot(pvec);

			if (std::fabs(det) <= epsilon) {
				return false;
			}

			const float invDet = 1.0f / det;
			const NE::Math::Vec3 tvec = ray.origin - triangle.p0;
			const float u = tvec.Dot(pvec) * invDet;
			if (u < 0.0f || u > 1.0f) {
				return false;
			}

			const NE::Math::Vec3 qvec = tvec.Cross(edge1);
			const float v = ray.direction.Dot(qvec) * invDet;
			if (v < 0.0f || (u + v) > 1.0f) {
				return false;
			}

			const float t = edge2.Dot(qvec) * invDet;
			if (t < ray.tMin || t > ray.tMax) {
				return false;
			}

			outT = t;
			outU = u;
			outV = v;
			return true;
		}

		void AppendBoundsWireframe(std::vector<NE::Math::Vec3>& positions, const BakeAABB& bounds) {
			const NE::Math::Vec3 corners[8] = {
				{ bounds.min.x, bounds.min.y, bounds.min.z },
				{ bounds.max.x, bounds.min.y, bounds.min.z },
				{ bounds.max.x, bounds.max.y, bounds.min.z },
				{ bounds.min.x, bounds.max.y, bounds.min.z },
				{ bounds.min.x, bounds.min.y, bounds.max.z },
				{ bounds.max.x, bounds.min.y, bounds.max.z },
				{ bounds.max.x, bounds.max.y, bounds.max.z },
				{ bounds.min.x, bounds.max.y, bounds.max.z }
			};

			const int edges[12][2] = {
				{0,1}, {1,2}, {2,3}, {3,0},
				{4,5}, {5,6}, {6,7}, {7,4},
				{0,4}, {1,5}, {2,6}, {3,7}
			};

			for (const auto& edge : edges) {
				positions.push_back(corners[edge[0]]);
				positions.push_back(corners[edge[1]]);
			}
		}

		SceneBakeBVHSessionState& SessionStateStorage() {
			static SceneBakeBVHSessionState state;
			return state;
		}

		void RebuildWarningCounts(SceneBakeBVHSessionState& state) {
			state.warningCounts.clear();
			for (const auto& warning : state.warnings) {
				state.warningCounts[ToString(warning.reason)]++;
			}
		}

		void InvalidateDebugGeometryCache(SceneBakeBVHSessionState& state) {
			state.debugGeometryCacheValid = false;
			state.cachedDrawRootBounds = false;
			state.cachedDrawLeafBounds = false;
			state.cachedLeafDepth = -1;
			state.cachedRootDebugLines.clear();
			state.cachedLeafDebugLines.clear();
			state.debugStatusMessage.clear();
		}
	}

	bool SceneBakeBVH::Build(const BakeGeometryCollection& collection, const SceneBakeBVHBuildSettings& settings) {
		Clear();
		m_buildSettings = settings;

		m_sources = collection.sources;
		m_stats.inputTriangleCount = collection.triangles.size();

		for (const auto& triangle : collection.triangles) {
			if (IsValidTriangle(triangle)) {
				m_primitives.push_back(triangle);
			} else {
				++m_stats.skippedInvalidTriangleCount;
			}
		}

		m_stats.triangleCount = m_primitives.size();
		if (m_primitives.empty()) {
			return false;
		}

		m_nodes.reserve((m_primitives.size() * 2u) - 1u);

		const auto buildStart = std::chrono::high_resolution_clock::now();
		BuildRecursive(0u, static_cast<uint32_t>(m_primitives.size()), 0u, settings);
		const auto buildEnd = std::chrono::high_resolution_clock::now();
		m_stats.buildMs = std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();
		m_stats.nodeCount = m_nodes.size();
		m_stats.avgPrimitivesPerLeaf = m_stats.leafCount > 0
			? static_cast<float>(m_stats.triangleCount) / static_cast<float>(m_stats.leafCount)
			: 0.0f;
		return true;
	}

	void SceneBakeBVH::Clear() {
		m_nodes.clear();
		m_primitives.clear();
		m_sources.clear();
		m_stats = {};
	}

	uint32_t SceneBakeBVH::BuildRecursive(
		uint32_t start,
		uint32_t count,
		uint32_t depth,
		const SceneBakeBVHBuildSettings& settings) {
		const uint32_t nodeIndex = static_cast<uint32_t>(m_nodes.size());
		m_nodes.push_back({});

		m_stats.maxDepth = std::max(m_stats.maxDepth, static_cast<size_t>(depth));

		BakeAABB bounds = InvalidBounds();
		BakeAABB centroidBounds = InvalidBounds();
		for (uint32_t i = start; i < start + count; ++i) {
			bounds = UnionBounds(bounds, m_primitives[i].bounds);

			BakeAABB centroidBox{};
			centroidBox.min = m_primitives[i].centroid;
			centroidBox.max = m_primitives[i].centroid;
			centroidBounds = UnionBounds(centroidBounds, centroidBox);
		}

		m_nodes[nodeIndex].bounds = bounds;

		if (count <= static_cast<uint32_t>(std::max(settings.maxLeafPrimitives, 1))) {
			auto& node = m_nodes[nodeIndex];
			node.isLeaf = true;
			node.firstPrimitive = start;
			node.primitiveCount = count;
			++m_stats.leafCount;
			return nodeIndex;
		}

		const int axis = LargestExtentAxis(centroidBounds);
		std::stable_sort(
			m_primitives.begin() + start,
			m_primitives.begin() + (start + count),
			[axis](const BakeTriangle& lhs, const BakeTriangle& rhs) {
				const float lhsValue = Component(lhs.centroid, axis);
				const float rhsValue = Component(rhs.centroid, axis);
				if (lhsValue != rhsValue) {
					return lhsValue < rhsValue;
				}
				return lhs.originalIndex < rhs.originalIndex;
			});

		const uint32_t leftCount = count / 2u;
		const uint32_t rightCount = count - leftCount;
		if (leftCount == 0u || rightCount == 0u) {
			auto& node = m_nodes[nodeIndex];
			node.isLeaf = true;
			node.firstPrimitive = start;
			node.primitiveCount = count;
			++m_stats.leafCount;
			return nodeIndex;
		}

		const uint32_t leftChild = BuildRecursive(start, leftCount, depth + 1u, settings);
		const uint32_t rightChild = BuildRecursive(start + leftCount, rightCount, depth + 1u, settings);
		auto& node = m_nodes[nodeIndex];
		node.isLeaf = false;
		node.leftChild = leftChild;
		node.rightChild = rightChild;
		return nodeIndex;
	}

	bool SceneBakeBVH::TryGetValidChildren(uint32_t nodeIndex, uint32_t& outLeftChild, uint32_t& outRightChild) const {
		if (nodeIndex >= m_nodes.size()) {
			return false;
		}

		const auto& node = m_nodes[nodeIndex];
		if (node.isLeaf) {
			return false;
		}

		if (node.leftChild == kInvalidNodeIndex || node.rightChild == kInvalidNodeIndex) {
			return false;
		}

		if (node.leftChild >= m_nodes.size() || node.rightChild >= m_nodes.size()) {
			return false;
		}

		if (node.leftChild == nodeIndex || node.rightChild == nodeIndex) {
			return false;
		}

		if (node.leftChild == node.rightChild) {
			return false;
		}

		outLeftChild = node.leftChild;
		outRightChild = node.rightChild;
		return true;
	}

	bool SceneBakeBVH::BuildHit(
		const BakeRay& ray,
		uint32_t primitiveIndex,
		float t,
		float u,
		float v,
		BakeHit& outHit) const {
		if (primitiveIndex >= m_primitives.size()) {
			return false;
		}

		const auto& triangle = m_primitives[primitiveIndex];
		if (triangle.sourceIndex >= m_sources.size()) {
			return false;
		}

		const auto& source = m_sources[triangle.sourceIndex];
		const float w = 1.0f - u - v;
		NE::Math::Vec3 shadingNormal =
			triangle.shadingNormal0 * w +
			triangle.shadingNormal1 * u +
			triangle.shadingNormal2 * v;
		if (!IsFiniteVec3(shadingNormal) || shadingNormal.LengthSquared() <= kTriangleAreaEpsilon) {
			shadingNormal = triangle.geometricNormal;
		} else {
			shadingNormal = shadingNormal.Normalized();
		}

		outHit.hit = true;
		outHit.t = t;
		outHit.barycentrics = { u, v };
		outHit.primitiveIndex = primitiveIndex;
		outHit.position = ray.origin + (ray.direction * t);
		outHit.geometricNormal = triangle.geometricNormal;
		outHit.shadingNormal = shadingNormal;
		outHit.entity = source.entity;
		outHit.rendererLuid = source.rendererLuid;
		outHit.modelUUID = source.modelUUID;
		outHit.materialUUID = source.materialUUID;
		outHit.subMeshIndex = source.subMeshIndex;
		return true;
	}

	bool SceneBakeBVH::AnyHit(const BakeRay& ray) const {
		if (!IsValid() || ray.tMax < ray.tMin) {
			return false;
		}

		struct StackEntry {
			uint32_t nodeIndex = 0;
			float nearT = 0.0f;
		};

		std::vector<StackEntry> stack;
		stack.reserve(64);
		stack.push_back({ 0u, ray.tMin });

		while (!stack.empty()) {
			const StackEntry entry = stack.back();
			stack.pop_back();

			const auto& node = m_nodes[entry.nodeIndex];
			float nearT = 0.0f;
			if (!IntersectAABB(node.bounds, ray, ray.tMax, nearT)) {
				continue;
			}

			if (node.isLeaf) {
				for (uint32_t i = 0; i < node.primitiveCount; ++i) {
					const uint32_t primitiveIndex = node.firstPrimitive + i;
					float t = 0.0f;
					float u = 0.0f;
					float v = 0.0f;
					if (IntersectTriangle(m_primitives[primitiveIndex], ray, m_buildSettings.traversalEpsilon, t, u, v)) {
						return true;
					}
				}
				continue;
			}

			uint32_t leftChild = kInvalidNodeIndex;
			uint32_t rightChild = kInvalidNodeIndex;
			if (!TryGetValidChildren(entry.nodeIndex, leftChild, rightChild)) {
				continue;
			}

			float leftNear = 0.0f;
			float rightNear = 0.0f;
			const bool hitLeft = IntersectAABB(m_nodes[leftChild].bounds, ray, ray.tMax, leftNear);
			const bool hitRight = IntersectAABB(m_nodes[rightChild].bounds, ray, ray.tMax, rightNear);

			if (hitLeft && hitRight) {
				if (leftNear < rightNear) {
					stack.push_back({ rightChild, rightNear });
					stack.push_back({ leftChild, leftNear });
				} else {
					stack.push_back({ leftChild, leftNear });
					stack.push_back({ rightChild, rightNear });
				}
			} else if (hitLeft) {
				stack.push_back({ leftChild, leftNear });
			} else if (hitRight) {
				stack.push_back({ rightChild, rightNear });
			}
		}

		return false;
	}

	bool SceneBakeBVH::ClosestHit(const BakeRay& ray, BakeHit& outHit) const {
		outHit = {};
		if (!IsValid() || ray.tMax < ray.tMin) {
			return false;
		}

		struct StackEntry {
			uint32_t nodeIndex = 0;
			float nearT = 0.0f;
		};

		std::vector<StackEntry> stack;
		stack.reserve(64);
		stack.push_back({ 0u, ray.tMin });

		float closestT = ray.tMax;
		bool hasHit = false;

		while (!stack.empty()) {
			const StackEntry entry = stack.back();
			stack.pop_back();

			const auto& node = m_nodes[entry.nodeIndex];
			float nearT = 0.0f;
			if (!IntersectAABB(node.bounds, ray, closestT, nearT)) {
				continue;
			}

			if (node.isLeaf) {
				for (uint32_t i = 0; i < node.primitiveCount; ++i) {
					const uint32_t primitiveIndex = node.firstPrimitive + i;
					float t = 0.0f;
					float u = 0.0f;
					float v = 0.0f;
					if (!IntersectTriangle(m_primitives[primitiveIndex], ray, m_buildSettings.traversalEpsilon, t, u, v)) {
						continue;
					}

					if (t < closestT) {
						closestT = t;
						hasHit = BuildHit(ray, primitiveIndex, t, u, v, outHit);
					}
				}
				continue;
			}

			uint32_t leftChild = kInvalidNodeIndex;
			uint32_t rightChild = kInvalidNodeIndex;
			if (!TryGetValidChildren(entry.nodeIndex, leftChild, rightChild)) {
				continue;
			}

			float leftNear = 0.0f;
			float rightNear = 0.0f;
			const bool hitLeft = IntersectAABB(m_nodes[leftChild].bounds, ray, closestT, leftNear);
			const bool hitRight = IntersectAABB(m_nodes[rightChild].bounds, ray, closestT, rightNear);

			if (hitLeft && hitRight) {
				if (leftNear < rightNear) {
					stack.push_back({ rightChild, rightNear });
					stack.push_back({ leftChild, leftNear });
				} else {
					stack.push_back({ leftChild, leftNear });
					stack.push_back({ rightChild, rightNear });
				}
			} else if (hitLeft) {
				stack.push_back({ leftChild, leftNear });
			} else if (hitRight) {
				stack.push_back({ rightChild, rightNear });
			}
		}

		return hasHit;
	}

	bool SceneBakeBVH::AppendDebugLineVertices(
		bool includeRoot,
		bool includeLeafs,
		int leafDepth,
		std::vector<NE::Math::Vec3>& outRoot,
		std::vector<NE::Math::Vec3>& outLeafs,
		std::string& outDebugMessage) const {
		outDebugMessage.clear();

		if (!IsValid()) {
			return false;
		}

		if (includeRoot) {
			AppendBoundsWireframe(outRoot, m_nodes.front().bounds);
		}

		if (!includeLeafs) {
			return true;
		}

		struct StackEntry {
			uint32_t nodeIndex = 0;
			int depth = 0;
		};

		std::vector<StackEntry> stack;
		stack.reserve(m_nodes.size());
		stack.push_back({ 0u, 0 });
		std::vector<uint8_t> visitCounts(m_nodes.size(), 0u);
		size_t visitedNodeCount = 0;
		size_t emittedLeafBoxes = 0;
		const int clampedLeafDepth = std::clamp(leafDepth, 0, static_cast<int>(m_stats.maxDepth));
		const int targetDisplayDepth = std::max(static_cast<int>(m_stats.maxDepth) - clampedLeafDepth, 0);

		while (!stack.empty()) {
			const StackEntry entry = stack.back();
			stack.pop_back();
			if (++visitedNodeCount > kMaxDebugTraversalNodes) {
				outDebugMessage =
					"Leaf debug traversal aborted after visiting " +
					std::to_string(kMaxDebugTraversalNodes) +
					" nodes. The BVH may contain invalid child links.";
				return false;
			}

			if (entry.nodeIndex >= m_nodes.size()) {
				outDebugMessage =
					"Leaf debug traversal hit invalid node index " +
					std::to_string(entry.nodeIndex) + ".";
				return false;
			}

			if (visitCounts[entry.nodeIndex] != 0u) {
				outDebugMessage =
					"Leaf debug traversal detected a cycle or duplicate child reference at node " +
					std::to_string(entry.nodeIndex) + ".";
				return false;
			}
			visitCounts[entry.nodeIndex] = 1u;

			const auto& node = m_nodes[entry.nodeIndex];
			if (node.isLeaf || entry.depth >= targetDisplayDepth) {
				if (emittedLeafBoxes >= kMaxDebugLeafBoxes) {
					outDebugMessage =
						"Leaf debug draw capped at " +
						std::to_string(kMaxDebugLeafBoxes) +
						" bounds to keep the editor responsive.";
					return true;
				}

				AppendBoundsWireframe(outLeafs, node.bounds);
				++emittedLeafBoxes;
				continue;
			}

			uint32_t leftChild = kInvalidNodeIndex;
			uint32_t rightChild = kInvalidNodeIndex;
			if (!TryGetValidChildren(entry.nodeIndex, leftChild, rightChild)) {
				outDebugMessage =
					"Leaf debug traversal found invalid children at node " +
					std::to_string(entry.nodeIndex) + ".";
				return false;
			}

			stack.push_back({ leftChild, entry.depth + 1 });
			stack.push_back({ rightChild, entry.depth + 1 });
		}

		if (outLeafs.empty()) {
			outDebugMessage =
				"No bounds matched the current depth filter.";
		} else {
			outDebugMessage =
				"Showing " + std::to_string(emittedLeafBoxes) +
				" BVH bounds at depth " + std::to_string(targetDisplayDepth) +
				" of " + std::to_string(m_stats.maxDepth) + ".";
		}

		return true;
	}

	SceneBakeBVHSessionState& GetSceneBakeBVHSessionState() {
		return SessionStateStorage();
	}

	bool BuildSceneBakeBVHFromCurrentScene(const SceneBakeBVHBuildSettings& settings) {
		auto& state = SessionStateStorage();
		const SceneBakeBVHDebugOptions debugOptions = state.debugOptions;
		const std::string selfCheckMessage = state.selfCheckMessage;
		const std::string debugStatusMessage = state.debugStatusMessage;
		const bool selfCheckPassed = state.selfCheckPassed;
		state = {};
		state.settings = settings;
		state.debugOptions = debugOptions;
		state.selfCheckMessage = selfCheckMessage;
		state.debugStatusMessage = debugStatusMessage;
		state.selfCheckPassed = selfCheckPassed;
		InvalidateDebugGeometryCache(state);

		BakeGeometryCollection collection = CollectSceneBakeGeometry();
		state.geometryStats = collection.stats;
		state.sceneBounds = InvalidBounds();
		for (const auto& triangle : collection.triangles) {
			if (!IsFiniteBounds(triangle.bounds)) {
				continue;
			}
			state.sceneBounds = UnionBounds(state.sceneBounds, triangle.bounds);
		}
		state.sceneDiagonalLength = BoundsDiagonalLength(state.sceneBounds);
		state.warnings = collection.warnings;
		RebuildWarningCounts(state);

		g_sceneBakeBVH = std::make_unique<SceneBakeBVH>();
		if (!g_sceneBakeBVH->Build(collection, settings)) {
			state.hasValidBVH = false;
			state.statusMessage = collection.triangles.empty()
				? "No bake triangles were collected from the current scene."
				: "BVH build failed because all collected triangles were invalid.";
			state.stats = g_sceneBakeBVH->GetStats();
			SPD_WARNING("Scene bake BVH build failed. " << state.statusMessage);
			return false;
		}

		state.hasValidBVH = true;
		state.stats = g_sceneBakeBVH->GetStats();
		state.statusMessage =
			"Built scene bake BVH from " +
			std::to_string(state.geometryStats.includedEntityCount) +
			" static renderers and " +
			std::to_string(state.stats.triangleCount) +
			" triangles.";

		SPD_INFO(
			"Built scene bake BVH: "
			<< state.stats.triangleCount << " triangles, "
			<< state.stats.nodeCount << " nodes, "
			<< state.stats.leafCount << " leaves in "
			<< state.stats.buildMs << " ms.");
		return true;
	}

	void ClearSceneBakeBVH() {
		g_sceneBakeBVH.reset();
		auto& state = SessionStateStorage();
		const SceneBakeBVHDebugOptions debugOptions = state.debugOptions;
		const std::string selfCheckMessage = state.selfCheckMessage;
		const std::string debugStatusMessage = state.debugStatusMessage;
		const bool selfCheckPassed = state.selfCheckPassed;
		state = {};
		state.debugOptions = debugOptions;
		state.selfCheckMessage = selfCheckMessage;
		state.debugStatusMessage = debugStatusMessage;
		state.selfCheckPassed = selfCheckPassed;
		InvalidateDebugGeometryCache(state);
		state.statusMessage = "Scene bake BVH cleared.";
	}

	bool HasValidSceneBakeBVH() {
		return g_sceneBakeBVH && g_sceneBakeBVH->IsValid();
	}

	bool SceneBakeBVHAnyHit(const BakeRay& ray) {
		return g_sceneBakeBVH && g_sceneBakeBVH->AnyHit(ray);
	}

	bool SceneBakeBVHClosestHit(const BakeRay& ray, BakeHit& outHit) {
		return g_sceneBakeBVH && g_sceneBakeBVH->ClosestHit(ray, outHit);
	}

	void DrawSceneBakeBVHDebug() {
		if (!g_sceneBakeBVH || !g_sceneBakeBVH->IsValid()) {
			return;
		}

		auto& state = SessionStateStorage();
		const int clampedLeafDepth = std::max(state.debugOptions.leafDepth, 0);
		const bool cacheMatches =
			state.debugGeometryCacheValid &&
			state.cachedDrawRootBounds == state.debugOptions.drawRootBounds &&
			state.cachedDrawLeafBounds == state.debugOptions.drawLeafBounds &&
			state.cachedLeafDepth == clampedLeafDepth;

		if (!cacheMatches) {
			state.cachedRootDebugLines.clear();
			state.cachedLeafDebugLines.clear();
			state.cachedRootDebugLines.reserve(24);
			state.debugStatusMessage.clear();

			const bool debugBuildOk = g_sceneBakeBVH->AppendDebugLineVertices(
				state.debugOptions.drawRootBounds,
				state.debugOptions.drawLeafBounds,
				clampedLeafDepth,
				state.cachedRootDebugLines,
				state.cachedLeafDebugLines,
				state.debugStatusMessage);

			state.cachedDrawRootBounds = state.debugOptions.drawRootBounds;
			state.cachedDrawLeafBounds = state.debugOptions.drawLeafBounds;
			state.cachedLeafDepth = clampedLeafDepth;
			state.debugGeometryCacheValid = debugBuildOk || !state.debugStatusMessage.empty();
		}

		if (!state.cachedRootDebugLines.empty()) {
			NE::Renderer::Command::AddDebugLinesBatch(state.cachedRootDebugLines, { 1.0f, 0.8f, 0.15f });
		}
		if (!state.cachedLeafDebugLines.empty()) {
			NE::Renderer::Command::AddDebugLinesBatch(state.cachedLeafDebugLines, { 0.25f, 0.95f, 0.95f });
		}
	}

	bool RunSceneBakeBVHSelfCheck(std::string& outMessage) {
		BakeGeometryCollection collection{};
		collection.sources.push_back({
			1u,
			42u,
			"SelfCheckTriangle",
			"builtin:model/triangle",
			"builtin:material/default",
			0u
		});

		BakeTriangle primary{};
		primary.p0 = { 0.0f, 0.0f, 0.0f };
		primary.p1 = { 1.0f, 0.0f, 0.0f };
		primary.p2 = { 0.0f, 1.0f, 0.0f };
		primary.geometricNormal = { 0.0f, 0.0f, 1.0f };
		primary.shadingNormal0 = primary.geometricNormal;
		primary.shadingNormal1 = primary.geometricNormal;
		primary.shadingNormal2 = primary.geometricNormal;
		primary.centroid = { 1.0f / 3.0f, 1.0f / 3.0f, 0.0f };
		primary.bounds = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
		primary.area = 0.5f;
		primary.sourceIndex = 0u;
		primary.originalIndex = 0u;
		collection.triangles.push_back(primary);

		BakeTriangle degenerate = primary;
		degenerate.p2 = primary.p1;
		degenerate.bounds = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
		degenerate.area = 0.0f;
		degenerate.originalIndex = 1u;
		collection.triangles.push_back(degenerate);

		BakeTriangle secondary = primary;
		secondary.p0 = { 2.0f, 0.0f, 0.0f };
		secondary.p1 = { 3.0f, 0.0f, 0.0f };
		secondary.p2 = { 2.0f, 1.0f, 0.0f };
		secondary.centroid = { 7.0f / 3.0f, 1.0f / 3.0f, 0.0f };
		secondary.bounds = { { 2.0f, 0.0f, 0.0f }, { 3.0f, 1.0f, 0.0f } };
		secondary.originalIndex = 2u;
		collection.triangles.push_back(secondary);

		SceneBakeBVH bvh;
		SceneBakeBVHBuildSettings settings{};
		settings.maxLeafPrimitives = 1;
		if (!bvh.Build(collection, settings)) {
			outMessage = "Self-check failed: synthetic BVH did not build.";
			return false;
		}

		const auto& stats = bvh.GetStats();
		if (stats.triangleCount != 2 || stats.skippedInvalidTriangleCount != 1 || stats.nodeCount < 3) {
			outMessage = "Self-check failed: invalid triangle filtering did not match expectations.";
			return false;
		}

		std::vector<NE::Math::Vec3> debugRootLines;
		std::vector<NE::Math::Vec3> debugLeafLines;
		std::string debugMessage;
		if (!bvh.AppendDebugLineVertices(true, true, 1, debugRootLines, debugLeafLines, debugMessage)) {
			outMessage = "Self-check failed: debug traversal reported invalid BVH links.";
			return false;
		}

		BakeRay missRay{};
		missRay.origin = { 2.0f, 2.0f, 1.0f };
		missRay.direction = { 0.0f, 0.0f, -1.0f };
		missRay.tMin = 0.0f;
		missRay.tMax = 10.0f;
		if (bvh.AnyHit(missRay)) {
			outMessage = "Self-check failed: miss ray reported an occluder.";
			return false;
		}

		BakeRay hitRay{};
		hitRay.origin = { 0.25f, 0.25f, 1.0f };
		hitRay.direction = { 0.0f, 0.0f, -1.0f };
		hitRay.tMin = 0.0f;
		hitRay.tMax = 10.0f;

		if (!bvh.AnyHit(hitRay)) {
			outMessage = "Self-check failed: any-hit ray missed the synthetic triangle.";
			return false;
		}

		BakeHit hit{};
		if (!bvh.ClosestHit(hitRay, hit)) {
			outMessage = "Self-check failed: closest-hit ray missed the synthetic triangle.";
			return false;
		}

		if (std::fabs(hit.t - 1.0f) > 1e-4f) {
			outMessage = "Self-check failed: closest-hit distance was incorrect.";
			return false;
		}

		if (std::fabs(hit.position.x - 0.25f) > 1e-4f ||
			std::fabs(hit.position.y - 0.25f) > 1e-4f ||
			std::fabs(hit.position.z) > 1e-4f) {
			outMessage = "Self-check failed: hit position was incorrect.";
			return false;
		}

		outMessage = "Scene bake BVH self-check passed.";
		return true;
	}
}
