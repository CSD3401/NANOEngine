#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <Engine.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/LightmapBinding.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Transform.hpp>
#include <Graphics/Core/Model.hpp>
#include <Math/Mat4.hpp>
#include <Math/Vec2.hpp>
#include <Math/Vec3.hpp>

namespace Editor::Lightmapping {

	inline constexpr int kDefaultLightmapPageSize = 2048;
	inline constexpr int kDefaultLightmapPadding = 8;
	inline constexpr int kDefaultLightmapMinUvIslandGap = 8;
	inline constexpr float kDefaultLightmapTexelsPerUnit = 16.0f;
	inline constexpr float kDefaultMinUvCoverage = 0.05f;
	inline constexpr float kDefaultMaxAspectRatio = 4.0f;

	enum class LightmapEntityStatusKind {
		None,
		OptedOut,
		Allocated,
		Unresolved,
		Skipped
	};

	enum class LightmapFailureReason {
		None,
		OptedOut,
		Inactive,
		MissingRenderer,
		MissingModel,
		InvalidSubmesh,
		MissingUv1,
		InvalidTransform,
		ZeroAreaGeometry,
		RectExceedsPageSize
	};

	struct LightmapAllocationSettings {
		float texelsPerUnit = kDefaultLightmapTexelsPerUnit;
		int pageSize = kDefaultLightmapPageSize;
		int padding = kDefaultLightmapPadding;
		int minUvIslandGap = kDefaultLightmapMinUvIslandGap;
		float minCoverage = kDefaultMinUvCoverage;
		float maxAspectRatio = kDefaultMaxAspectRatio;
	};

	struct LightmapAllocationInput {
		uint32_t entity = NE::ECS::NO_ENTITY;
		uint64_t stableId = 0;
		std::string entityName;
		float worldSurfaceArea = 0.0f;
		float uvCoverage = 0.0f;
		float uvAspectRatio = 1.0f;
		int baseInnerWidth = 0;
		int baseInnerHeight = 0;
		int innerWidth = 0;
		int innerHeight = 0;
		int totalWidth = 0;
		int totalHeight = 0;
		int effectiveUvIslandGap = std::numeric_limits<int>::max();
		int uvIslandChartCount = 0;
		int rasterizedUvIslandChartCount = 0;
		size_t uvIslandOverlapTexelCount = 0;
		bool expandedForUvIslandGap = false;
		bool uvIslandGapLimitedByPageSize = false;
		bool hadBinding = false;
	};

	struct LightmapPlacement {
		uint32_t entity = NE::ECS::NO_ENTITY;
		std::string entityName;
		int pageIndex = -1;
		std::string pageId;
		int outerX = 0;
		int outerY = 0;
		int outerWidth = 0;
		int outerHeight = 0;
		int innerX = 0;
		int innerY = 0;
		int innerWidth = 0;
		int innerHeight = 0;
		NE::Math::Vec2 uvScale = { 1.0f, 1.0f };
		NE::Math::Vec2 uvOffset = { 0.0f, 0.0f };
	};

	struct LightmapAtlasPage {
		int pageIndex = -1;
		std::string pageId;
		int width = 0;
		int height = 0;
		int usedArea = 0;
		std::vector<LightmapPlacement> placements;
	};

	struct AllocationReportEntry {
		uint32_t entity = NE::ECS::NO_ENTITY;
		std::string entityName;
		LightmapEntityStatusKind status = LightmapEntityStatusKind::None;
		LightmapFailureReason reason = LightmapFailureReason::None;
		bool temporary = false;
		std::string message;
	};

	struct AllocationReport {
		size_t optedOutEntityCount = 0;
		size_t consideredEntityCount = 0;
		size_t eligibleEntityCount = 0;
		size_t allocatedEntityCount = 0;
		size_t skippedEntityCount = 0;
		size_t totalPages = 0;
		std::map<std::string, int> failureCounts;
		std::vector<AllocationReportEntry> entries;
	};

	struct LightmapEntityPreviewStatus {
		LightmapEntityStatusKind kind = LightmapEntityStatusKind::None;
		LightmapFailureReason reason = LightmapFailureReason::None;
		bool temporary = false;
		bool hadBinding = false;
		std::string message;
		std::string pageId;
		NE::Math::Vec2 uvScale = { 1.0f, 1.0f };
		NE::Math::Vec2 uvOffset = { 0.0f, 0.0f };
	};

	struct LightmapAllocationResult {
		LightmapAllocationSettings settings{};
		AllocationReport report{};
		std::vector<LightmapAtlasPage> pages;
		std::unordered_map<uint32_t, LightmapPlacement> placements;
	};

	struct LightmapAllocationPreviewState {
		bool hasRun = false;
		LightmapAllocationSettings settings{};
		AllocationReport report{};
		std::vector<LightmapAtlasPage> pages;
		std::unordered_map<uint32_t, LightmapEntityPreviewStatus> entityStatuses;
	};

	inline LightmapAllocationPreviewState& GetLightmapAllocationPreviewState() {
		static LightmapAllocationPreviewState state;
		return state;
	}

	inline const LightmapEntityPreviewStatus* FindLightmapEntityPreviewStatus(uint32_t entity) {
		const auto& state = GetLightmapAllocationPreviewState();
		const auto it = state.entityStatuses.find(entity);
		return it == state.entityStatuses.end() ? nullptr : &it->second;
	}

	inline const char* ToString(LightmapEntityStatusKind kind) {
		switch (kind) {
		case LightmapEntityStatusKind::OptedOut: return "Opted Out";
		case LightmapEntityStatusKind::Allocated: return "Allocated";
		case LightmapEntityStatusKind::Unresolved: return "Unresolved";
		case LightmapEntityStatusKind::Skipped: return "Skipped";
		default: return "Not Run";
		}
	}

	inline const char* ToString(LightmapFailureReason reason) {
		switch (reason) {
		case LightmapFailureReason::OptedOut: return "Opted Out";
		case LightmapFailureReason::Inactive: return "Inactive";
		case LightmapFailureReason::MissingRenderer: return "Missing Renderer";
		case LightmapFailureReason::MissingModel: return "Missing Model";
		case LightmapFailureReason::InvalidSubmesh: return "Invalid Submesh";
		case LightmapFailureReason::MissingUv1: return "Missing UV1";
		case LightmapFailureReason::InvalidTransform: return "Invalid Transform";
		case LightmapFailureReason::ZeroAreaGeometry: return "Zero-Area Geometry";
		case LightmapFailureReason::RectExceedsPageSize: return "Rect Exceeds Page";
		default: return "None";
		}
	}

	namespace Detail {
		struct IntRect {
			int x = 0;
			int y = 0;
			int w = 0;
			int h = 0;
		};

		struct MeshMetrics {
			float worldSurfaceArea = 0.0f;
			float uvCoverage = 0.0f;
			float uvAspectRatio = 1.0f;
			bool hasAnyUvArea = false;
		};

		struct UvIslandSpacingAnalysis {
			bool valid = false;
			int chartCount = 0;
			int rasterizedChartCount = 0;
			int minGapTexels = std::numeric_limits<int>::max();
			size_t overlapTexelCount = 0u;
		};

		class DisjointSet {
		public:
			explicit DisjointSet(size_t count)
				: m_parent(count),
				m_rank(count, 0u) {
				for (size_t i = 0; i < count; ++i) {
					m_parent[i] = static_cast<uint32_t>(i);
				}
			}

			uint32_t Find(uint32_t value) {
				uint32_t parent = m_parent[value];
				if (parent != value) {
					parent = Find(parent);
					m_parent[value] = parent;
				}
				return parent;
			}

			void Unite(uint32_t lhs, uint32_t rhs) {
				uint32_t rootLhs = Find(lhs);
				uint32_t rootRhs = Find(rhs);
				if (rootLhs == rootRhs) {
					return;
				}

				if (m_rank[rootLhs] < m_rank[rootRhs]) {
					std::swap(rootLhs, rootRhs);
				}

				m_parent[rootRhs] = rootLhs;
				if (m_rank[rootLhs] == m_rank[rootRhs]) {
					++m_rank[rootLhs];
				}
			}

		private:
			std::vector<uint32_t> m_parent;
			std::vector<uint8_t> m_rank;
		};

		inline bool IsFiniteMatrix(const NE::Math::Mat4& matrix) {
			for (int i = 0; i < 16; ++i) {
				if (!std::isfinite(matrix.a[i])) {
					return false;
				}
			}
			return true;
		}

		inline NE::Math::Vec3 TransformPoint(const NE::Math::Mat4& matrix, const NE::Math::Vec3& point) {
			return matrix * point;
		}

		inline bool IsFiniteVec2(const NE::Math::Vec2& value) {
			return std::isfinite(value.x) && std::isfinite(value.y);
		}

		inline float EdgeFunction(const NE::Math::Vec2& a, const NE::Math::Vec2& b, const NE::Math::Vec2& p) {
			return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
		}

		inline bool IsTopLeftEdge(const NE::Math::Vec2& a, const NE::Math::Vec2& b, float edgeEpsilon) {
			const NE::Math::Vec2 edge = b - a;
			return (edge.y > edgeEpsilon) || (std::fabs(edge.y) <= edgeEpsilon && edge.x < 0.0f);
		}

		inline bool PassesFillRule(float edgeValue, bool topLeft, bool positiveArea, float edgeEpsilon) {
			if (positiveArea) {
				return edgeValue > edgeEpsilon || (std::fabs(edgeValue) <= edgeEpsilon && topLeft);
			}
			return edgeValue < -edgeEpsilon || (std::fabs(edgeValue) <= edgeEpsilon && topLeft);
		}

		inline bool BuildTriangleChartLabels(
			const NE::Graphics::SubMesh& submesh,
			std::vector<uint32_t>& outChartLabels,
			int& outChartCount) {
			outChartLabels.clear();
			outChartCount = 0;

			const size_t triangleCount = submesh.indices.size() / 3u;
			if (triangleCount == 0u) {
				return false;
			}

			DisjointSet sets(triangleCount);
			std::vector<std::vector<uint32_t>> trianglesByVertex(submesh.vertices.size());
			for (size_t triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
				for (size_t corner = 0; corner < 3u; ++corner) {
					const size_t indexOffset = triangleIndex * 3u + corner;
					if (indexOffset >= submesh.indices.size()) {
						return false;
					}

					const uint32_t vertexIndex = submesh.indices[indexOffset];
					if (vertexIndex >= submesh.vertices.size()) {
						return false;
					}

					trianglesByVertex[vertexIndex].push_back(static_cast<uint32_t>(triangleIndex));
				}
			}

			for (const auto& triangleList : trianglesByVertex) {
				if (triangleList.size() < 2u) {
					continue;
				}

				const uint32_t baseTriangle = triangleList.front();
				for (size_t i = 1; i < triangleList.size(); ++i) {
					sets.Unite(baseTriangle, triangleList[i]);
				}
			}

			std::unordered_map<uint32_t, uint32_t> denseLabelsByRoot;
			denseLabelsByRoot.reserve(triangleCount);
			outChartLabels.resize(triangleCount, 0u);
			for (size_t triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
				const uint32_t root = sets.Find(static_cast<uint32_t>(triangleIndex));
				auto [it, inserted] = denseLabelsByRoot.emplace(root, static_cast<uint32_t>(denseLabelsByRoot.size() + 1u));
				if (inserted) {
					++outChartCount;
				}
				outChartLabels[triangleIndex] = it->second;
			}

			return true;
		}

		inline UvIslandSpacingAnalysis AnalyzeUvIslandSpacing(
			const NE::Graphics::SubMesh& submesh,
			int width,
			int height) {
			UvIslandSpacingAnalysis analysis{};
			if (width <= 0 || height <= 0 || !submesh.hasUv1 || submesh.vertices.empty() || submesh.indices.size() < 3u) {
				return analysis;
			}

			std::vector<uint32_t> chartLabels;
			if (!BuildTriangleChartLabels(submesh, chartLabels, analysis.chartCount) || analysis.chartCount <= 0) {
				return analysis;
			}

			analysis.valid = true;
			if (analysis.chartCount <= 1) {
				return analysis;
			}

			const float edgeEpsilon = 1e-6f;
			const size_t texelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
			std::vector<uint32_t> ownerLabels(texelCount, 0u);
			std::vector<uint8_t> chartHasTexels(static_cast<size_t>(analysis.chartCount) + 1u, 0u);

			for (size_t triangleIndex = 0; triangleIndex < chartLabels.size(); ++triangleIndex) {
				const size_t baseIndex = triangleIndex * 3u;
				if (baseIndex + 2u >= submesh.indices.size()) {
					break;
				}

				const uint32_t ia = submesh.indices[baseIndex + 0u];
				const uint32_t ib = submesh.indices[baseIndex + 1u];
				const uint32_t ic = submesh.indices[baseIndex + 2u];
				if (ia >= submesh.vertices.size() || ib >= submesh.vertices.size() || ic >= submesh.vertices.size()) {
					continue;
				}

				const auto& va = submesh.vertices[ia];
				const auto& vb = submesh.vertices[ib];
				const auto& vc = submesh.vertices[ic];
				if (!IsFiniteVec2(va.texCoord1) || !IsFiniteVec2(vb.texCoord1) || !IsFiniteVec2(vc.texCoord1)) {
					continue;
				}

				const NE::Math::Vec2 page0{
					va.texCoord1.x * static_cast<float>(width),
					va.texCoord1.y * static_cast<float>(height)
				};
				const NE::Math::Vec2 page1{
					vb.texCoord1.x * static_cast<float>(width),
					vb.texCoord1.y * static_cast<float>(height)
				};
				const NE::Math::Vec2 page2{
					vc.texCoord1.x * static_cast<float>(width),
					vc.texCoord1.y * static_cast<float>(height)
				};

				const float signedArea = EdgeFunction(page0, page1, page2);
				if (!std::isfinite(signedArea) || std::fabs(signedArea) <= edgeEpsilon) {
					continue;
				}

				const bool positiveArea = signedArea > 0.0f;
				const bool topLeft0 = IsTopLeftEdge(page1, page2, edgeEpsilon);
				const bool topLeft1 = IsTopLeftEdge(page2, page0, edgeEpsilon);
				const bool topLeft2 = IsTopLeftEdge(page0, page1, edgeEpsilon);

				const float boundsMinX = std::min(page0.x, std::min(page1.x, page2.x));
				const float boundsMinY = std::min(page0.y, std::min(page1.y, page2.y));
				const float boundsMaxX = std::max(page0.x, std::max(page1.x, page2.x));
				const float boundsMaxY = std::max(page0.y, std::max(page1.y, page2.y));

				const int startX = std::max(0, static_cast<int>(std::ceil(boundsMinX - 0.5f)));
				const int startY = std::max(0, static_cast<int>(std::ceil(boundsMinY - 0.5f)));
				const int endX = std::min(width - 1, static_cast<int>(std::floor(boundsMaxX - 0.5f)));
				const int endY = std::min(height - 1, static_cast<int>(std::floor(boundsMaxY - 0.5f)));
				if (startX > endX || startY > endY) {
					continue;
				}

				const uint32_t chartLabel = chartLabels[triangleIndex];
				for (int y = startY; y <= endY; ++y) {
					for (int x = startX; x <= endX; ++x) {
						const NE::Math::Vec2 texelCenter{
							static_cast<float>(x) + 0.5f,
							static_cast<float>(y) + 0.5f
						};

						const float edge0 = EdgeFunction(page1, page2, texelCenter);
						const float edge1 = EdgeFunction(page2, page0, texelCenter);
						const float edge2 = EdgeFunction(page0, page1, texelCenter);
						if (!PassesFillRule(edge0, topLeft0, positiveArea, edgeEpsilon) ||
							!PassesFillRule(edge1, topLeft1, positiveArea, edgeEpsilon) ||
							!PassesFillRule(edge2, topLeft2, positiveArea, edgeEpsilon)) {
							continue;
						}

						const size_t linearIndex = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
						if (ownerLabels[linearIndex] == 0u) {
							ownerLabels[linearIndex] = chartLabel;
							chartHasTexels[chartLabel] = 1u;
						} else if (ownerLabels[linearIndex] != chartLabel) {
							++analysis.overlapTexelCount;
						}
					}
				}
			}

			analysis.rasterizedChartCount = static_cast<int>(std::count_if(
				chartHasTexels.begin() + 1,
				chartHasTexels.end(),
				[](uint8_t value) { return value != 0u; }));

			if (analysis.overlapTexelCount > 0u || analysis.rasterizedChartCount < analysis.chartCount) {
				analysis.minGapTexels = 0;
				return analysis;
			}

			std::vector<int> distances(texelCount, -1);
			std::vector<uint32_t> propagatedOwner = ownerLabels;
			std::vector<size_t> queue;
			queue.reserve(texelCount);
			for (size_t texelIndex = 0; texelIndex < texelCount; ++texelIndex) {
				if (ownerLabels[texelIndex] != 0u) {
					distances[texelIndex] = 0;
					queue.push_back(texelIndex);
				}
			}

			size_t queueHead = 0u;
			int minGapTexels = std::numeric_limits<int>::max();
			while (queueHead < queue.size()) {
				const size_t currentIndex = queue[queueHead++];
				const int x = static_cast<int>(currentIndex % static_cast<size_t>(width));
				const int y = static_cast<int>(currentIndex / static_cast<size_t>(width));

				for (int offsetY = -1; offsetY <= 1; ++offsetY) {
					for (int offsetX = -1; offsetX <= 1; ++offsetX) {
						if (offsetX == 0 && offsetY == 0) {
							continue;
						}

						const int neighborX = x + offsetX;
						const int neighborY = y + offsetY;
						if (neighborX < 0 || neighborY < 0 || neighborX >= width || neighborY >= height) {
							continue;
						}

						const size_t neighborIndex =
							static_cast<size_t>(neighborY) * static_cast<size_t>(width) +
							static_cast<size_t>(neighborX);
						if (propagatedOwner[neighborIndex] == 0u) {
							propagatedOwner[neighborIndex] = propagatedOwner[currentIndex];
							distances[neighborIndex] = distances[currentIndex] + 1;
							queue.push_back(neighborIndex);
							continue;
						}

						if (propagatedOwner[neighborIndex] == propagatedOwner[currentIndex]) {
							continue;
						}

						const int candidateGap = distances[currentIndex] + distances[neighborIndex] - 1;
						minGapTexels = std::min(minGapTexels, candidateGap);
					}
				}
			}

			if (minGapTexels == std::numeric_limits<int>::max()) {
				minGapTexels = 0;
			}

			analysis.minGapTexels = std::max(minGapTexels, 0);
			return analysis;
		}

		inline void EnforceMinUvIslandGap(
			const NE::Graphics::SubMesh& submesh,
			const LightmapAllocationSettings& settings,
			int& ioInnerWidth,
			int& ioInnerHeight,
			UvIslandSpacingAnalysis& outAnalysis,
			bool& outLimitedByPageSize) {
			outLimitedByPageSize = false;
			outAnalysis = AnalyzeUvIslandSpacing(submesh, ioInnerWidth, ioInnerHeight);
			if (settings.minUvIslandGap <= 0 || !outAnalysis.valid || outAnalysis.chartCount <= 1) {
				return;
			}

			const int maxInnerWidth = std::max(1, settings.pageSize - (settings.padding * 2));
			const int maxInnerHeight = std::max(1, settings.pageSize - (settings.padding * 2));
			for (int iteration = 0; iteration < 8; ++iteration) {
				const bool meetsGapTarget =
					outAnalysis.overlapTexelCount == 0u &&
					outAnalysis.rasterizedChartCount == outAnalysis.chartCount &&
					outAnalysis.minGapTexels >= settings.minUvIslandGap;
				if (meetsGapTarget) {
					return;
				}

				if (ioInnerWidth >= maxInnerWidth && ioInnerHeight >= maxInnerHeight) {
					outLimitedByPageSize = true;
					return;
				}

				double scale = 1.0;
				if (outAnalysis.overlapTexelCount > 0u ||
					outAnalysis.rasterizedChartCount < outAnalysis.chartCount ||
					outAnalysis.minGapTexels <= 0) {
					scale = 2.0;
				} else {
					scale = static_cast<double>(settings.minUvIslandGap + 1) /
						static_cast<double>(outAnalysis.minGapTexels + 1);
					scale = std::max(scale, 1.10);
				}

				const int nextInnerWidth = std::clamp(
					std::max(ioInnerWidth + 1, static_cast<int>(std::ceil(static_cast<double>(ioInnerWidth) * scale))),
					1,
					maxInnerWidth);
				const int nextInnerHeight = std::clamp(
					std::max(ioInnerHeight + 1, static_cast<int>(std::ceil(static_cast<double>(ioInnerHeight) * scale))),
					1,
					maxInnerHeight);
				if (nextInnerWidth == ioInnerWidth && nextInnerHeight == ioInnerHeight) {
					outLimitedByPageSize = true;
					return;
				}

				ioInnerWidth = nextInnerWidth;
				ioInnerHeight = nextInnerHeight;
				outAnalysis = AnalyzeUvIslandSpacing(submesh, ioInnerWidth, ioInnerHeight);
			}

			outLimitedByPageSize =
				outAnalysis.overlapTexelCount > 0u ||
				outAnalysis.rasterizedChartCount < outAnalysis.chartCount ||
				outAnalysis.minGapTexels < settings.minUvIslandGap;
		}

		inline float TriangleWorldArea(
			const NE::Math::Mat4& worldMatrix,
			const NE::Graphics::Vertex& a,
			const NE::Graphics::Vertex& b,
			const NE::Graphics::Vertex& c) {
			const NE::Math::Vec3 wa = TransformPoint(worldMatrix, a.position);
			const NE::Math::Vec3 wb = TransformPoint(worldMatrix, b.position);
			const NE::Math::Vec3 wc = TransformPoint(worldMatrix, c.position);
			return 0.5f * (wb - wa).Cross(wc - wa).Length();
		}

		inline float TriangleUvArea(
			const NE::Graphics::Vertex& a,
			const NE::Graphics::Vertex& b,
			const NE::Graphics::Vertex& c) {
			const NE::Math::Vec2 ab = b.texCoord1 - a.texCoord1;
			const NE::Math::Vec2 ac = c.texCoord1 - a.texCoord1;
			return 0.5f * std::fabs(ab.Cross(ac));
		}

		inline MeshMetrics ComputeMeshMetrics(
			const NE::Graphics::SubMesh& submesh,
			const NE::Math::Mat4& worldMatrix,
			float maxAspectRatio) {
			MeshMetrics metrics{};
			if (!submesh.hasUv1 || submesh.indices.size() < 3 || submesh.vertices.empty()) {
				return metrics;
			}

			NE::Math::Vec2 uvMin{
				std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max()
			};
			NE::Math::Vec2 uvMax{
				-std::numeric_limits<float>::max(),
				-std::numeric_limits<float>::max()
			};

			float totalUvArea = 0.0f;
			for (size_t i = 0; i + 2 < submesh.indices.size(); i += 3) {
				const uint32_t ia = submesh.indices[i + 0];
				const uint32_t ib = submesh.indices[i + 1];
				const uint32_t ic = submesh.indices[i + 2];
				if (ia >= submesh.vertices.size() || ib >= submesh.vertices.size() || ic >= submesh.vertices.size()) {
					continue;
				}

				const auto& va = submesh.vertices[ia];
				const auto& vb = submesh.vertices[ib];
				const auto& vc = submesh.vertices[ic];

				metrics.worldSurfaceArea += TriangleWorldArea(worldMatrix, va, vb, vc);
				totalUvArea += TriangleUvArea(va, vb, vc);

				const NE::Math::Vec2 uvs[] = { va.texCoord1, vb.texCoord1, vc.texCoord1 };
				for (const auto& uv : uvs) {
					uvMin.x = std::min(uvMin.x, uv.x);
					uvMin.y = std::min(uvMin.y, uv.y);
					uvMax.x = std::max(uvMax.x, uv.x);
					uvMax.y = std::max(uvMax.y, uv.y);
				}
			}

			metrics.uvCoverage = std::clamp(totalUvArea, 0.0f, 1.0f);
			metrics.hasAnyUvArea = totalUvArea > 0.0f;

			const float uvWidth = std::max(uvMax.x - uvMin.x, 0.0f);
			const float uvHeight = std::max(uvMax.y - uvMin.y, 0.0f);
			if (uvWidth > 1e-5f && uvHeight > 1e-5f) {
				const float rawAspect = uvWidth / uvHeight;
				metrics.uvAspectRatio = std::clamp(rawAspect, 1.0f / maxAspectRatio, maxAspectRatio);
			}

			return metrics;
		}

		inline int CeilToInt(float value) {
			return static_cast<int>(std::ceil(std::max(value, 0.0f)));
		}

		inline std::string FormatPageId(int pageIndex) {
			std::ostringstream stream;
			stream << "lm_page_" << std::setw(3) << std::setfill('0') << pageIndex;
			return stream.str();
		}

		inline bool Intersects(const IntRect& a, const IntRect& b) {
			return a.x < (b.x + b.w) &&
				(a.x + a.w) > b.x &&
				a.y < (b.y + b.h) &&
				(a.y + a.h) > b.y;
		}

		inline bool Contains(const IntRect& outer, const IntRect& inner) {
			return inner.x >= outer.x &&
				inner.y >= outer.y &&
				(inner.x + inner.w) <= (outer.x + outer.w) &&
				(inner.y + inner.h) <= (outer.y + outer.h);
		}

		class MaxRectsBin {
		public:
			MaxRectsBin(int width, int height) {
				m_freeRects.push_back({ 0, 0, width, height });
			}

			bool Insert(int width, int height, IntRect& outRect) {
				int bestShortSideFit = std::numeric_limits<int>::max();
				int bestLongSideFit = std::numeric_limits<int>::max();
				int bestIndex = -1;

				for (int i = 0; i < static_cast<int>(m_freeRects.size()); ++i) {
					const IntRect& freeRect = m_freeRects[i];
					if (width > freeRect.w || height > freeRect.h) {
						continue;
					}

					const int leftoverHoriz = std::abs(freeRect.w - width);
					const int leftoverVert = std::abs(freeRect.h - height);
					const int shortSideFit = std::min(leftoverHoriz, leftoverVert);
					const int longSideFit = std::max(leftoverHoriz, leftoverVert);

					if (shortSideFit < bestShortSideFit ||
						(shortSideFit == bestShortSideFit && longSideFit < bestLongSideFit) ||
						(shortSideFit == bestShortSideFit && longSideFit == bestLongSideFit &&
							(freeRect.y < m_freeRects[bestIndex].y ||
								(freeRect.y == m_freeRects[bestIndex].y && freeRect.x < m_freeRects[bestIndex].x)))) {
						bestIndex = i;
						bestShortSideFit = shortSideFit;
						bestLongSideFit = longSideFit;
						outRect = { freeRect.x, freeRect.y, width, height };
					}
				}

				if (bestIndex < 0) {
					return false;
				}

				for (size_t i = 0; i < m_freeRects.size();) {
					if (!SplitFreeRect(m_freeRects[i], outRect)) {
						++i;
						continue;
					}
					m_freeRects.erase(m_freeRects.begin() + static_cast<std::ptrdiff_t>(i));
				}

				PruneFreeList();
				return true;
			}

		private:
			bool SplitFreeRect(const IntRect& freeRect, const IntRect& usedRect) {
				if (!Intersects(freeRect, usedRect)) {
					return false;
				}

				if (usedRect.x > freeRect.x) {
					m_freeRects.push_back({ freeRect.x, freeRect.y, usedRect.x - freeRect.x, freeRect.h });
				}
				if ((usedRect.x + usedRect.w) < (freeRect.x + freeRect.w)) {
					m_freeRects.push_back({
						usedRect.x + usedRect.w,
						freeRect.y,
						(freeRect.x + freeRect.w) - (usedRect.x + usedRect.w),
						freeRect.h
					});
				}
				if (usedRect.y > freeRect.y) {
					m_freeRects.push_back({ freeRect.x, freeRect.y, freeRect.w, usedRect.y - freeRect.y });
				}
				if ((usedRect.y + usedRect.h) < (freeRect.y + freeRect.h)) {
					m_freeRects.push_back({
						freeRect.x,
						usedRect.y + usedRect.h,
						freeRect.w,
						(freeRect.y + freeRect.h) - (usedRect.y + usedRect.h)
					});
				}

				return true;
			}

			void PruneFreeList() {
				for (size_t i = 0; i < m_freeRects.size(); ++i) {
					if (m_freeRects[i].w <= 0 || m_freeRects[i].h <= 0) {
						m_freeRects.erase(m_freeRects.begin() + static_cast<std::ptrdiff_t>(i));
						--i;
						continue;
					}

					for (size_t j = i + 1; j < m_freeRects.size(); ++j) {
						if (Contains(m_freeRects[i], m_freeRects[j])) {
							m_freeRects.erase(m_freeRects.begin() + static_cast<std::ptrdiff_t>(j));
							--j;
						} else if (Contains(m_freeRects[j], m_freeRects[i])) {
							m_freeRects.erase(m_freeRects.begin() + static_cast<std::ptrdiff_t>(i));
							--i;
							break;
						}
					}
				}
			}

			std::vector<IntRect> m_freeRects;
		};

		inline void PushReportEntry(
			AllocationReport& report,
			LightmapAllocationPreviewState& previewState,
			uint32_t entity,
			const std::string& entityName,
			LightmapEntityStatusKind status,
			LightmapFailureReason reason,
			bool temporary,
			bool hadBinding,
			const std::string& message) {
			report.entries.push_back({ entity, entityName, status, reason, temporary, message });

			auto& preview = previewState.entityStatuses[entity];
			preview.kind = status;
			preview.reason = reason;
			preview.temporary = temporary;
			preview.hadBinding = hadBinding;
			preview.message = message;

			if (status == LightmapEntityStatusKind::Allocated) {
				return;
			}
			if (reason != LightmapFailureReason::None) {
				report.failureCounts[ToString(reason)]++;
			}
		}

		inline void DisableBindingKeepPlacement(uint32_t entity) {
			if (!NE::ECS::Query::HasComponent<NE::ECS::Component::LightmapBinding>(entity)) {
				return;
			}

			auto& binding = NE::ECS::Command::GetComponent<NE::ECS::Component::LightmapBinding>(entity);
			binding.enabled = false;
			binding.pageResolved = false;
			binding.resolvedPageSlot = NE::ECS::Component::INVALID_LIGHTMAP_PAGE_SLOT;
		}

		inline void DisableBindingAndClearPlacement(uint32_t entity) {
			if (!NE::ECS::Query::HasComponent<NE::ECS::Component::LightmapBinding>(entity)) {
				return;
			}

			auto& binding = NE::ECS::Command::GetComponent<NE::ECS::Component::LightmapBinding>(entity);
			binding.enabled = false;
			binding.pageId.clear();
			binding.uvScale = { 1.0f, 1.0f };
			binding.uvOffset = { 0.0f, 0.0f };
			binding.pageResolved = false;
			binding.resolvedPageSlot = NE::ECS::Component::INVALID_LIGHTMAP_PAGE_SLOT;
		}

		inline void ApplyPlacement(uint32_t entity, const LightmapPlacement& placement) {
			if (!NE::ECS::Query::HasComponent<NE::ECS::Component::LightmapBinding>(entity)) {
				NE::ECS::Command::AddLightmapBindingComponent(entity, NE::ECS::Component::LightmapBinding{});
			}

			auto& binding = NE::ECS::Command::GetComponent<NE::ECS::Component::LightmapBinding>(entity);
			binding.enabled = true;
			binding.pageId = placement.pageId;
			binding.uvScale = placement.uvScale;
			binding.uvOffset = placement.uvOffset;
			binding.pageResolved = false;
			binding.resolvedPageSlot = NE::ECS::Component::INVALID_LIGHTMAP_PAGE_SLOT;
		}
	}

	inline LightmapAllocationResult RunSceneLightmapAllocation(const LightmapAllocationSettings& settings) {
		LightmapAllocationResult result{};
		result.settings = settings;

		auto& previewState = GetLightmapAllocationPreviewState();
		previewState = {};
		previewState.hasRun = true;
		previewState.settings = settings;

		const auto& entities = NE::GetNumEntities();

		std::vector<LightmapAllocationInput> requests;
		requests.reserve(entities.size());

		for (uint32_t entity : entities) {
			if (!NE::ECS::Query::HasComponent<NE::ECS::Component::EntityMeta>(entity)) {
				continue;
			}

			const auto& meta = NE::ECS::Query::GetComponent<NE::ECS::Component::EntityMeta>(entity);
			const std::string entityName = meta.name;
			const bool hadBinding = NE::ECS::Query::HasComponent<NE::ECS::Component::LightmapBinding>(entity);

			if (!meta.isStatic) {
				result.report.optedOutEntityCount++;
				Detail::DisableBindingAndClearPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::OptedOut,
					LightmapFailureReason::OptedOut,
					false,
					hadBinding,
					"Static lightmap baking is disabled for this entity.");
				continue;
			}

			result.report.consideredEntityCount++;

			if (!NE::ECS::Query::GetActive(entity)) {
				Detail::DisableBindingKeepPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::Unresolved,
					LightmapFailureReason::Inactive,
					true,
					hadBinding,
					"Entity is inactive, so allocation is deferred.");
				result.report.skippedEntityCount++;
				continue;
			}

			if (!NE::ECS::Query::HasComponent<NE::ECS::Component::Renderer>(entity)) {
				Detail::DisableBindingKeepPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::Unresolved,
					LightmapFailureReason::MissingRenderer,
					true,
					hadBinding,
					"Entity is marked static for baking but has no Renderer component.");
				result.report.skippedEntityCount++;
				continue;
			}

			if (!NE::ECS::Query::HasComponent<NE::ECS::Component::Transform>(entity)) {
				Detail::DisableBindingKeepPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::Unresolved,
					LightmapFailureReason::InvalidTransform,
					true,
					hadBinding,
					"Entity transform is missing, so world-space metrics cannot be computed.");
				result.report.skippedEntityCount++;
				continue;
			}

			const auto& renderer = NE::ECS::Query::GetComponent<NE::ECS::Component::Renderer>(entity);
			const auto& transform = NE::ECS::Query::GetComponent<NE::ECS::Component::Transform>(entity);

			if (!Detail::IsFiniteMatrix(transform.worldMatrix)) {
				Detail::DisableBindingKeepPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::Unresolved,
					LightmapFailureReason::InvalidTransform,
					true,
					hadBinding,
					"World transform contains non-finite values.");
				result.report.skippedEntityCount++;
				continue;
			}

			if (!renderer.model) {
				Detail::DisableBindingKeepPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::Unresolved,
					LightmapFailureReason::MissingModel,
					true,
					hadBinding,
					"Renderer model is not loaded.");
				result.report.skippedEntityCount++;
				continue;
			}

			if (renderer.subMeshIndex < 0 ||
				renderer.subMeshIndex >= static_cast<int32_t>(renderer.model->meshes.size())) {
				Detail::DisableBindingKeepPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::Unresolved,
					LightmapFailureReason::InvalidSubmesh,
					true,
					hadBinding,
					"Renderer does not resolve to a valid single submesh.");
				result.report.skippedEntityCount++;
				continue;
			}

			const auto& submesh = renderer.model->meshes[renderer.subMeshIndex];
			if (!submesh.hasUv1) {
				Detail::DisableBindingAndClearPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::Skipped,
					LightmapFailureReason::MissingUv1,
					false,
					hadBinding,
					"Submesh is missing UV1 and cannot receive baked lighting.");
				result.report.skippedEntityCount++;
				continue;
			}

			const Detail::MeshMetrics metrics =
				Detail::ComputeMeshMetrics(submesh, transform.worldMatrix, settings.maxAspectRatio);
			if (!std::isfinite(metrics.worldSurfaceArea) || metrics.worldSurfaceArea <= 1e-5f) {
				Detail::DisableBindingAndClearPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::Skipped,
					LightmapFailureReason::ZeroAreaGeometry,
					false,
					hadBinding,
					"Submesh has zero usable world-space surface area.");
				result.report.skippedEntityCount++;
				continue;
			}

			const float clampedCoverage = std::clamp(metrics.uvCoverage, settings.minCoverage, 1.0f);
			const float texelArea =
				std::ceil(metrics.worldSurfaceArea * settings.texelsPerUnit * settings.texelsPerUnit / clampedCoverage);
			const float aspect = std::clamp(metrics.uvAspectRatio, 1.0f / settings.maxAspectRatio, settings.maxAspectRatio);
			const float rectHeight = std::sqrt(std::max(texelArea / aspect, 1.0f));
			const float rectWidth = rectHeight * aspect;

			const int baseInnerWidth = std::max(1, Detail::CeilToInt(rectWidth));
			const int baseInnerHeight = std::max(1, Detail::CeilToInt(rectHeight));
			int innerWidth = baseInnerWidth;
			int innerHeight = baseInnerHeight;
			Detail::UvIslandSpacingAnalysis uvIslandSpacing{};
			bool uvIslandGapLimitedByPageSize = false;
			Detail::EnforceMinUvIslandGap(submesh, settings, innerWidth, innerHeight, uvIslandSpacing, uvIslandGapLimitedByPageSize);
			const int totalWidth = innerWidth + (settings.padding * 2);
			const int totalHeight = innerHeight + (settings.padding * 2);

			if (totalWidth > settings.pageSize || totalHeight > settings.pageSize) {
				Detail::DisableBindingAndClearPlacement(entity);
				Detail::PushReportEntry(
					result.report,
					previewState,
					entity,
					entityName,
					LightmapEntityStatusKind::Skipped,
					LightmapFailureReason::RectExceedsPageSize,
					false,
					hadBinding,
					"Requested rectangle exceeds the fixed lightmap page size.");
				result.report.skippedEntityCount++;
				continue;
			}

			requests.push_back(LightmapAllocationInput{
				entity,
				meta.luid != 0 ? meta.luid : static_cast<uint64_t>(entity),
				entityName,
				metrics.worldSurfaceArea,
				clampedCoverage,
				aspect,
				baseInnerWidth,
				baseInnerHeight,
				innerWidth,
				innerHeight,
				totalWidth,
				totalHeight,
				uvIslandSpacing.minGapTexels,
				uvIslandSpacing.chartCount,
				uvIslandSpacing.rasterizedChartCount,
				uvIslandSpacing.overlapTexelCount,
				innerWidth != baseInnerWidth || innerHeight != baseInnerHeight,
				uvIslandGapLimitedByPageSize,
				hadBinding
			});
		}

		std::sort(requests.begin(), requests.end(),
			[](const LightmapAllocationInput& a, const LightmapAllocationInput& b) {
				const int areaA = a.totalWidth * a.totalHeight;
				const int areaB = b.totalWidth * b.totalHeight;
				if (areaA != areaB) return areaA > areaB;
				if (a.totalWidth != b.totalWidth) return a.totalWidth > b.totalWidth;
				if (a.totalHeight != b.totalHeight) return a.totalHeight > b.totalHeight;
				return a.stableId < b.stableId;
			});

		result.report.eligibleEntityCount = requests.size();

		std::vector<Detail::MaxRectsBin> pagePackers;
		for (const auto& request : requests) {
			Detail::IntRect packedRect{};
			int targetPage = -1;

			for (int pageIndex = 0; pageIndex < static_cast<int>(pagePackers.size()); ++pageIndex) {
				if (pagePackers[pageIndex].Insert(request.totalWidth, request.totalHeight, packedRect)) {
					targetPage = pageIndex;
					break;
				}
			}

			if (targetPage < 0) {
				pagePackers.emplace_back(settings.pageSize, settings.pageSize);
				result.pages.push_back({
					static_cast<int>(result.pages.size()),
					Detail::FormatPageId(static_cast<int>(result.pages.size())),
					settings.pageSize,
					settings.pageSize,
					0,
					{}
				});
				targetPage = static_cast<int>(pagePackers.size()) - 1;
				if (!pagePackers.back().Insert(request.totalWidth, request.totalHeight, packedRect)) {
					Detail::DisableBindingAndClearPlacement(request.entity);
					Detail::PushReportEntry(
						result.report,
						previewState,
						request.entity,
						request.entityName,
						LightmapEntityStatusKind::Skipped,
						LightmapFailureReason::RectExceedsPageSize,
						false,
						request.hadBinding,
						"Requested rectangle could not fit into a new lightmap page.");
					result.report.skippedEntityCount++;
					continue;
				}
			}

			LightmapPlacement placement{};
			placement.entity = request.entity;
			placement.entityName = request.entityName;
			placement.pageIndex = targetPage;
			placement.pageId = result.pages[targetPage].pageId;
			placement.outerX = packedRect.x;
			placement.outerY = packedRect.y;
			placement.outerWidth = request.totalWidth;
			placement.outerHeight = request.totalHeight;
			placement.innerX = packedRect.x + settings.padding;
			placement.innerY = packedRect.y + settings.padding;
			placement.innerWidth = request.innerWidth;
			placement.innerHeight = request.innerHeight;
			placement.uvScale = {
				static_cast<float>(request.innerWidth) / static_cast<float>(settings.pageSize),
				static_cast<float>(request.innerHeight) / static_cast<float>(settings.pageSize)
			};
			placement.uvOffset = {
				static_cast<float>(placement.innerX) / static_cast<float>(settings.pageSize),
				static_cast<float>(placement.innerY) / static_cast<float>(settings.pageSize)
			};

			result.pages[targetPage].usedArea += request.totalWidth * request.totalHeight;
			result.pages[targetPage].placements.push_back(placement);
			result.placements[request.entity] = placement;
			Detail::ApplyPlacement(request.entity, placement);

			auto& preview = previewState.entityStatuses[request.entity];
			preview.kind = LightmapEntityStatusKind::Allocated;
			preview.reason = LightmapFailureReason::None;
			preview.temporary = false;
			preview.hadBinding = request.hadBinding;
			preview.pageId = placement.pageId;
			preview.uvScale = placement.uvScale;
			preview.uvOffset = placement.uvOffset;
			preview.message = "Allocated to " + placement.pageId + ".";
			if (request.uvIslandChartCount > 1) {
				if (request.uvIslandGapLimitedByPageSize) {
					preview.message += " Final UV1 island gap reached " +
						std::to_string(std::max(request.effectiveUvIslandGap, 0)) +
						" px but could not meet the " +
						std::to_string(std::max(settings.minUvIslandGap, 0)) +
						" px target within the page size.";
				} else if (request.expandedForUvIslandGap) {
					preview.message += " Receiver size was expanded to preserve at least " +
						std::to_string(std::max(settings.minUvIslandGap, 0)) +
						" px of UV1 island spacing at bake resolution.";
				}
			}
			result.report.entries.push_back({
				request.entity,
				request.entityName,
				LightmapEntityStatusKind::Allocated,
				LightmapFailureReason::None,
				false,
				preview.message
			});
		}

		result.report.allocatedEntityCount = result.placements.size();
		result.report.totalPages = result.pages.size();

		previewState.report = result.report;
		previewState.pages = result.pages;
		return result;
	}
}
