#include "pch.h"
#include "DirectLightmapBaker.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <variant>

#include "SceneBakeBVH.hpp"

#include <Core/SpdLogger.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Light.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Transform.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <EditorInterface/RendererExports.hpp>
#include <Engine.hpp>
#include <Graphics/Core/Model.hpp>
#include <Graphics/Core/Vertex.hpp>
#include <Math/Mat4.hpp>
#include <Math/Vec2.hpp>
#include <Math/Vec4.hpp>

namespace Editor::Lightmapping {
	namespace {
		constexpr float kFiniteEpsilon = 1e-6f;
		constexpr float kUvAreaEpsilon = 1e-8f;
		constexpr size_t kMaxWarningExamples = 64;

		struct BakeReceiverTriangle {
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

		struct BakeReceiverInstance {
			uint32_t entity = NE::ECS::NO_ENTITY;
			uint64_t stableId = 0;
			std::string entityName;
			LightmapPlacement placement{};
			std::vector<BakeReceiverTriangle> triangles;
		};

		enum class BakeLightKind : uint8_t {
			Directional,
			Point,
			Spot
		};

		struct BakeLightSnapshot {
			uint32_t entity = NE::ECS::NO_ENTITY;
			uint64_t stableId = 0;
			std::string entityName;
			BakeLightKind kind = BakeLightKind::Directional;
			NE::Math::Vec3 position{ 0.0f, 0.0f, 0.0f };
			NE::Math::Vec3 direction{ 0.0f, -1.0f, 0.0f };
			NE::Math::Vec3 color{ 1.0f, 1.0f, 1.0f };
			float intensity = 0.0f;
			float range = 0.0f;
			float innerCos = 0.0f;
			float outerCos = 0.0f;
			bool castsBakedShadow = false;
		};

		struct PreparedBakeInput {
			DirectLightBakeSettings settings{};
			std::vector<LightmapAtlasPage> pages;
			std::vector<BakeReceiverInstance> instances;
			std::vector<BakeLightSnapshot> lights;
			std::vector<std::string> warnings;
			BakeAABB sceneBounds{};
			float directionalShadowDistance = 0.0f;
		};

		struct WorkerControl {
			std::mutex mutex;
			DirectLightBakeSessionState state{};
			std::thread workerThread;
			std::atomic<bool> cancelRequested = false;
			std::atomic<bool> workerFinished = false;
		};

		WorkerControl& Control() {
			static WorkerControl control;
			return control;
		}

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

			BakeAABB result{};
			result.min.x = std::min(lhs.min.x, rhs.min.x);
			result.min.y = std::min(lhs.min.y, rhs.min.y);
			result.min.z = std::min(lhs.min.z, rhs.min.z);
			result.max.x = std::max(lhs.max.x, rhs.max.x);
			result.max.y = std::max(lhs.max.y, rhs.max.y);
			result.max.z = std::max(lhs.max.z, rhs.max.z);
			return result;
		}

		float BoundsDiagonalLength(const BakeAABB& bounds) {
			if (!IsFiniteBounds(bounds)) {
				return 0.0f;
			}

			const NE::Math::Vec3 extent = bounds.max - bounds.min;
			const float diagonal = extent.Length();
			return std::isfinite(diagonal) ? diagonal : 0.0f;
		}

		NE::Math::Vec3 TransformPoint(const NE::Math::Mat4& matrix, const NE::Math::Vec3& point) {
			const NE::Math::Vec4 transformed = matrix * NE::Math::Vec4(point.x, point.y, point.z, 1.0f);
			return { transformed.x, transformed.y, transformed.z };
		}

		NE::Math::Vec3 TransformDirection(const NE::Math::Mat4& matrix, const NE::Math::Vec3& direction) {
			const NE::Math::Vec4 transformed = matrix * NE::Math::Vec4(direction.x, direction.y, direction.z, 0.0f);
			return { transformed.x, transformed.y, transformed.z };
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

		std::vector<LightmapPlacement> CollectPlacements(const LightmapAllocationPreviewState& previewState) {
			std::vector<LightmapPlacement> placements;
			for (const auto& page : previewState.pages) {
				for (const auto& placement : page.placements) {
					placements.push_back(placement);
				}
			}

			std::sort(placements.begin(), placements.end(),
				[](const LightmapPlacement& lhs, const LightmapPlacement& rhs) {
					if (lhs.pageIndex != rhs.pageIndex) {
						return lhs.pageIndex < rhs.pageIndex;
					}
					if (lhs.innerY != rhs.innerY) {
						return lhs.innerY < rhs.innerY;
					}
					if (lhs.innerX != rhs.innerX) {
						return lhs.innerX < rhs.innerX;
					}
					return lhs.entity < rhs.entity;
				});
			return placements;
		}

		std::vector<BakeReceiverInstance> CollectBakeInstances(
			const std::vector<LightmapPlacement>& placements,
			std::vector<std::string>& warnings) {
			std::vector<BakeReceiverInstance> instances;
			instances.reserve(placements.size());

			for (const auto& placement : placements) {
				const uint32_t entity = placement.entity;
				const std::string entityName = GetEntityNameOrFallback(entity);

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

				const float determinant = transform.worldMatrix.Determinant();
				const bool hasValidNormalMatrix = std::isfinite(determinant) && std::fabs(determinant) > kFiniteEpsilon;
				const NE::Math::Mat4 normalMatrix = hasValidNormalMatrix
					? transform.worldMatrix.Inverse().Transpose()
					: transform.worldMatrix;

				BakeReceiverInstance instance{};
				instance.entity = entity;
				instance.stableId = meta.luid != 0 ? meta.luid : static_cast<uint64_t>(entity);
				instance.entityName = entityName;
				instance.placement = placement;
				instance.triangles.reserve(submesh.indices.size() / 3u);

				for (size_t index = 0; index + 2 < submesh.indices.size(); index += 3) {
					const uint32_t ia = submesh.indices[index + 0];
					const uint32_t ib = submesh.indices[index + 1];
					const uint32_t ic = submesh.indices[index + 2];
					if (ia >= submesh.vertices.size() || ib >= submesh.vertices.size() || ic >= submesh.vertices.size()) {
						continue;
					}

					const auto& va = submesh.vertices[ia];
					const auto& vb = submesh.vertices[ib];
					const auto& vc = submesh.vertices[ic];

					const NE::Math::Vec3 p0 = TransformPoint(transform.worldMatrix, va.position);
					const NE::Math::Vec3 p1 = TransformPoint(transform.worldMatrix, vb.position);
					const NE::Math::Vec3 p2 = TransformPoint(transform.worldMatrix, vc.position);
					if (!IsFiniteVec3(p0) || !IsFiniteVec3(p1) || !IsFiniteVec3(p2)) {
						continue;
					}

					const NE::Math::Vec3 geometricCross = (p1 - p0).Cross(p2 - p0);
					const float doubleArea = geometricCross.Length();
					if (!std::isfinite(doubleArea) || doubleArea <= kFiniteEpsilon) {
						continue;
					}

					if (!IsFiniteVec2(va.texCoord1) || !IsFiniteVec2(vb.texCoord1) || !IsFiniteVec2(vc.texCoord1)) {
						continue;
					}

					BakeReceiverTriangle triangle{};
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
					instance.triangles.push_back(triangle);
				}

				if (instance.triangles.empty()) {
					PushWarning(warnings, entityName + ": skipped bake receiver because no valid world-space UV1 triangles remained after validation.");
					continue;
				}

				instances.push_back(std::move(instance));
			}

			std::sort(instances.begin(), instances.end(),
				[](const BakeReceiverInstance& lhs, const BakeReceiverInstance& rhs) {
					if (lhs.placement.pageIndex != rhs.placement.pageIndex) {
						return lhs.placement.pageIndex < rhs.placement.pageIndex;
					}
					return lhs.stableId < rhs.stableId;
				});
			return instances;
		}

		bool ValidatePageLayout(const std::vector<LightmapAtlasPage>& pages, std::string& outMessage) {
			std::unordered_map<int, std::string> pageIdsByIndex;
			pageIdsByIndex.reserve(pages.size());

			for (const auto& page : pages) {
				if (page.pageIndex < 0) {
					outMessage = "Atlas allocation produced an invalid page index for direct baking.";
					return false;
				}

				const auto [it, inserted] = pageIdsByIndex.emplace(page.pageIndex, page.pageId);
				if (!inserted) {
					outMessage = "Atlas allocation produced duplicate page indices for direct baking.";
					return false;
				}
			}

			return true;
		}

		std::vector<BakeLightSnapshot> CollectBakeLights(std::vector<std::string>& warnings) {
			std::vector<BakeLightSnapshot> lights;
			const auto& entities = NE::GetNumEntities();
			lights.reserve(entities.size());

			for (uint32_t entity : entities) {
				if (!NE::ECS::Query::HasLight(entity) ||
					!NE::ECS::Query::HasTransform(entity) ||
					!NE::ECS::Query::GetActive(entity)) {
					continue;
				}

				const auto& light = NE::ECS::Query::GetEntityLight(entity);
				const auto& transform = NE::ECS::Query::GetEntityTransform(entity);
				if (!IsFiniteMatrix(transform.worldMatrix)) {
					PushWarning(warnings, GetEntityNameOrFallback(entity) + ": skipped light because the world transform is non-finite.");
					continue;
				}

				BakeLightSnapshot snapshot{};
				snapshot.entity = entity;
				snapshot.stableId = NE::ECS::Query::HasEntityMeta(entity)
					? (NE::ECS::Query::GetEntityMeta(entity).luid != 0 ? NE::ECS::Query::GetEntityMeta(entity).luid : static_cast<uint64_t>(entity))
					: static_cast<uint64_t>(entity);
				snapshot.entityName = GetEntityNameOrFallback(entity);
				snapshot.position = transform.worldMatrix.GetTranslation();
				snapshot.direction = transform.worldMatrix.Forward().Normalized();
				if (!IsFiniteVec3(snapshot.direction) || snapshot.direction.LengthSquared() <= kFiniteEpsilon) {
					snapshot.direction = { 0.0f, -1.0f, 0.0f };
				}
				snapshot.color = light.color;
				snapshot.castsBakedShadow = (light.shadowType != NE::ECS::Component::Light::ShadowType::None);

				bool supported = true;
				std::visit([&](const auto& lightData) {
					using T = std::decay_t<decltype(lightData)>;
					snapshot.intensity = lightData.intensity;

					if constexpr (std::is_same_v<T, NE::ECS::Component::Light::DirectionalLightData>) {
						snapshot.kind = BakeLightKind::Directional;
					} else if constexpr (std::is_same_v<T, NE::ECS::Component::Light::PointLightData>) {
						snapshot.kind = BakeLightKind::Point;
						snapshot.range = lightData.range;
					} else if constexpr (std::is_same_v<T, NE::ECS::Component::Light::SpotLightData>) {
						snapshot.kind = BakeLightKind::Spot;
						snapshot.range = lightData.range;
						if (!std::isfinite(lightData.innerConeAngleDeg) || !std::isfinite(lightData.outerConeAngleDeg)) {
							PushWarning(warnings, snapshot.entityName + ": skipped spot light because its cone angles are non-finite.");
							supported = false;
							return;
						}
						if (lightData.innerConeAngleDeg > lightData.outerConeAngleDeg) {
							PushWarning(warnings, snapshot.entityName + ": skipped spot light because inner cone angle exceeds outer cone angle.");
							supported = false;
							return;
						}
						snapshot.innerCos = std::cos(lightData.innerConeAngleDeg * NE::Math::DEG_TO_RAD);
						snapshot.outerCos = std::cos(lightData.outerConeAngleDeg * NE::Math::DEG_TO_RAD);
					} else {
						supported = false;
					}
				}, light.data);

				if (!supported) {
					continue;
				}

				if (!IsFiniteVec3(snapshot.color) || snapshot.intensity <= 0.0f || !std::isfinite(snapshot.intensity)) {
					continue;
				}

				if ((snapshot.kind == BakeLightKind::Point || snapshot.kind == BakeLightKind::Spot) &&
					(!std::isfinite(snapshot.range) || snapshot.range <= 0.0f)) {
					PushWarning(warnings, snapshot.entityName + ": skipped light because its range is invalid for baking.");
					continue;
				}

				lights.push_back(snapshot);
			}

			std::sort(lights.begin(), lights.end(),
				[](const BakeLightSnapshot& lhs, const BakeLightSnapshot& rhs) {
					return lhs.stableId < rhs.stableId;
				});
			return lights;
		}

		bool PrepareBakeInput(const DirectLightBakeSettings& settings, PreparedBakeInput& outInput, std::string& outMessage) {
			outInput = {};
			outInput.settings = settings;
			outInput.sceneBounds = InvalidBounds();
			outMessage.clear();

			const auto& previewState = GetLightmapAllocationPreviewState();
			if (!previewState.hasRun || previewState.pages.empty()) {
				outMessage = "Run atlas allocation before baking direct lightmaps.";
				return false;
			}

			SceneBakeBVHBuildSettings bvhSettings{};
			const auto& bvhState = GetSceneBakeBVHSessionState();
			if (bvhState.settings.maxLeafPrimitives > 0) {
				bvhSettings = bvhState.settings;
			}

			if (settings.rebuildBvhBeforeBake || !HasValidSceneBakeBVH()) {
				BuildSceneBakeBVHFromCurrentScene(bvhSettings);
			}

			const auto placements = CollectPlacements(previewState);
			outInput.pages = previewState.pages;
			if (!ValidatePageLayout(outInput.pages, outMessage)) {
				return false;
			}
			outInput.instances = CollectBakeInstances(placements, outInput.warnings);
			outInput.directionalShadowDistance = std::max(GetSceneBakeBVHSessionState().sceneDiagonalLength + 1.0f, 1.0f);
			outInput.sceneBounds = GetSceneBakeBVHSessionState().sceneBounds;

			outInput.lights = CollectBakeLights(outInput.warnings);

			const bool needsShadowBvh = std::any_of(outInput.lights.begin(), outInput.lights.end(),
				[](const BakeLightSnapshot& light) { return light.castsBakedShadow; });
			if (needsShadowBvh && !HasValidSceneBakeBVH()) {
				outMessage = "Direct bake requires a valid scene bake BVH because one or more lights cast baked shadows.";
				return false;
			}

			if (outInput.instances.empty()) {
				outMessage = "No allocated bake receivers were available for direct lighting.";
				return false;
			}

			return true;
		}

		float DistanceAttenuation(float distance, float range) {
			if (range <= 0.0f) {
				return 0.0f;
			}

			const float x = std::clamp(distance / range, 0.0f, 1.0f);
			const float attenuation = 1.0f - x;
			return attenuation * attenuation;
		}

		float EdgeFunction(const NE::Math::Vec2& a, const NE::Math::Vec2& b, const NE::Math::Vec2& p) {
			return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
		}

		bool IsTopLeftEdge(const NE::Math::Vec2& a, const NE::Math::Vec2& b) {
			const NE::Math::Vec2 edge = b - a;
			return (edge.y > 0.0f) || (std::fabs(edge.y) <= kFiniteEpsilon && edge.x < 0.0f);
		}

		bool PassesFillRule(float edgeValue, bool topLeft, bool positiveArea) {
			if (positiveArea) {
				return edgeValue > kFiniteEpsilon || (std::fabs(edgeValue) <= kFiniteEpsilon && topLeft);
			}
			return edgeValue < -kFiniteEpsilon || (std::fabs(edgeValue) <= kFiniteEpsilon && topLeft);
		}

		NE::Math::Vec3 SafeNormalize(const NE::Math::Vec3& value, const NE::Math::Vec3& fallback) {
			if (!IsFiniteVec3(value) || value.LengthSquared() <= kFiniteEpsilon) {
				return fallback;
			}
			return value.Normalized();
		}

		void PublishProgress(const std::string& stage, size_t processedInstances, size_t totalInstances, const DirectLightBakeStats& liveStats) {
			auto& control = Control();
			std::scoped_lock lock(control.mutex);
			control.state.activeStage = stage;
			control.state.processedInstanceCount = processedInstances;
			control.state.queuedInstanceCount = totalInstances;
			control.state.progress01 = totalInstances > 0
				? std::clamp(static_cast<float>(processedInstances) / static_cast<float>(totalInstances), 0.0f, 1.0f)
				: 0.0f;
			control.state.liveStats = liveStats;
			control.state.statusMessage = stage + " (" + std::to_string(processedInstances) + "/" + std::to_string(totalInstances) + " instances)";
		}

		void BuildPagePreviews(DirectLightBakeResult& result) {
			for (auto& page : result.pages) {
				page.preview.pageIndex = page.pageIndex;
				page.preview.pageId = page.pageId;
				page.preview.width = page.width;
				page.preview.height = page.height;
				page.preview.validTexelCount = page.validTexelCount;
				page.preview.lightingRgba8.resize(static_cast<size_t>(page.width) * static_cast<size_t>(page.height) * 4u, 0u);
				page.preview.validityRgba8.resize(static_cast<size_t>(page.width) * static_cast<size_t>(page.height) * 4u, 0u);

				for (size_t i = 0; i < page.lighting.size(); ++i) {
					const NE::Math::Vec3 hdr = page.lighting[i] * result.settings.previewExposure;
					const NE::Math::Vec3 mapped{
						hdr.x / (1.0f + std::max(hdr.x, 0.0f)),
						hdr.y / (1.0f + std::max(hdr.y, 0.0f)),
						hdr.z / (1.0f + std::max(hdr.z, 0.0f))
					};

					const auto toByte = [](float value) -> uint8_t {
						const float gamma = std::pow(std::clamp(value, 0.0f, 1.0f), 1.0f / 2.2f);
						return static_cast<uint8_t>(std::clamp(gamma * 255.0f, 0.0f, 255.0f));
					};

					const size_t rgbaIndex = i * 4u;
					page.preview.lightingRgba8[rgbaIndex + 0u] = toByte(mapped.x);
					page.preview.lightingRgba8[rgbaIndex + 1u] = toByte(mapped.y);
					page.preview.lightingRgba8[rgbaIndex + 2u] = toByte(mapped.z);
					page.preview.lightingRgba8[rgbaIndex + 3u] = 255u;

					const uint8_t maskValue = page.validMask[i] != 0u ? 255u : 0u;
					page.preview.validityRgba8[rgbaIndex + 0u] = maskValue;
					page.preview.validityRgba8[rgbaIndex + 1u] = maskValue;
					page.preview.validityRgba8[rgbaIndex + 2u] = maskValue;
					page.preview.validityRgba8[rgbaIndex + 3u] = 255u;
				}
			}
		}

		void TallyWarningCounts(DirectLightBakeResult& result) {
			result.warningCounts.clear();
			for (const auto& warning : result.warnings) {
				result.warningCounts[warning]++;
			}
		}

		void RunBakeWorker(PreparedBakeInput input, double setupMs) {
			auto& control = Control();
			const auto evaluationStart = std::chrono::high_resolution_clock::now();

			DirectLightBakeResult result{};
			result.settings = input.settings;
			result.stats.bakeInstanceCount = input.instances.size();
			result.stats.pageCount = input.pages.size();
			result.stats.supportedLightCount = input.lights.size();
			result.stats.directionalLightCount = std::count_if(input.lights.begin(), input.lights.end(),
				[](const BakeLightSnapshot& light) { return light.kind == BakeLightKind::Directional; });
			result.stats.pointLightCount = std::count_if(input.lights.begin(), input.lights.end(),
				[](const BakeLightSnapshot& light) { return light.kind == BakeLightKind::Point; });
			result.stats.spotLightCount = std::count_if(input.lights.begin(), input.lights.end(),
				[](const BakeLightSnapshot& light) { return light.kind == BakeLightKind::Spot; });
			result.stats.setupMs = setupMs;
			result.warnings = input.warnings;

			result.pages.reserve(input.pages.size());
			std::unordered_map<int, size_t> pageSlotsByIndex;
			pageSlotsByIndex.reserve(input.pages.size());
			for (const auto& page : input.pages) {
				DirectLightBakePageBuffers pageBuffers{};
				pageBuffers.pageIndex = page.pageIndex;
				pageBuffers.pageId = page.pageId;
				pageBuffers.width = static_cast<uint32_t>(std::max(page.width, 0));
				pageBuffers.height = static_cast<uint32_t>(std::max(page.height, 0));
				const size_t texelCount = static_cast<size_t>(pageBuffers.width) * static_cast<size_t>(pageBuffers.height);
				pageBuffers.lighting.assign(texelCount, { 0.0f, 0.0f, 0.0f });
				pageBuffers.validMask.assign(texelCount, 0u);
				if (input.settings.generateDebugBuffers) {
					pageBuffers.ownerEntity.assign(texelCount, 0u);
					pageBuffers.ownerTriangle.assign(texelCount, std::numeric_limits<uint32_t>::max());
					pageBuffers.worldNormal.assign(texelCount, { 0.0f, 1.0f, 0.0f });
				}
				const size_t pageSlot = result.pages.size();
				result.pages.push_back(std::move(pageBuffers));
				pageSlotsByIndex.emplace(page.pageIndex, pageSlot);
			}

			std::atomic<size_t> nextInstance{ 0u };
			std::atomic<size_t> processedInstances{ 0u };
			std::mutex statsMutex;
			DirectLightBakeStats liveStats = result.stats;

			const uint32_t requestedWorkerCount = input.settings.workerCount != 0
				? input.settings.workerCount
				: std::max(1u, std::thread::hardware_concurrency() > 1u ? std::thread::hardware_concurrency() - 1u : 1u);
			const uint32_t workerCount = std::max(1u, std::min<uint32_t>(requestedWorkerCount, static_cast<uint32_t>(input.instances.size())));

			auto processInstance = [&](const BakeReceiverInstance& instance, DirectLightBakeStats& localStats) {
				const auto pageSlotIt = pageSlotsByIndex.find(instance.placement.pageIndex);
				if (pageSlotIt == pageSlotsByIndex.end()) {
					++localStats.skippedTexelCount;
					return;
				}

				auto& page = result.pages[pageSlotIt->second];
				const int minX = instance.placement.innerX;
				const int minY = instance.placement.innerY;
				const int maxX = instance.placement.innerX + instance.placement.innerWidth - 1;
				const int maxY = instance.placement.innerY + instance.placement.innerHeight - 1;

				for (const auto& triangle : instance.triangles) {
					if (control.cancelRequested.load()) {
						return;
					}

					const NE::Math::Vec2 atlas0{
						static_cast<float>(instance.placement.innerX) + (triangle.uv0.x * static_cast<float>(instance.placement.innerWidth)),
						static_cast<float>(instance.placement.innerY) + (triangle.uv0.y * static_cast<float>(instance.placement.innerHeight))
					};
					const NE::Math::Vec2 atlas1{
						static_cast<float>(instance.placement.innerX) + (triangle.uv1.x * static_cast<float>(instance.placement.innerWidth)),
						static_cast<float>(instance.placement.innerY) + (triangle.uv1.y * static_cast<float>(instance.placement.innerHeight))
					};
					const NE::Math::Vec2 atlas2{
						static_cast<float>(instance.placement.innerX) + (triangle.uv2.x * static_cast<float>(instance.placement.innerWidth)),
						static_cast<float>(instance.placement.innerY) + (triangle.uv2.y * static_cast<float>(instance.placement.innerHeight))
					};

					if (!IsFiniteVec2(atlas0) || !IsFiniteVec2(atlas1) || !IsFiniteVec2(atlas2)) {
						continue;
					}

					const float signedArea = EdgeFunction(atlas0, atlas1, atlas2);
					if (!std::isfinite(signedArea) || std::fabs(signedArea) <= kUvAreaEpsilon) {
						continue;
					}

					const bool positiveArea = signedArea > 0.0f;
					const bool topLeft0 = IsTopLeftEdge(atlas1, atlas2);
					const bool topLeft1 = IsTopLeftEdge(atlas2, atlas0);
					const bool topLeft2 = IsTopLeftEdge(atlas0, atlas1);

					const float boundsMinX = std::min(atlas0.x, std::min(atlas1.x, atlas2.x));
					const float boundsMinY = std::min(atlas0.y, std::min(atlas1.y, atlas2.y));
					const float boundsMaxX = std::max(atlas0.x, std::max(atlas1.x, atlas2.x));
					const float boundsMaxY = std::max(atlas0.y, std::max(atlas1.y, atlas2.y));

					const int startX = std::max(minX, static_cast<int>(std::ceil(boundsMinX - 0.5f)));
					const int startY = std::max(minY, static_cast<int>(std::ceil(boundsMinY - 0.5f)));
					const int endX = std::min(maxX, static_cast<int>(std::floor(boundsMaxX - 0.5f)));
					const int endY = std::min(maxY, static_cast<int>(std::floor(boundsMaxY - 0.5f)));
					if (startX > endX || startY > endY) {
						continue;
					}

					for (int y = startY; y <= endY; ++y) {
						for (int x = startX; x <= endX; ++x) {
							if (control.cancelRequested.load()) {
								return;
							}

							const NE::Math::Vec2 texelCenter{
								static_cast<float>(x) + 0.5f,
								static_cast<float>(y) + 0.5f
							};

							const float edge0 = EdgeFunction(atlas1, atlas2, texelCenter);
							const float edge1 = EdgeFunction(atlas2, atlas0, texelCenter);
							const float edge2 = EdgeFunction(atlas0, atlas1, texelCenter);
							if (!PassesFillRule(edge0, topLeft0, positiveArea) ||
								!PassesFillRule(edge1, topLeft1, positiveArea) ||
								!PassesFillRule(edge2, topLeft2, positiveArea)) {
								continue;
							}

							const float bary0 = edge0 / signedArea;
							const float bary1 = edge1 / signedArea;
							const float bary2 = edge2 / signedArea;
							if (!std::isfinite(bary0) || !std::isfinite(bary1) || !std::isfinite(bary2)) {
								++localStats.skippedTexelCount;
								continue;
							}

							const NE::Math::Vec3 worldPosition =
								triangle.p0 * bary0 +
								triangle.p1 * bary1 +
								triangle.p2 * bary2;
							const NE::Math::Vec3 shadingNormal = SafeNormalize(
								triangle.shadingNormal0 * bary0 +
								triangle.shadingNormal1 * bary1 +
								triangle.shadingNormal2 * bary2,
								triangle.geometricNormal);
							const NE::Math::Vec3 geometricNormal = SafeNormalize(triangle.geometricNormal, { 0.0f, 1.0f, 0.0f });
							if (!IsFiniteVec3(worldPosition) || !IsFiniteVec3(shadingNormal) || !IsFiniteVec3(geometricNormal)) {
								++localStats.skippedTexelCount;
								continue;
							}

							NE::Math::Vec3 accumulated{ 0.0f, 0.0f, 0.0f };
							for (const auto& light : input.lights) {
								NE::Math::Vec3 lightDirection{ 0.0f, 0.0f, 0.0f };
								float lightDistance = input.directionalShadowDistance;
								float attenuation = 1.0f;
								float spotAttenuation = 1.0f;

								switch (light.kind) {
								case BakeLightKind::Directional:
									lightDirection = SafeNormalize(-light.direction, { 0.0f, -1.0f, 0.0f });
									break;
								case BakeLightKind::Point: {
									const NE::Math::Vec3 lightVector = light.position - worldPosition;
									lightDistance = lightVector.Length();
									if (!std::isfinite(lightDistance) || lightDistance <= kFiniteEpsilon || lightDistance > light.range) {
										continue;
									}
									lightDirection = lightVector / lightDistance;
									attenuation = DistanceAttenuation(lightDistance, light.range);
									break;
								}
								case BakeLightKind::Spot: {
									const NE::Math::Vec3 lightVector = light.position - worldPosition;
									lightDistance = lightVector.Length();
									if (!std::isfinite(lightDistance) || lightDistance <= kFiniteEpsilon || lightDistance > light.range) {
										continue;
									}
									lightDirection = lightVector / lightDistance;
									const float theta = lightDirection.Dot(SafeNormalize(-light.direction, { 0.0f, -1.0f, 0.0f }));
									const float epsilon = std::max(light.innerCos - light.outerCos, 1e-4f);
									spotAttenuation = std::clamp((theta - light.outerCos) / epsilon, 0.0f, 1.0f);
									if (spotAttenuation <= 0.0f) {
										continue;
									}
									attenuation = DistanceAttenuation(lightDistance, light.range);
									break;
								}
								}

								const float nDotL = shadingNormal.Dot(lightDirection);
								if (!std::isfinite(nDotL) || nDotL <= 0.0f) {
									continue;
								}

								bool occluded = false;
								if (light.castsBakedShadow) {
									NE::Math::Vec3 biasNormal = geometricNormal;
									if (biasNormal.Dot(shadingNormal) < 0.0f) {
										biasNormal = -biasNormal;
									}
									biasNormal = SafeNormalize(biasNormal, shadingNormal);

									BakeRay ray{};
									ray.origin = worldPosition + (biasNormal * input.settings.rayOriginBias);
									ray.direction = lightDirection;
									ray.tMin = input.settings.rayMinDistance;
									ray.tMax = (light.kind == BakeLightKind::Directional)
										? input.directionalShadowDistance
										: std::max(lightDistance - input.settings.finiteLightDistanceEpsilon, ray.tMin);

									++localStats.raysCast;
									occluded = SceneBakeBVHAnyHit(ray);
									if (occluded) {
										++localStats.occludedRayCount;
									} else {
										++localStats.visibleRayCount;
									}
								}

								if (occluded) {
									continue;
								}

								accumulated += light.color * (light.intensity * attenuation * spotAttenuation * nDotL);
							}

							const size_t linearIndex = static_cast<size_t>(y) * static_cast<size_t>(page.width) + static_cast<size_t>(x);
							if (linearIndex >= page.lighting.size()) {
								++localStats.skippedTexelCount;
								continue;
							}

							if (page.validMask[linearIndex] != 0u) {
								continue;
							}

							page.lighting[linearIndex] = accumulated;
							page.validMask[linearIndex] = 1u;
							++localStats.coveredTexelCount;

							if (input.settings.generateDebugBuffers) {
								page.ownerEntity[linearIndex] = instance.entity;
								page.ownerTriangle[linearIndex] = triangle.sourceTriangleIndex;
								page.worldNormal[linearIndex] = shadingNormal;
							}
						}
					}
				}
			};

			auto workerFn = [&]() {
				while (!control.cancelRequested.load()) {
					const size_t instanceIndex = nextInstance.fetch_add(1u);
					if (instanceIndex >= input.instances.size()) {
						break;
					}

					DirectLightBakeStats localStats{};
					processInstance(input.instances[instanceIndex], localStats);

					DirectLightBakeStats mergedStats{};
					size_t processed = 0u;
					{
						std::scoped_lock statsLock(statsMutex);
						liveStats.coveredTexelCount += localStats.coveredTexelCount;
						liveStats.skippedTexelCount += localStats.skippedTexelCount;
						liveStats.raysCast += localStats.raysCast;
						liveStats.occludedRayCount += localStats.occludedRayCount;
						liveStats.visibleRayCount += localStats.visibleRayCount;
						mergedStats = liveStats;
						processed = processedInstances.fetch_add(1u) + 1u;
					}
					PublishProgress("Evaluating direct lighting", processed, input.instances.size(), mergedStats);
				}
			};

			PublishProgress("Evaluating direct lighting", 0u, input.instances.size(), liveStats);
			std::vector<std::thread> workers;
			workers.reserve(workerCount);
			for (uint32_t i = 0; i < workerCount; ++i) {
				workers.emplace_back(workerFn);
			}
			for (auto& worker : workers) {
				if (worker.joinable()) {
					worker.join();
				}
			}

			const auto evaluationEnd = std::chrono::high_resolution_clock::now();
			result.stats.evaluationMs =
				std::chrono::duration<double, std::milli>(evaluationEnd - evaluationStart).count();

			size_t totalValidTexelCount = 0u;
			for (auto& page : result.pages) {
				page.validTexelCount = static_cast<size_t>(std::count(page.validMask.begin(), page.validMask.end(), static_cast<uint8_t>(1u)));
				totalValidTexelCount += page.validTexelCount;
			}

			{
				std::scoped_lock statsLock(statsMutex);
				result.stats.coveredTexelCount = totalValidTexelCount;
				result.stats.skippedTexelCount = liveStats.skippedTexelCount;
				result.stats.raysCast = liveStats.raysCast;
				result.stats.occludedRayCount = liveStats.occludedRayCount;
				result.stats.visibleRayCount = liveStats.visibleRayCount;
			}

			TallyWarningCounts(result);
			BuildPagePreviews(result);

			{
				std::scoped_lock lock(control.mutex);
				control.state.liveStats = result.stats;
				control.state.processedInstanceCount = input.instances.size();
				control.state.queuedInstanceCount = input.instances.size();
				control.state.progress01 = 1.0f;
				control.state.cancelRequested = control.cancelRequested.load();

				if (control.cancelRequested.load()) {
					control.state.isRunning = false;
					control.state.lastBakeSucceeded = false;
					control.state.activeStage = "Cancelled";
					control.state.statusMessage = "Direct light bake cancelled.";
				} else {
					const uint64_t previousRevision =
						(control.state.hasResult && control.state.result)
						? control.state.result->revision
						: 0u;
					result.revision = previousRevision + 1u;
					control.state.result = std::make_shared<DirectLightBakeResult>(std::move(result));
					control.state.hasResult = true;
					control.state.isRunning = false;
					control.state.lastBakeSucceeded = true;
					control.state.activeStage = "Complete";
					control.state.statusMessage = "Direct light bake completed.";
				}
			}

			control.workerFinished.store(true);
		}
	}

	DirectLightBakeSessionState GetDirectLightBakeSessionState() {
		auto& control = Control();
		std::scoped_lock lock(control.mutex);
		return control.state;
	}

	void UpdateDirectLightBakeSession() {
		auto& control = Control();
		if (control.workerFinished.load() && control.workerThread.joinable()) {
			control.workerThread.join();
			control.workerFinished.store(false);
		}
	}

	bool StartSceneDirectLightBake(const DirectLightBakeSettings& settings) {
		UpdateDirectLightBakeSession();

		PreparedBakeInput input{};
		std::string failureMessage;
		const auto setupStart = std::chrono::high_resolution_clock::now();
		if (!PrepareBakeInput(settings, input, failureMessage)) {
			auto& control = Control();
			std::scoped_lock lock(control.mutex);
			control.state.settings = settings;
			control.state.isRunning = false;
			control.state.cancelRequested = false;
			control.state.lastBakeSucceeded = false;
			control.state.activeStage = "Idle";
			control.state.statusMessage = failureMessage;
			return false;
		}

		const auto setupEnd = std::chrono::high_resolution_clock::now();
		const double setupMs = std::chrono::duration<double, std::milli>(setupEnd - setupStart).count();

		auto& control = Control();
		{
			std::scoped_lock lock(control.mutex);
			if (control.workerThread.joinable()) {
				control.state.statusMessage = "A direct light bake is already running.";
				return false;
			}

			control.cancelRequested.store(false);
			control.workerFinished.store(false);
			control.state.isRunning = true;
			control.state.cancelRequested = false;
			control.state.lastBakeSucceeded = false;
			control.state.settings = settings;
			control.state.activeStage = "Preparing";
			control.state.statusMessage = "Preparing direct light bake snapshot.";
			control.state.progress01 = 0.0f;
			control.state.queuedInstanceCount = input.instances.size();
			control.state.processedInstanceCount = 0u;
			control.state.liveStats = {};
			control.state.liveStats.setupMs = setupMs;
		}

		control.workerThread = std::thread([capturedInput = std::move(input), setupMs]() mutable {
			RunBakeWorker(std::move(capturedInput), setupMs);
		});

		return true;
	}

	void CancelSceneDirectLightBake() {
		auto& control = Control();
		control.cancelRequested.store(true);
		std::scoped_lock lock(control.mutex);
		control.state.cancelRequested = true;
		if (control.state.isRunning) {
			control.state.statusMessage = "Cancelling direct light bake...";
			control.state.activeStage = "Cancelling";
		}
	}

	void ShutdownDirectLightBakeSession() {
		CancelSceneDirectLightBake();
		UpdateDirectLightBakeSession();
		auto& control = Control();
		if (control.workerThread.joinable()) {
			control.workerThread.join();
		}
	}
}
