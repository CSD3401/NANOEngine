#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <Math/Vec2.hpp>

namespace Editor::Assets::ModelAssetInternal {
	enum class UvValidationSeverity : uint8_t {
		Warning,
		Error
	};

	struct UvValidationIssue {
		UvValidationSeverity severity{};
		std::string message;
	};

	struct UvValidationStats {
		NE::Math::Vec2 minUv{ std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };
		NE::Math::Vec2 maxUv{ -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };

		uint32_t nonFiniteVertexCount = 0;
		uint32_t outOfRangeVertexCount = 0;
		uint32_t triangleCount = 0;
		uint32_t degenerateTriangleCount = 0;

		// Overlap heuristic stats (one or the other is used).
		uint32_t overlapGridResolution = 0;
		uint32_t overlapTotalSamplesOrCells = 0;
		uint32_t overlapCollisionCount = 0;
	};

	struct UvValidationConfig {
		// Range sanity
		float rangeEpsilon = 0.01f;      // allow slight spill outside [0,1]
		float extremeRange = 10.0f;      // treat values outside [-extremeRange, extremeRange] as error

		// Degenerate UV triangle check
		float degenerateAreaEpsilon = 1e-10f; // UV area threshold (in UV^2 units, not normalized)
		float degenerateWarnRatio = 0.01f;
		float degenerateErrorRatio = 0.20f;

		// Overlap heuristic
		uint32_t overlapGridResolution = 128;
		float overlapWarnRatio = 0.02f;
		float overlapErrorRatio = 0.10f;

		// If triangle count is small, rasterize triangle coverage (more accurate).
		// Otherwise, use cheap sampling (O(triangles)).
		uint32_t overlapRasterizeMaxTriangles = 20000;
		uint32_t overlapSamplesPerTriangle = 3;
	};

	namespace detail {
		inline bool IsFinite(const NE::Math::Vec2& v) {
			return std::isfinite(v.x) && std::isfinite(v.y);
		}

		inline float Clamp01(float v) {
			return std::min(1.0f, std::max(0.0f, v));
		}

		inline double TriSignedArea2D(const NE::Math::Vec2& a, const NE::Math::Vec2& b, const NE::Math::Vec2& c) {
			return static_cast<double>(b.x - a.x) * static_cast<double>(c.y - a.y) -
				static_cast<double>(b.y - a.y) * static_cast<double>(c.x - a.x);
		}

		inline double TriArea2D(const NE::Math::Vec2& a, const NE::Math::Vec2& b, const NE::Math::Vec2& c) {
			return std::abs(TriSignedArea2D(a, b, c)) * 0.5;
		}

		inline bool PointInTriangle(const NE::Math::Vec2& p, const NE::Math::Vec2& a, const NE::Math::Vec2& b, const NE::Math::Vec2& c) {
			// Barycentric sign test; includes edges.
			const double s1 = TriSignedArea2D(p, a, b);
			const double s2 = TriSignedArea2D(p, b, c);
			const double s3 = TriSignedArea2D(p, c, a);

			const bool hasNeg = (s1 < 0.0) || (s2 < 0.0) || (s3 < 0.0);
			const bool hasPos = (s1 > 0.0) || (s2 > 0.0) || (s3 > 0.0);
			return !(hasNeg && hasPos);
		}

		inline uint32_t QuantizeCell(float uv01, uint32_t res) {
			const float clamped = Clamp01(uv01);
			const uint32_t cell = static_cast<uint32_t>(clamped * static_cast<float>(res));
			return std::min(res - 1, cell);
		}

		inline bool IsInReasonableUv01Range(const NE::Math::Vec2& uv, float epsilon) {
			return uv.x >= -epsilon && uv.x <= 1.0f + epsilon && uv.y >= -epsilon && uv.y <= 1.0f + epsilon;
		}
	}

	inline bool ValidateLightmapUv1(
		const std::string& assetNameForMessages,
		uint32_t submeshIndex,
		const NE::Math::Vec2* uv1,
		uint32_t uv1Count,
		uint32_t expectedVertexCount,
		const uint32_t* indices,
		uint32_t indexCount,
		const UvValidationConfig& config,
		std::vector<UvValidationIssue>& outIssues,
		UvValidationStats* outStats = nullptr
	) {
		outIssues.clear();

		UvValidationStats stats{};
		stats.triangleCount = indexCount / 3;
		stats.overlapGridResolution = config.overlapGridResolution;

		if (!uv1 || uv1Count == 0) {
			outIssues.push_back({ UvValidationSeverity::Error,
				assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
				": Generate Lightmap UVs enabled but UV1 data is missing." });
			if (outStats) *outStats = stats;
			return false;
		}

		if (expectedVertexCount != 0 && uv1Count != expectedVertexCount) {
			outIssues.push_back({ UvValidationSeverity::Error,
				assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
				": UV1 vertex count (" + std::to_string(uv1Count) +
				") does not match vertex buffer count (" + std::to_string(expectedVertexCount) +
				"). UV1 generation likely did not rebuild the vertex/index buffers correctly." });
			if (outStats) *outStats = stats;
			return false;
		}

		if (!indices || (indexCount % 3) != 0) {
			outIssues.push_back({ UvValidationSeverity::Error,
				assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
				": invalid index buffer for UV1 validation (expected triangles)." });
			if (outStats) *outStats = stats;
			return false;
		}

		// Finite + range sanity + min/max
		uint32_t nonFinite = 0;
		uint32_t outOfRange = 0;
		uint32_t extreme = 0;
		for (uint32_t i = 0; i < uv1Count; ++i) {
			const NE::Math::Vec2 uv = uv1[i];
			if (!detail::IsFinite(uv)) {
				++nonFinite;
				continue;
			}

			stats.minUv.x = std::min(stats.minUv.x, uv.x);
			stats.minUv.y = std::min(stats.minUv.y, uv.y);
			stats.maxUv.x = std::max(stats.maxUv.x, uv.x);
			stats.maxUv.y = std::max(stats.maxUv.y, uv.y);

			if (!detail::IsInReasonableUv01Range(uv, config.rangeEpsilon)) {
				++outOfRange;
			}

			if (uv.x < -config.extremeRange || uv.x > config.extremeRange ||
				uv.y < -config.extremeRange || uv.y > config.extremeRange) {
				++extreme;
			}
		}

		stats.nonFiniteVertexCount = nonFinite;
		stats.outOfRangeVertexCount = outOfRange;

		if (nonFinite > 0) {
			outIssues.push_back({ UvValidationSeverity::Error,
				assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
				": UV1 contains non-finite values for " + std::to_string(nonFinite) +
				" / " + std::to_string(uv1Count) + " vertices. Unwrap/pack likely failed." });
		}

		if (extreme > 0) {
			outIssues.push_back({ UvValidationSeverity::Error,
				assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
				": UV1 contains extreme values outside [-" + std::to_string(config.extremeRange) +
				"," + std::to_string(config.extremeRange) + "] for " + std::to_string(extreme) +
				" vertices. Packed lightmap UVs should be normalized." });
		} else if (outOfRange > 0) {
			outIssues.push_back({ UvValidationSeverity::Warning,
				assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
				": UV1 range appears out of [0,1] (epsilon " + std::to_string(config.rangeEpsilon) +
				") for " + std::to_string(outOfRange) + " / " + std::to_string(uv1Count) +
				" vertices. Lightmap UVs are typically expected to be normalized." });
		}

		// Degenerate UV triangles
		uint32_t degenerate = 0;
		for (uint32_t i = 0; i < indexCount; i += 3) {
			const uint32_t i0 = indices[i + 0];
			const uint32_t i1 = indices[i + 1];
			const uint32_t i2 = indices[i + 2];
			if (i0 >= uv1Count || i1 >= uv1Count || i2 >= uv1Count) {
				outIssues.push_back({ UvValidationSeverity::Error,
					assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
					": index buffer references out-of-range vertex while validating UV1." });
				if (outStats) *outStats = stats;
				return false;
			}

			const NE::Math::Vec2 a = uv1[i0];
			const NE::Math::Vec2 b = uv1[i1];
			const NE::Math::Vec2 c = uv1[i2];
			if (!detail::IsFinite(a) || !detail::IsFinite(b) || !detail::IsFinite(c)) {
				continue;
			}

			const double area = detail::TriArea2D(a, b, c);
			if (area <= static_cast<double>(config.degenerateAreaEpsilon)) {
				++degenerate;
			}
		}

		stats.degenerateTriangleCount = degenerate;
		const uint32_t triCount = stats.triangleCount;
		const float degenerateRatio = triCount > 0 ? (static_cast<float>(degenerate) / static_cast<float>(triCount)) : 0.0f;
		if (triCount > 0 && degenerateRatio >= config.degenerateErrorRatio) {
			outIssues.push_back({ UvValidationSeverity::Error,
				assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
				": " + std::to_string(static_cast<int>(degenerateRatio * 100.0f)) +
				"% of triangles have near-zero UV1 area (<= " + std::to_string(config.degenerateAreaEpsilon) +
				"). This will cause lightmap artifacts; check for degenerate/non-manifold geometry." });
		} else if (triCount > 0 && degenerateRatio >= config.degenerateWarnRatio) {
			outIssues.push_back({ UvValidationSeverity::Warning,
				assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
				": " + std::to_string(static_cast<int>(degenerateRatio * 100.0f)) +
				"% of triangles have near-zero UV1 area (<= " + std::to_string(config.degenerateAreaEpsilon) +
				"). This may cause light leaks." });
		}

		// Approximate overlap heuristic
		const uint32_t res = std::max(8u, config.overlapGridResolution);
		if (triCount > 0 && res <= 2048) {
			if (triCount <= config.overlapRasterizeMaxTriangles) {
				std::vector<uint8_t> occupancy(static_cast<size_t>(res) * static_cast<size_t>(res), 0);
				uint32_t coveredCells = 0;
				uint32_t collisions = 0;

				for (uint32_t i = 0; i < indexCount; i += 3) {
					const NE::Math::Vec2 a = uv1[indices[i + 0]];
					const NE::Math::Vec2 b = uv1[indices[i + 1]];
					const NE::Math::Vec2 c = uv1[indices[i + 2]];
					if (!detail::IsFinite(a) || !detail::IsFinite(b) || !detail::IsFinite(c)) continue;

					NE::Math::Vec2 mn{ std::min({ a.x, b.x, c.x }), std::min({ a.y, b.y, c.y }) };
					NE::Math::Vec2 mx{ std::max({ a.x, b.x, c.x }), std::max({ a.y, b.y, c.y }) };

					// If triangle doesn't overlap [0,1]^2 at all, skip (range sanity will already report it).
					if (mx.x < 0.0f || mx.y < 0.0f || mn.x > 1.0f || mn.y > 1.0f) continue;

					const uint32_t x0 = detail::QuantizeCell(mn.x, res);
					const uint32_t y0 = detail::QuantizeCell(mn.y, res);
					const uint32_t x1 = detail::QuantizeCell(mx.x, res);
					const uint32_t y1 = detail::QuantizeCell(mx.y, res);

					for (uint32_t y = y0; y <= y1; ++y) {
						for (uint32_t x = x0; x <= x1; ++x) {
							const NE::Math::Vec2 p{
								(static_cast<float>(x) + 0.5f) / static_cast<float>(res),
								(static_cast<float>(y) + 0.5f) / static_cast<float>(res)
							};
							if (!detail::PointInTriangle(p, a, b, c)) continue;

							const size_t cell = static_cast<size_t>(y) * static_cast<size_t>(res) + static_cast<size_t>(x);
							++coveredCells;
							if (occupancy[cell] != 0) {
								++collisions;
							} else {
								occupancy[cell] = 1;
							}
						}
					}
				}

				stats.overlapTotalSamplesOrCells = coveredCells;
				stats.overlapCollisionCount = collisions;

				const float overlapRatio = coveredCells > 0 ? (static_cast<float>(collisions) / static_cast<float>(coveredCells)) : 0.0f;
				if (overlapRatio >= config.overlapErrorRatio) {
					outIssues.push_back({ UvValidationSeverity::Error,
						assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
						": UV1 overlap heuristic indicates heavy overlap (cell collision ratio " +
						std::to_string(overlapRatio) + " at " + std::to_string(res) + "x" + std::to_string(res) +
						"). Charts may overlap; consider increasing padding or fixing mesh topology." });
				} else if (overlapRatio >= config.overlapWarnRatio) {
					outIssues.push_back({ UvValidationSeverity::Warning,
						assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
						": UV1 overlap heuristic indicates potential overlap (cell collision ratio " +
						std::to_string(overlapRatio) + " at " + std::to_string(res) + "x" + std::to_string(res) +
						")." });
				}
			} else {
				// Large meshes: cheap sampling heuristic
				const uint32_t samplesPerTri = std::max(1u, config.overlapSamplesPerTriangle);
				std::vector<uint32_t> occupancy(static_cast<size_t>(res) * static_cast<size_t>(res), 0);
				uint32_t samples = 0;
				uint32_t collisions = 0;

				auto TrySample = [&](uint32_t triId, const NE::Math::Vec2& p) {
					if (!detail::IsFinite(p)) return;
					if (p.x < 0.0f || p.x > 1.0f || p.y < 0.0f || p.y > 1.0f) return;
					const uint32_t x = detail::QuantizeCell(p.x, res);
					const uint32_t y = detail::QuantizeCell(p.y, res);
					const size_t cell = static_cast<size_t>(y) * static_cast<size_t>(res) + static_cast<size_t>(x);
					++samples;
					const uint32_t owner = occupancy[cell];
					if (owner != 0 && owner != (triId + 1)) {
						++collisions;
					} else {
						occupancy[cell] = triId + 1;
					}
				};

				uint32_t triId = 0;
				for (uint32_t i = 0; i < indexCount; i += 3, ++triId) {
					const NE::Math::Vec2 a = uv1[indices[i + 0]];
					const NE::Math::Vec2 b = uv1[indices[i + 1]];
					const NE::Math::Vec2 c = uv1[indices[i + 2]];
					if (!detail::IsFinite(a) || !detail::IsFinite(b) || !detail::IsFinite(c)) continue;

					// Always include centroid; optionally include a couple edge midpoints.
					const NE::Math::Vec2 centroid{ (a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f };
					TrySample(triId, centroid);

					if (samplesPerTri >= 2) {
						TrySample(triId, NE::Math::Vec2{ (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f });
					}
					if (samplesPerTri >= 3) {
						TrySample(triId, NE::Math::Vec2{ (b.x + c.x) * 0.5f, (b.y + c.y) * 0.5f });
					}
					if (samplesPerTri >= 4) {
						TrySample(triId, NE::Math::Vec2{ (c.x + a.x) * 0.5f, (c.y + a.y) * 0.5f });
					}
				}

				stats.overlapTotalSamplesOrCells = samples;
				stats.overlapCollisionCount = collisions;

				const float overlapRatio = samples > 0 ? (static_cast<float>(collisions) / static_cast<float>(samples)) : 0.0f;
				if (overlapRatio >= config.overlapErrorRatio) {
					outIssues.push_back({ UvValidationSeverity::Error,
						assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
						": UV1 overlap sampling heuristic indicates heavy overlap (sample collision ratio " +
						std::to_string(overlapRatio) + " at " + std::to_string(res) + "x" + std::to_string(res) +
						"). Charts may overlap; consider increasing padding or fixing mesh topology." });
				} else if (overlapRatio >= config.overlapWarnRatio) {
					outIssues.push_back({ UvValidationSeverity::Warning,
						assetNameForMessages + " submesh " + std::to_string(submeshIndex) +
						": UV1 overlap sampling heuristic indicates potential overlap (sample collision ratio " +
						std::to_string(overlapRatio) + " at " + std::to_string(res) + "x" + std::to_string(res) +
						")." });
				}
			}
		}

		if (outStats) *outStats = stats;

		const bool hasError = std::any_of(outIssues.begin(), outIssues.end(), [](const UvValidationIssue& i) {
			return i.severity == UvValidationSeverity::Error;
			});
		return !hasError;
	}
}
