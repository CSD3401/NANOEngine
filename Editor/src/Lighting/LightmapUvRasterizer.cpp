#include "pch.h"
#include "LightmapUvRasterizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Transform.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <EditorInterface/RendererExports.hpp>
#include <Engine.hpp>
#include <Graphics/Core/Model.hpp>
#include <Graphics/Core/Vertex.hpp>
#include <Math/Mat4.hpp>
#include <Math/Vec4.hpp>

namespace Editor::Lightmapping {
	namespace {
		constexpr float kFiniteEpsilon = 1e-6f;
		constexpr float kBarycentricRangeTolerance = 1e-3f;
		constexpr float kBarycentricSumTolerance = 1e-3f;
		constexpr size_t kMaxWarningExamples = 64;

		struct PageInfo {
			int pageIndex = -1;
			uint32_t width = 0;
			uint32_t height = 0;
			std::string pageId;
		};

		struct RasterLocalStats {
			size_t degenerateUvTriangleCount = 0;
			size_t coveredTexelCount = 0;
			size_t ownershipConflictCount = 0;
			size_t sameReceiverOwnershipConflictCount = 0;
			size_t crossReceiverOwnershipConflictCount = 0;
			size_t invalidBarycentricTexelCount = 0;
			size_t outOfRangeTexelCount = 0;
			size_t invalidSampleTexelCount = 0;
			size_t innerRectClampedTriangleCount = 0;
		};

		bool IsFiniteFloat(float value) {
			return std::isfinite(value);
		}

		bool IsFiniteVec2(const NE::Math::Vec2& value) {
			return IsFiniteFloat(value.x) && IsFiniteFloat(value.y);
		}

		bool IsFiniteVec3(const NE::Math::Vec3& value) {
			return IsFiniteFloat(value.x) && IsFiniteFloat(value.y) && IsFiniteFloat(value.z);
		}

		bool IsFiniteMatrix(const NE::Math::Mat4& matrix) {
			for (float value : matrix.a) {
				if (!std::isfinite(value)) {
					return false;
				}
			}
			return true;
		}

		NE::Math::Vec3 TransformPoint(const NE::Math::Mat4& matrix, const NE::Math::Vec3& point) {
			const NE::Math::Vec4 transformed = matrix * NE::Math::Vec4(point.x, point.y, point.z, 1.0f);
			return { transformed.x, transformed.y, transformed.z };
		}

		NE::Math::Vec3 TransformDirection(const NE::Math::Mat4& matrix, const NE::Math::Vec3& direction) {
			const NE::Math::Vec4 transformed = matrix * NE::Math::Vec4(direction.x, direction.y, direction.z, 0.0f);
			return { transformed.x, transformed.y, transformed.z };
		}

		NE::Math::Vec3 SafeNormalize(const NE::Math::Vec3& value, const NE::Math::Vec3& fallback) {
			if (!IsFiniteVec3(value) || value.LengthSquared() <= kFiniteEpsilon) {
				return fallback;
			}
			return value.Normalized();
		}

		std::string GetEntityNameOrFallback(uint32_t entity) {
			if (!NE::ECS::Query::HasEntityMeta(entity)) {
				return "Entity " + std::to_string(entity);
			}

			const auto& meta = NE::ECS::Query::GetEntityMeta(entity);
			return meta.name.empty() ? ("Entity " + std::to_string(entity)) : meta.name;
		}

		std::shared_ptr<NE::Graphics::Model> ResolveModel(uint32_t entity, NE::ECS::Component::Renderer& renderer) {
			if (renderer.model && !renderer.isDirty) {
				return renderer.model;
			}

			if (renderer.modelUUID.empty()) {
				renderer.model.reset();
				return nullptr;
			}

			NE::Renderer::Command::AssignModel(entity, renderer.modelUUID, renderer.subMeshIndex);
			renderer.isDirty = false;
			return renderer.model;
		}

		NE::Math::Vec3 ResolveShadingNormal(
			const NE::Math::Vec3& sourceNormal,
			const NE::Math::Mat4& normalMatrix,
			bool hasValidNormalMatrix,
			const NE::Math::Vec3& geometricNormal) {
			if (!IsFiniteVec3(sourceNormal) || sourceNormal.LengthSquared() <= kFiniteEpsilon) {
				return geometricNormal;
			}

			NE::Math::Vec3 transformed = hasValidNormalMatrix ? TransformDirection(normalMatrix, sourceNormal) : sourceNormal;
			if (!IsFiniteVec3(transformed) || transformed.LengthSquared() <= kFiniteEpsilon) {
				return geometricNormal;
			}

			return transformed.Normalized();
		}

		void PushWarning(std::vector<std::string>& warnings, const std::string& message) {
			if (warnings.size() < kMaxWarningExamples) {
				warnings.push_back(message);
			}
		}

		bool AreBarycentricsSane(const NE::Math::Vec3& barycentrics) {
			if (!IsFiniteVec3(barycentrics)) {
				return false;
			}

			const float sum = barycentrics.x + barycentrics.y + barycentrics.z;
			if (!std::isfinite(sum) || std::fabs(sum - 1.0f) > kBarycentricSumTolerance) {
				return false;
			}

			return barycentrics.x >= -kBarycentricRangeTolerance &&
				barycentrics.y >= -kBarycentricRangeTolerance &&
				barycentrics.z >= -kBarycentricRangeTolerance &&
				barycentrics.x <= 1.0f + kBarycentricRangeTolerance &&
				barycentrics.y <= 1.0f + kBarycentricRangeTolerance &&
				barycentrics.z <= 1.0f + kBarycentricRangeTolerance;
		}

		bool AreBarycentricsNearlyEqual(const NE::Math::Vec3& lhs, const NE::Math::Vec3& rhs) {
			return std::fabs(lhs.x - rhs.x) <= kFiniteEpsilon &&
				std::fabs(lhs.y - rhs.y) <= kFiniteEpsilon &&
				std::fabs(lhs.z - rhs.z) <= kFiniteEpsilon;
		}

		std::string CategorizeRasterWarning(const std::string& warning) {
			if (warning.find("invalid page index") != std::string::npos) {
				return "Invalid atlas page index";
			}
			if (warning.find("allocated inner rect is empty") != std::string::npos) {
				return "Empty inner rect";
			}
			if (warning.find("allocated inner rect lies outside its page") != std::string::npos) {
				return "Inner rect outside page";
			}
			if (warning.find("required components are missing") != std::string::npos) {
				return "Missing required receiver components";
			}
			if (warning.find("no longer marked static") != std::string::npos) {
				return "Receiver no longer static";
			}
			if (warning.find("is inactive") != std::string::npos) {
				return "Inactive receiver";
			}
			if (warning.find("cooked model is unavailable") != std::string::npos) {
				return "Cooked model unavailable";
			}
			if (warning.find("does not resolve to one valid baked submesh") != std::string::npos) {
				return "Invalid baked submesh";
			}
			if (warning.find("world transform is non-finite") != std::string::npos) {
				return "Non-finite world transform";
			}
			if (warning.find("UV1 triangle data is unavailable") != std::string::npos) {
				return "Missing UV1 triangle data";
			}
			if (warning.find("no valid world-space UV1 triangles remained after validation") != std::string::npos) {
				return "No valid UV1 triangles after validation";
			}
			if (warning.find("page buffer could not be resolved") != std::string::npos) {
				return "Missing raster page buffer";
			}
			return "Other raster warning";
		}

		void TallyWarningCounts(LightmapUvRasterResult& result) {
			result.warningCounts.clear();
			for (const auto& warning : result.warnings) {
				result.warningCounts[CategorizeRasterWarning(warning)]++;
			}
		}

		float EdgeFunction(const NE::Math::Vec2& a, const NE::Math::Vec2& b, const NE::Math::Vec2& p) {
			return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
		}

		bool IsTopLeftEdge(const NE::Math::Vec2& a, const NE::Math::Vec2& b, float edgeEpsilon) {
			const NE::Math::Vec2 edge = b - a;
			return (edge.y > edgeEpsilon) || (std::fabs(edge.y) <= edgeEpsilon && edge.x < 0.0f);
		}

		bool PassesFillRule(float edgeValue, bool topLeft, bool positiveArea, float edgeEpsilon) {
			if (positiveArea) {
				return edgeValue > edgeEpsilon || (std::fabs(edgeValue) <= edgeEpsilon && topLeft);
			}
			return edgeValue < -edgeEpsilon || (std::fabs(edgeValue) <= edgeEpsilon && topLeft);
		}

		PageInfo* FindPageInfo(std::unordered_map<int, PageInfo>& pageInfoByIndex, int pageIndex) {
			const auto it = pageInfoByIndex.find(pageIndex);
			return it == pageInfoByIndex.end() ? nullptr : &it->second;
		}

		uint32_t StableOwnerColor(uint64_t stableId, uint32_t sourceTriangleIndex) {
			uint64_t hash = 1469598103934665603ull;
			const uint64_t mix[] = { stableId, static_cast<uint64_t>(sourceTriangleIndex) };
			for (uint64_t value : mix) {
				for (int i = 0; i < 8; ++i) {
					hash ^= ((value >> (i * 8)) & 0xffull);
					hash *= 1099511628211ull;
				}
			}

			const uint8_t r = static_cast<uint8_t>((hash >> 0) & 0xffu);
			const uint8_t g = static_cast<uint8_t>((hash >> 8) & 0xffu);
			const uint8_t b = static_cast<uint8_t>((hash >> 16) & 0xffu);
			return (static_cast<uint32_t>(255u) << 24) |
				(static_cast<uint32_t>(std::max<uint8_t>(r, 48u)) << 0) |
				(static_cast<uint32_t>(std::max<uint8_t>(g, 48u)) << 8) |
				(static_cast<uint32_t>(std::max<uint8_t>(b, 48u)) << 16);
		}

		void BuildPagePreviews(LightmapUvRasterResult& result) {
			for (auto& page : result.pageBuffers) {
				page.preview.pageIndex = page.pageIndex;
				page.preview.pageId = page.pageId;
				page.preview.width = page.width;
				page.preview.height = page.height;
				page.preview.validTexelCount = page.validTexelCount;
				page.preview.allocatedInnerTexelCount = page.allocatedInnerTexelCount;
				page.preview.coverage01 = page.coverage01;
				page.preview.validityRgba8.resize(static_cast<size_t>(page.width) * static_cast<size_t>(page.height) * 4u, 0u);
				page.preview.ownerRgba8.resize(static_cast<size_t>(page.width) * static_cast<size_t>(page.height) * 4u, 0u);

				for (size_t i = 0; i < page.validMask.size(); ++i) {
					const size_t rgbaIndex = i * 4u;
					const uint8_t maskValue = page.validMask[i] != 0u ? 255u : 0u;
					page.preview.validityRgba8[rgbaIndex + 0u] = maskValue;
					page.preview.validityRgba8[rgbaIndex + 1u] = maskValue;
					page.preview.validityRgba8[rgbaIndex + 2u] = maskValue;
					page.preview.validityRgba8[rgbaIndex + 3u] = 255u;

					if (page.validMask[i] == 0u) {
						page.preview.ownerRgba8[rgbaIndex + 3u] = 255u;
						continue;
					}

					const uint32_t receiverIndex = page.ownerReceiverIndex[i];
					if (receiverIndex >= result.receivers.size()) {
						page.preview.ownerRgba8[rgbaIndex + 3u] = 255u;
						continue;
					}

					const auto& receiver = result.receivers[receiverIndex];
					const uint32_t packedColor = StableOwnerColor(receiver.stableId, page.ownerSourceTriangleIndex[i]);
					page.preview.ownerRgba8[rgbaIndex + 0u] = static_cast<uint8_t>((packedColor >> 0) & 0xffu);
					page.preview.ownerRgba8[rgbaIndex + 1u] = static_cast<uint8_t>((packedColor >> 8) & 0xffu);
					page.preview.ownerRgba8[rgbaIndex + 2u] = static_cast<uint8_t>((packedColor >> 16) & 0xffu);
					page.preview.ownerRgba8[rgbaIndex + 3u] = static_cast<uint8_t>((packedColor >> 24) & 0xffu);
				}
			}
		}

		bool ValidateRectWithinPage(
			const LightmapPlacement& placement,
			const PageInfo& pageInfo,
			std::string& outMessage) {
			if (placement.innerWidth <= 0 || placement.innerHeight <= 0) {
				outMessage = placement.entityName + ": skipped bake receiver because the allocated inner rect is empty.";
				return false;
			}

			if (placement.innerX < 0 ||
				placement.innerY < 0 ||
				placement.innerX + placement.innerWidth > static_cast<int>(pageInfo.width) ||
				placement.innerY + placement.innerHeight > static_cast<int>(pageInfo.height)) {
				outMessage = placement.entityName + ": skipped bake receiver because the allocated inner rect lies outside its page.";
				return false;
			}

			return true;
		}

		bool ReconstructSampleInternal(
			const LightmapUvRasterResult& result,
			const LightmapUvRasterPageBuffers& page,
			size_t linearIndex,
			LightmapRasterSample& outSample) {
			if (linearIndex >= page.validMask.size() || page.validMask[linearIndex] == 0u) {
				return false;
			}

			const uint32_t receiverIndex = page.ownerReceiverIndex[linearIndex];
			const uint32_t triangleIndex = page.ownerTriangleIndex[linearIndex];
			if (receiverIndex >= result.receivers.size()) {
				return false;
			}

			const auto& receiver = result.receivers[receiverIndex];
			if (triangleIndex >= receiver.triangles.size()) {
				return false;
			}

			const auto& triangle = receiver.triangles[triangleIndex];
			const NE::Math::Vec3 barycentrics = page.barycentrics[linearIndex];
			if (!AreBarycentricsSane(barycentrics)) {
				return false;
			}

			const NE::Math::Vec3 worldPosition =
				triangle.p0 * barycentrics.x +
				triangle.p1 * barycentrics.y +
				triangle.p2 * barycentrics.z;
			const NE::Math::Vec3 shadingNormal = SafeNormalize(
				triangle.shadingNormal0 * barycentrics.x +
				triangle.shadingNormal1 * barycentrics.y +
				triangle.shadingNormal2 * barycentrics.z,
				triangle.geometricNormal);
			const NE::Math::Vec3 geometricNormal = SafeNormalize(triangle.geometricNormal, { 0.0f, 1.0f, 0.0f });
			if (!IsFiniteVec3(worldPosition) || !IsFiniteVec3(shadingNormal) || !IsFiniteVec3(geometricNormal)) {
				return false;
			}

			outSample = {};
			outSample.entity = receiver.entity;
			outSample.stableId = receiver.stableId;
			outSample.rendererLuid = receiver.rendererLuid;
			outSample.entityName = receiver.entityName;
			outSample.modelUUID = receiver.modelUUID;
			outSample.materialUUID = receiver.materialUUID;
			outSample.subMeshIndex = receiver.subMeshIndex;
			outSample.sourceTriangleIndex = triangle.sourceTriangleIndex;
			outSample.receiverIndex = receiverIndex;
			outSample.triangleIndex = triangleIndex;
			outSample.pageIndex = page.pageIndex;
			outSample.texelX = static_cast<uint32_t>(linearIndex % static_cast<size_t>(page.width));
			outSample.texelY = static_cast<uint32_t>(linearIndex / static_cast<size_t>(page.width));
			outSample.barycentrics = barycentrics;
			outSample.worldPosition = worldPosition;
			outSample.shadingNormal = shadingNormal;
			outSample.geometricNormal = geometricNormal;
			return true;
		}
	}

	std::vector<LightmapBakeReceiverSnapshot> CollectLightmapBakeReceiverSnapshots(
		const std::vector<LightmapPlacement>& placements,
		const std::vector<LightmapAtlasPage>& pages,
		std::vector<std::string>& warnings,
		LightmapBakeReceiverCollectionStats* outStats) {
		std::vector<LightmapBakeReceiverSnapshot> receivers;
		receivers.reserve(placements.size());
		LightmapBakeReceiverCollectionStats collectionStats{};

		std::unordered_map<int, PageInfo> pageInfoByIndex;
		pageInfoByIndex.reserve(pages.size());
		for (const auto& page : pages) {
			pageInfoByIndex.emplace(page.pageIndex, PageInfo{
				page.pageIndex,
				static_cast<uint32_t>(std::max(page.width, 0)),
				static_cast<uint32_t>(std::max(page.height, 0)),
				page.pageId
			});
		}

		for (const auto& placement : placements) {
			const uint32_t entity = placement.entity;
			const std::string entityName = placement.entityName.empty() ? GetEntityNameOrFallback(entity) : placement.entityName;

			PageInfo* pageInfo = FindPageInfo(pageInfoByIndex, placement.pageIndex);
			if (!pageInfo) {
				PushWarning(warnings, entityName + ": skipped bake receiver because the atlas snapshot references an invalid page index.");
				continue;
			}

			std::string rectFailure;
			if (!ValidateRectWithinPage(placement, *pageInfo, rectFailure)) {
				PushWarning(warnings, rectFailure);
				continue;
			}

			if (!NE::ECS::Query::HasEntityMeta(entity) ||
				!NE::ECS::Query::HasRenderer(entity) ||
				!NE::ECS::Query::HasTransform(entity)) {
				PushWarning(warnings, entityName + ": skipped bake receiver because required components are missing.");
				continue;
			}

			const auto& meta = NE::ECS::Query::GetEntityMeta(entity);
			if (!meta.isStatic) {
				PushWarning(warnings, entityName + ": skipped bake receiver because it is no longer marked static.");
				continue;
			}

			if (!NE::ECS::Query::GetActive(entity)) {
				PushWarning(warnings, entityName + ": skipped bake receiver because it is inactive.");
				continue;
			}

			auto& renderer = NE::ECS::Command::GetEntityRenderer(entity);
			const auto model = ResolveModel(entity, renderer);
			if (!model || model->meshes.empty()) {
				PushWarning(warnings, entityName + ": skipped bake receiver because the cooked model is unavailable.");
				continue;
			}

			if (renderer.subMeshIndex < 0 || renderer.subMeshIndex >= static_cast<int32_t>(model->meshes.size())) {
				PushWarning(warnings, entityName + ": skipped bake receiver because the renderer does not resolve to one valid baked submesh.");
				continue;
			}

			const auto& transform = NE::ECS::Query::GetEntityTransform(entity);
			if (!IsFiniteMatrix(transform.worldMatrix)) {
				PushWarning(warnings, entityName + ": skipped bake receiver because the world transform is non-finite.");
				continue;
			}

			const auto& submesh = model->meshes[static_cast<size_t>(renderer.subMeshIndex)];
			if (!submesh.hasUv1 || submesh.vertices.empty() || submesh.indices.size() < 3) {
				PushWarning(warnings, entityName + ": skipped bake receiver because UV1 triangle data is unavailable.");
				continue;
			}

			LightmapBakeReceiverSnapshot receiver{};
			receiver.entity = entity;
			receiver.stableId = meta.luid != 0 ? meta.luid : static_cast<uint64_t>(entity);
			receiver.rendererLuid = renderer.luid;
			receiver.entityName = entityName;
			receiver.modelUUID = renderer.modelUUID;
			receiver.materialUUID = renderer.materialUUID;
			receiver.subMeshIndex = static_cast<uint32_t>(renderer.subMeshIndex);
			receiver.pageIndex = placement.pageIndex;
			receiver.pageWidth = pageInfo->width;
			receiver.pageHeight = pageInfo->height;
			receiver.placement = placement;

			const float determinant = transform.worldMatrix.Determinant();
			const bool hasValidNormalMatrix = std::isfinite(determinant) && std::fabs(determinant) > kFiniteEpsilon;
			const NE::Math::Mat4 normalMatrix = hasValidNormalMatrix
				? transform.worldMatrix.Inverse().Transpose()
				: transform.worldMatrix;

			receiver.triangles.reserve(submesh.indices.size() / 3u);
			bool receiverDiscardedTriangles = false;
			for (size_t index = 0; index + 2 < submesh.indices.size(); index += 3) {
				const uint32_t ia = submesh.indices[index + 0];
				const uint32_t ib = submesh.indices[index + 1];
				const uint32_t ic = submesh.indices[index + 2];
				if (ia >= submesh.vertices.size() || ib >= submesh.vertices.size() || ic >= submesh.vertices.size()) {
					receiverDiscardedTriangles = true;
					++collectionStats.discardedTriangleCount;
					++collectionStats.outOfRangeIndexTriangleCount;
					continue;
				}

				const auto& va = submesh.vertices[ia];
				const auto& vb = submesh.vertices[ib];
				const auto& vc = submesh.vertices[ic];
				if (!IsFiniteVec2(va.texCoord1) || !IsFiniteVec2(vb.texCoord1) || !IsFiniteVec2(vc.texCoord1)) {
					receiverDiscardedTriangles = true;
					++collectionStats.discardedTriangleCount;
					++collectionStats.nonFiniteUvTriangleCount;
					continue;
				}

				const NE::Math::Vec3 p0 = TransformPoint(transform.worldMatrix, va.position);
				const NE::Math::Vec3 p1 = TransformPoint(transform.worldMatrix, vb.position);
				const NE::Math::Vec3 p2 = TransformPoint(transform.worldMatrix, vc.position);
				if (!IsFiniteVec3(p0) || !IsFiniteVec3(p1) || !IsFiniteVec3(p2)) {
					receiverDiscardedTriangles = true;
					++collectionStats.discardedTriangleCount;
					++collectionStats.nonFiniteWorldPositionTriangleCount;
					continue;
				}

				const NE::Math::Vec3 geometricCross = (p1 - p0).Cross(p2 - p0);
				const float doubleArea = geometricCross.Length();
				if (!std::isfinite(doubleArea) || doubleArea <= kFiniteEpsilon) {
					receiverDiscardedTriangles = true;
					++collectionStats.discardedTriangleCount;
					++collectionStats.degenerateWorldTriangleCount;
					continue;
				}

				LightmapBakeReceiverTriangle triangle{};
				triangle.p0 = p0;
				triangle.p1 = p1;
				triangle.p2 = p2;
				triangle.geometricNormal = geometricCross / doubleArea;
				triangle.shadingNormal0 = ResolveShadingNormal(va.normal, normalMatrix, hasValidNormalMatrix, triangle.geometricNormal);
				triangle.shadingNormal1 = ResolveShadingNormal(vb.normal, normalMatrix, hasValidNormalMatrix, triangle.geometricNormal);
				triangle.shadingNormal2 = ResolveShadingNormal(vc.normal, normalMatrix, hasValidNormalMatrix, triangle.geometricNormal);
				triangle.uv0 = va.texCoord1;
				triangle.uv1 = vb.texCoord1;
				triangle.uv2 = vc.texCoord1;
				triangle.sourceTriangleIndex = static_cast<uint32_t>(index / 3u);
				receiver.triangles.push_back(triangle);
			}

			if (receiverDiscardedTriangles) {
				++collectionStats.receiversWithDiscardedTriangles;
			}

			if (receiver.triangles.empty()) {
				PushWarning(warnings, entityName + ": skipped bake receiver because no valid world-space UV1 triangles remained after validation.");
				continue;
			}

			receivers.push_back(std::move(receiver));
		}

		std::sort(receivers.begin(), receivers.end(),
			[](const LightmapBakeReceiverSnapshot& lhs, const LightmapBakeReceiverSnapshot& rhs) {
				if (lhs.pageIndex != rhs.pageIndex) {
					return lhs.pageIndex < rhs.pageIndex;
				}
				return lhs.stableId < rhs.stableId;
			});
		if (outStats) {
			*outStats = collectionStats;
		}
		return receivers;
	}

	LightmapUvRasterResult RasterizeLightmapUv1Atlas(
		const std::vector<LightmapAtlasPage>& pages,
		const std::vector<LightmapBakeReceiverSnapshot>& receivers,
		const LightmapUvRasterizationSettings& settings,
		std::atomic<bool>* cancelRequested) {
		LightmapUvRasterResult result{};
		result.settings = settings;
		result.pages = pages;
		result.receivers = receivers;
		result.stats.receiverInstanceCount = receivers.size();
		result.stats.pageCount = pages.size();

		result.pageBuffers.reserve(pages.size());
		std::unordered_map<int, size_t> pageSlotsByIndex;
		pageSlotsByIndex.reserve(pages.size());
		for (const auto& page : pages) {
			LightmapUvRasterPageBuffers buffers{};
			buffers.pageIndex = page.pageIndex;
			buffers.pageId = page.pageId;
			buffers.width = static_cast<uint32_t>(std::max(page.width, 0));
			buffers.height = static_cast<uint32_t>(std::max(page.height, 0));
			const size_t texelCount = static_cast<size_t>(buffers.width) * static_cast<size_t>(buffers.height);
			buffers.validMask.assign(texelCount, 0u);
			buffers.ownerReceiverIndex.assign(texelCount, kInvalidRasterReceiverIndex);
			buffers.ownerTriangleIndex.assign(texelCount, kInvalidRasterTriangleIndex);
			buffers.ownerSourceTriangleIndex.assign(texelCount, kInvalidRasterTriangleIndex);
			buffers.barycentrics.assign(texelCount, { 0.0f, 0.0f, 0.0f });
			pageSlotsByIndex.emplace(page.pageIndex, result.pageBuffers.size());
			result.pageBuffers.push_back(std::move(buffers));
		}

		result.receiverCoverage.resize(receivers.size());
		for (size_t receiverIndex = 0; receiverIndex < receivers.size(); ++receiverIndex) {
			const auto& receiver = receivers[receiverIndex];
			auto& coverage = result.receiverCoverage[receiverIndex];
			coverage.entity = receiver.entity;
			coverage.stableId = receiver.stableId;
			coverage.pageIndex = receiver.pageIndex;
			coverage.allocatedInnerTexelCount =
				static_cast<size_t>(std::max(receiver.placement.innerWidth, 0)) *
				static_cast<size_t>(std::max(receiver.placement.innerHeight, 0));

			const auto pageSlotIt = pageSlotsByIndex.find(receiver.pageIndex);
			if (pageSlotIt != pageSlotsByIndex.end()) {
				result.pageBuffers[pageSlotIt->second].allocatedInnerTexelCount += coverage.allocatedInnerTexelCount;
			}

			result.stats.triangleCount += receiver.triangles.size();
		}

		for (size_t receiverIndex = 0; receiverIndex < receivers.size(); ++receiverIndex) {
			if (cancelRequested && cancelRequested->load()) {
				break;
			}

			const auto& receiver = receivers[receiverIndex];
			const auto pageSlotIt = pageSlotsByIndex.find(receiver.pageIndex);
			if (pageSlotIt == pageSlotsByIndex.end()) {
				PushWarning(result.warnings, receiver.entityName + ": skipped rasterization because its page buffer could not be resolved.");
				continue;
			}

			auto& page = result.pageBuffers[pageSlotIt->second];
			const int minX = receiver.placement.innerX;
			const int minY = receiver.placement.innerY;
			const int maxX = receiver.placement.innerX + receiver.placement.innerWidth - 1;
			const int maxY = receiver.placement.innerY + receiver.placement.innerHeight - 1;

			RasterLocalStats localStats{};
			for (size_t triangleIndex = 0; triangleIndex < receiver.triangles.size(); ++triangleIndex) {
				if (cancelRequested && cancelRequested->load()) {
					break;
				}

				const auto& triangle = receiver.triangles[triangleIndex];
				const NE::Math::Vec2 atlasUv0 = triangle.uv0 * receiver.placement.uvScale + receiver.placement.uvOffset;
				const NE::Math::Vec2 atlasUv1 = triangle.uv1 * receiver.placement.uvScale + receiver.placement.uvOffset;
				const NE::Math::Vec2 atlasUv2 = triangle.uv2 * receiver.placement.uvScale + receiver.placement.uvOffset;
				if (!IsFiniteVec2(atlasUv0) || !IsFiniteVec2(atlasUv1) || !IsFiniteVec2(atlasUv2)) {
					++localStats.degenerateUvTriangleCount;
					continue;
				}

				const NE::Math::Vec2 page0{
					atlasUv0.x * static_cast<float>(page.width),
					atlasUv0.y * static_cast<float>(page.height)
				};
				const NE::Math::Vec2 page1{
					atlasUv1.x * static_cast<float>(page.width),
					atlasUv1.y * static_cast<float>(page.height)
				};
				const NE::Math::Vec2 page2{
					atlasUv2.x * static_cast<float>(page.width),
					atlasUv2.y * static_cast<float>(page.height)
				};
				if (!IsFiniteVec2(page0) || !IsFiniteVec2(page1) || !IsFiniteVec2(page2)) {
					++localStats.degenerateUvTriangleCount;
					continue;
				}

				const float signedArea = EdgeFunction(page0, page1, page2);
				if (!std::isfinite(signedArea) || std::fabs(signedArea) <= settings.uvAreaEpsilon) {
					++localStats.degenerateUvTriangleCount;
					continue;
				}

				const bool positiveArea = signedArea > 0.0f;
				const bool topLeft0 = IsTopLeftEdge(page1, page2, settings.edgeEpsilon);
				const bool topLeft1 = IsTopLeftEdge(page2, page0, settings.edgeEpsilon);
				const bool topLeft2 = IsTopLeftEdge(page0, page1, settings.edgeEpsilon);

				const float boundsMinX = std::min(page0.x, std::min(page1.x, page2.x));
				const float boundsMinY = std::min(page0.y, std::min(page1.y, page2.y));
				const float boundsMaxX = std::max(page0.x, std::max(page1.x, page2.x));
				const float boundsMaxY = std::max(page0.y, std::max(page1.y, page2.y));

				const int unclampedStartX = static_cast<int>(std::ceil(boundsMinX - 0.5f));
				const int unclampedStartY = static_cast<int>(std::ceil(boundsMinY - 0.5f));
				const int unclampedEndX = static_cast<int>(std::floor(boundsMaxX - 0.5f));
				const int unclampedEndY = static_cast<int>(std::floor(boundsMaxY - 0.5f));

				const int startX = std::max(minX, unclampedStartX);
				const int startY = std::max(minY, unclampedStartY);
				const int endX = std::min(maxX, unclampedEndX);
				const int endY = std::min(maxY, unclampedEndY);
				if (startX != unclampedStartX || startY != unclampedStartY || endX != unclampedEndX || endY != unclampedEndY) {
					++localStats.innerRectClampedTriangleCount;
				}
				if (startX > endX || startY > endY) {
					continue;
				}

				for (int y = startY; y <= endY; ++y) {
					for (int x = startX; x <= endX; ++x) {
						const NE::Math::Vec2 texelCenter{
							static_cast<float>(x) + 0.5f,
							static_cast<float>(y) + 0.5f
						};

						const float edge0 = EdgeFunction(page1, page2, texelCenter);
						const float edge1 = EdgeFunction(page2, page0, texelCenter);
						const float edge2 = EdgeFunction(page0, page1, texelCenter);
						if (!PassesFillRule(edge0, topLeft0, positiveArea, settings.edgeEpsilon) ||
							!PassesFillRule(edge1, topLeft1, positiveArea, settings.edgeEpsilon) ||
							!PassesFillRule(edge2, topLeft2, positiveArea, settings.edgeEpsilon)) {
							continue;
						}

						const NE::Math::Vec3 barycentrics{
							edge0 / signedArea,
							edge1 / signedArea,
							edge2 / signedArea
						};
						if (!AreBarycentricsSane(barycentrics)) {
							++localStats.invalidBarycentricTexelCount;
							continue;
						}

						const size_t linearIndex = static_cast<size_t>(y) * static_cast<size_t>(page.width) + static_cast<size_t>(x);
						if (linearIndex >= page.validMask.size()) {
							++localStats.outOfRangeTexelCount;
							continue;
						}

						if (page.validMask[linearIndex] != 0u) {
							++localStats.ownershipConflictCount;
							const uint32_t priorReceiverIndex = page.ownerReceiverIndex[linearIndex];
							if (priorReceiverIndex == static_cast<uint32_t>(receiverIndex)) {
								++localStats.sameReceiverOwnershipConflictCount;
							} else {
								++localStats.crossReceiverOwnershipConflictCount;
							}
							continue;
						}

						const NE::Math::Vec3 worldPosition =
							triangle.p0 * barycentrics.x +
							triangle.p1 * barycentrics.y +
							triangle.p2 * barycentrics.z;
						const NE::Math::Vec3 shadingNormal = SafeNormalize(
							triangle.shadingNormal0 * barycentrics.x +
							triangle.shadingNormal1 * barycentrics.y +
							triangle.shadingNormal2 * barycentrics.z,
							triangle.geometricNormal);
						const NE::Math::Vec3 geometricNormal = SafeNormalize(triangle.geometricNormal, { 0.0f, 1.0f, 0.0f });
						if (!IsFiniteVec3(worldPosition) || !IsFiniteVec3(shadingNormal) || !IsFiniteVec3(geometricNormal)) {
							++localStats.invalidSampleTexelCount;
							continue;
						}

						page.validMask[linearIndex] = 1u;
						page.ownerReceiverIndex[linearIndex] = static_cast<uint32_t>(receiverIndex);
						page.ownerTriangleIndex[linearIndex] = static_cast<uint32_t>(triangleIndex);
						page.ownerSourceTriangleIndex[linearIndex] = triangle.sourceTriangleIndex;
						page.barycentrics[linearIndex] = barycentrics;
						++localStats.coveredTexelCount;
						++result.receiverCoverage[receiverIndex].validTexelCount;
					}
				}
			}

			result.stats.degenerateUvTriangleCount += localStats.degenerateUvTriangleCount;
			result.stats.coveredTexelCount += localStats.coveredTexelCount;
			result.stats.ownershipConflictCount += localStats.ownershipConflictCount;
			result.stats.sameReceiverOwnershipConflictCount += localStats.sameReceiverOwnershipConflictCount;
			result.stats.crossReceiverOwnershipConflictCount += localStats.crossReceiverOwnershipConflictCount;
			result.stats.invalidBarycentricTexelCount += localStats.invalidBarycentricTexelCount;
			result.stats.outOfRangeTexelCount += localStats.outOfRangeTexelCount;
			result.stats.invalidSampleTexelCount += localStats.invalidSampleTexelCount;
			result.stats.innerRectClampedTriangleCount += localStats.innerRectClampedTriangleCount;
		}

		for (size_t receiverIndex = 0; receiverIndex < result.receiverCoverage.size(); ++receiverIndex) {
			auto& coverage = result.receiverCoverage[receiverIndex];
			coverage.coverage01 = coverage.allocatedInnerTexelCount > 0
				? std::clamp(static_cast<float>(coverage.validTexelCount) / static_cast<float>(coverage.allocatedInnerTexelCount), 0.0f, 1.0f)
				: 0.0f;
			result.stats.uncoveredTexelCount += coverage.allocatedInnerTexelCount - coverage.validTexelCount;
		}

		for (auto& page : result.pageBuffers) {
			page.validTexelCount = static_cast<size_t>(std::count_if(page.validMask.begin(), page.validMask.end(),
				[](uint8_t value) { return value != 0u; }));
			page.coverage01 = page.allocatedInnerTexelCount > 0
				? std::clamp(static_cast<float>(page.validTexelCount) / static_cast<float>(page.allocatedInnerTexelCount), 0.0f, 1.0f)
				: 0.0f;
		}

		BuildPagePreviews(result);
		TallyWarningCounts(result);
		return result;
	}

	bool TryReconstructRasterSample(
		const LightmapUvRasterResult& result,
		const LightmapUvRasterPageBuffers& page,
		size_t linearIndex,
		LightmapRasterSample& outSample) {
		return ReconstructSampleInternal(result, page, linearIndex, outSample);
	}

	bool RunLightmapUvRasterizerSelfCheck(std::string& outMessage) {
		outMessage.clear();

		const std::vector<LightmapAtlasPage> pages{
			LightmapAtlasPage{
				0,
				"lm_page_000",
				8,
				8,
				0,
				{}
			}
		};

		LightmapBakeReceiverSnapshot square{};
		square.entity = 1u;
		square.stableId = 101u;
		square.entityName = "Raster Self-Check Quad";
		square.pageIndex = 0;
		square.pageWidth = 8u;
		square.pageHeight = 8u;
		square.placement.pageIndex = 0;
		square.placement.pageId = "lm_page_000";
		square.placement.innerX = 0;
		square.placement.innerY = 0;
		square.placement.innerWidth = 8;
		square.placement.innerHeight = 8;
		square.placement.uvScale = { 1.0f, 1.0f };
		square.placement.uvOffset = { 0.0f, 0.0f };

		const NE::Math::Vec3 n{ 0.0f, 1.0f, 0.0f };
		square.triangles.push_back({
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 1.0f },
			n, n, n, n,
			{ 0.0f, 0.0f },
			{ 1.0f, 0.0f },
			{ 1.0f, 1.0f },
			0u
		});
		square.triangles.push_back({
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 1.0f },
			{ 0.0f, 0.0f, 1.0f },
			n, n, n, n,
			{ 0.0f, 0.0f },
			{ 1.0f, 1.0f },
			{ 0.0f, 1.0f },
			1u
		});

		LightmapBakeReceiverSnapshot boundary = square;
		boundary.entity = 2u;
		boundary.stableId = 202u;
		boundary.entityName = "Raster Self-Check Boundary";
		boundary.placement.innerX = 2;
		boundary.placement.innerY = 2;
		boundary.placement.innerWidth = 4;
		boundary.placement.innerHeight = 4;
		boundary.placement.uvScale = { 0.5f, 0.5f };
		boundary.placement.uvOffset = { 0.0f, 0.0f };
		boundary.triangles.clear();
		boundary.triangles.push_back({
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f },
			n, n, n, n,
			{ -0.25f, -0.25f },
			{ 1.25f, -0.25f },
			{ -0.25f, 1.25f },
			0u
		});

		LightmapBakeReceiverSnapshot degenerate = square;
		degenerate.entity = 3u;
		degenerate.stableId = 303u;
		degenerate.entityName = "Raster Self-Check Degenerate";
		degenerate.triangles.clear();
		degenerate.triangles.push_back({
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f },
			n, n, n, n,
			{ 0.1f, 0.1f },
			{ 0.1f, 0.1f },
			{ 0.1f, 0.1f },
			0u
		});

		const LightmapUvRasterizationSettings settings{};
		const LightmapUvRasterResult squareResult = RasterizeLightmapUv1Atlas(pages, { square }, settings, nullptr);
		if (squareResult.pageBuffers.empty() || squareResult.pageBuffers[0].validTexelCount != 64u) {
			outMessage = "Self-check failed: shared-edge quad did not cover the full 8x8 inner rect exactly once.";
			return false;
		}
		if (squareResult.stats.ownershipConflictCount != 0u) {
			outMessage = "Self-check failed: shared-edge quad produced ownership conflicts.";
			return false;
		}

		LightmapBakeReceiverSnapshot reversed = square;
		std::swap(reversed.triangles[0].uv1, reversed.triangles[0].uv2);
		std::swap(reversed.triangles[0].p1, reversed.triangles[0].p2);
		std::swap(reversed.triangles[0].shadingNormal1, reversed.triangles[0].shadingNormal2);
		const LightmapUvRasterResult windingResult = RasterizeLightmapUv1Atlas(pages, { reversed }, settings, nullptr);
		if (windingResult.pageBuffers.empty() ||
			windingResult.pageBuffers[0].validMask != squareResult.pageBuffers[0].validMask) {
			outMessage = "Self-check failed: clockwise and counter-clockwise UV winding did not produce the same valid texel mask.";
			return false;
		}

		const LightmapUvRasterResult boundaryResult = RasterizeLightmapUv1Atlas(pages, { boundary }, settings, nullptr);
		if (boundaryResult.pageBuffers.empty()) {
			outMessage = "Self-check failed: boundary test produced no page buffers.";
			return false;
		}
		for (size_t i = 0; i < boundaryResult.pageBuffers[0].validMask.size(); ++i) {
			if (boundaryResult.pageBuffers[0].validMask[i] == 0u) {
				continue;
			}
			const uint32_t x = static_cast<uint32_t>(i % 8u);
			const uint32_t y = static_cast<uint32_t>(i / 8u);
			if (x < 2u || x > 5u || y < 2u || y > 5u) {
				outMessage = "Self-check failed: boundary rasterization wrote outside the allocated inner rect.";
				return false;
			}
		}

		const LightmapUvRasterResult degenerateResult = RasterizeLightmapUv1Atlas(pages, { degenerate }, settings, nullptr);
		if (degenerateResult.stats.degenerateUvTriangleCount == 0u || degenerateResult.stats.coveredTexelCount != 0u) {
			outMessage = "Self-check failed: degenerate UV triangles were not skipped safely.";
			return false;
		}

		for (const auto& page : squareResult.pageBuffers) {
			for (size_t linearIndex = 0; linearIndex < page.validMask.size(); ++linearIndex) {
				if (page.validMask[linearIndex] == 0u) {
					continue;
				}
				LightmapRasterSample sample{};
				if (!TryReconstructRasterSample(squareResult, page, linearIndex, sample)) {
					outMessage = "Self-check failed: covered texel reconstruction returned invalid sample data.";
					return false;
				}
			}
		}

		const LightmapUvRasterResult determinismA = RasterizeLightmapUv1Atlas(pages, { square, boundary }, settings, nullptr);
		const LightmapUvRasterResult determinismB = RasterizeLightmapUv1Atlas(pages, { square, boundary }, settings, nullptr);
		if (determinismA.pageBuffers.size() != determinismB.pageBuffers.size()) {
			outMessage = "Self-check failed: repeated rasterization produced a different page count.";
			return false;
		}
		for (size_t i = 0; i < determinismA.pageBuffers.size(); ++i) {
			if (determinismA.pageBuffers[i].validMask != determinismB.pageBuffers[i].validMask ||
				determinismA.pageBuffers[i].ownerReceiverIndex != determinismB.pageBuffers[i].ownerReceiverIndex ||
				determinismA.pageBuffers[i].ownerTriangleIndex != determinismB.pageBuffers[i].ownerTriangleIndex ||
				determinismA.pageBuffers[i].ownerSourceTriangleIndex != determinismB.pageBuffers[i].ownerSourceTriangleIndex) {
				outMessage = "Self-check failed: repeated rasterization produced non-deterministic ownership buffers.";
				return false;
			}
			if (determinismA.pageBuffers[i].barycentrics.size() != determinismB.pageBuffers[i].barycentrics.size()) {
				outMessage = "Self-check failed: repeated rasterization produced non-deterministic barycentric buffer sizes.";
				return false;
			}
			for (size_t baryIndex = 0; baryIndex < determinismA.pageBuffers[i].barycentrics.size(); ++baryIndex) {
				if (!AreBarycentricsNearlyEqual(
					determinismA.pageBuffers[i].barycentrics[baryIndex],
					determinismB.pageBuffers[i].barycentrics[baryIndex])) {
					outMessage = "Self-check failed: repeated rasterization produced non-deterministic barycentric buffers.";
					return false;
				}
			}
		}

		outMessage = "Raster self-check passed: shared edges, winding, bounds, degenerates, reconstruction, and determinism are all valid.";
		return true;
	}
}
