#include "pch.h"
#include "DirectLightmapBaker.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <variant>

#include "SceneBakeBVH.hpp"

#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Light.hpp>
#include <ECS/Components/Transform.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <Engine.hpp>

namespace Editor::Lightmapping {
	namespace {
		constexpr float kFiniteEpsilon = 1e-6f;
		constexpr size_t kMaxWarningExamples = 64;

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
			std::vector<LightmapBakeReceiverSnapshot> receivers;
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

		bool IsFiniteVec3(const NE::Math::Vec3& value) {
			return IsFiniteFloat(value.x) && IsFiniteFloat(value.y) && IsFiniteFloat(value.z);
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

		float BoundsDiagonalLength(const BakeAABB& bounds) {
			if (!IsFiniteBounds(bounds)) {
				return 0.0f;
			}

			const NE::Math::Vec3 extent = bounds.max - bounds.min;
			const float diagonal = extent.Length();
			return std::isfinite(diagonal) ? diagonal : 0.0f;
		}

		std::string GetEntityNameOrFallback(uint32_t entity) {
			if (!NE::ECS::Query::HasEntityMeta(entity)) {
				return "Entity " + std::to_string(entity);
			}

			const auto& meta = NE::ECS::Query::GetEntityMeta(entity);
			return meta.name.empty() ? ("Entity " + std::to_string(entity)) : meta.name;
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
				bool validMatrix = true;
				for (float value : transform.worldMatrix.a) {
					if (!std::isfinite(value)) {
						validMatrix = false;
						break;
					}
				}
				if (!validMatrix) {
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

			outInput.receivers = CollectLightmapBakeReceiverSnapshots(placements, outInput.pages, outInput.warnings);
			outInput.directionalShadowDistance = std::max(GetSceneBakeBVHSessionState().sceneDiagonalLength + 1.0f, 1.0f);
			outInput.sceneBounds = GetSceneBakeBVHSessionState().sceneBounds;
			outInput.lights = CollectBakeLights(outInput.warnings);

			const bool needsShadowBvh = std::any_of(outInput.lights.begin(), outInput.lights.end(),
				[](const BakeLightSnapshot& light) { return light.castsBakedShadow; });
			if (needsShadowBvh && !HasValidSceneBakeBVH()) {
				outMessage = "Direct bake requires a valid scene bake BVH because one or more lights cast baked shadows.";
				return false;
			}

			if (outInput.receivers.empty()) {
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

			DirectLightBakeResult result{};
			result.settings = input.settings;
			result.stats.bakeInstanceCount = input.receivers.size();
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

			PublishProgress("Rasterizing UV1 atlas ownership", 0u, input.receivers.size(), result.stats);
			const auto rasterStart = std::chrono::high_resolution_clock::now();
			LightmapUvRasterizationSettings rasterSettings{};
			rasterSettings.workerCount = input.settings.workerCount;
			rasterSettings.generateDebugPreviews = true;
			auto rasterResult = std::make_shared<LightmapUvRasterResult>(
				RasterizeLightmapUv1Atlas(input.pages, input.receivers, rasterSettings, &control.cancelRequested));
			const auto rasterEnd = std::chrono::high_resolution_clock::now();
			result.stats.rasterizationMs =
				std::chrono::duration<double, std::milli>(rasterEnd - rasterStart).count();
			result.stats.rasterTriangleCount = rasterResult->stats.triangleCount;
			result.stats.rasterDegenerateUvTriangleCount = rasterResult->stats.degenerateUvTriangleCount;
			result.stats.rasterUncoveredTexelCount = rasterResult->stats.uncoveredTexelCount;
			result.stats.rasterOwnershipConflictCount = rasterResult->stats.ownershipConflictCount;
			result.stats.rasterInvalidBarycentricTexelCount = rasterResult->stats.invalidBarycentricTexelCount;
			result.stats.rasterInvalidSampleTexelCount = rasterResult->stats.invalidSampleTexelCount;
			result.stats.rasterInnerRectClampedTriangleCount = rasterResult->stats.innerRectClampedTriangleCount;
			result.stats.coveredTexelCount = rasterResult->stats.coveredTexelCount;
			result.stats.skippedTexelCount =
				rasterResult->stats.invalidBarycentricTexelCount +
				rasterResult->stats.invalidSampleTexelCount;
			result.rasterResult = rasterResult;
			result.warnings.insert(result.warnings.end(), rasterResult->warnings.begin(), rasterResult->warnings.end());

			result.pages.reserve(input.pages.size());
			std::unordered_map<int, size_t> pageSlotsByIndex;
			pageSlotsByIndex.reserve(input.pages.size());
			for (size_t pageIndex = 0; pageIndex < input.pages.size(); ++pageIndex) {
				const auto& page = input.pages[pageIndex];
				DirectLightBakePageBuffers pageBuffers{};
				pageBuffers.pageIndex = page.pageIndex;
				pageBuffers.pageId = page.pageId;
				pageBuffers.width = static_cast<uint32_t>(std::max(page.width, 0));
				pageBuffers.height = static_cast<uint32_t>(std::max(page.height, 0));
				const size_t texelCount = static_cast<size_t>(pageBuffers.width) * static_cast<size_t>(pageBuffers.height);
				pageBuffers.lighting.assign(texelCount, { 0.0f, 0.0f, 0.0f });
				if (pageIndex < rasterResult->pageBuffers.size()) {
					pageBuffers.validMask = rasterResult->pageBuffers[pageIndex].validMask;
					pageBuffers.validTexelCount = rasterResult->pageBuffers[pageIndex].validTexelCount;
				} else {
					pageBuffers.validMask.assign(texelCount, 0u);
				}
				if (input.settings.generateDebugBuffers) {
					pageBuffers.ownerEntity.assign(texelCount, 0u);
					pageBuffers.ownerTriangle.assign(texelCount, std::numeric_limits<uint32_t>::max());
					pageBuffers.worldNormal.assign(texelCount, { 0.0f, 1.0f, 0.0f });
				}
				pageSlotsByIndex.emplace(page.pageIndex, result.pages.size());
				result.pages.push_back(std::move(pageBuffers));
			}

			const auto evaluationStart = std::chrono::high_resolution_clock::now();
			std::atomic<size_t> nextReceiver{ 0u };
			std::atomic<size_t> processedReceivers{ 0u };
			std::mutex statsMutex;
			DirectLightBakeStats liveStats = result.stats;

			const uint32_t requestedWorkerCount = input.settings.workerCount != 0
				? input.settings.workerCount
				: std::max(1u, std::thread::hardware_concurrency() > 1u ? std::thread::hardware_concurrency() - 1u : 1u);
			const uint32_t workerCount = std::max(1u, std::min<uint32_t>(requestedWorkerCount, static_cast<uint32_t>(input.receivers.size())));

			auto processReceiver = [&](size_t receiverIndex, DirectLightBakeStats& localStats) {
				const auto& receiver = input.receivers[receiverIndex];
				const auto pageSlotIt = pageSlotsByIndex.find(receiver.pageIndex);
				if (pageSlotIt == pageSlotsByIndex.end()) {
					++localStats.skippedTexelCount;
					return;
				}

				auto& page = result.pages[pageSlotIt->second];
				const auto& rasterPage = rasterResult->pageBuffers[pageSlotIt->second];
				const int minX = receiver.placement.innerX;
				const int minY = receiver.placement.innerY;
				const int maxX = receiver.placement.innerX + receiver.placement.innerWidth - 1;
				const int maxY = receiver.placement.innerY + receiver.placement.innerHeight - 1;

				for (int y = minY; y <= maxY; ++y) {
					for (int x = minX; x <= maxX; ++x) {
						if (control.cancelRequested.load()) {
							return;
						}

						const size_t linearIndex = static_cast<size_t>(y) * static_cast<size_t>(page.width) + static_cast<size_t>(x);
						if (linearIndex >= rasterPage.validMask.size() ||
							rasterPage.validMask[linearIndex] == 0u ||
							rasterPage.ownerReceiverIndex[linearIndex] != receiverIndex) {
							continue;
						}

						LightmapRasterSample sample{};
						if (!TryReconstructRasterSample(*rasterResult, rasterPage, linearIndex, sample)) {
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
								const NE::Math::Vec3 lightVector = light.position - sample.worldPosition;
								lightDistance = lightVector.Length();
								if (!std::isfinite(lightDistance) || lightDistance <= kFiniteEpsilon || lightDistance > light.range) {
									continue;
								}
								lightDirection = lightVector / lightDistance;
								attenuation = DistanceAttenuation(lightDistance, light.range);
								break;
							}
							case BakeLightKind::Spot: {
								const NE::Math::Vec3 lightVector = light.position - sample.worldPosition;
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

							const float nDotL = sample.shadingNormal.Dot(lightDirection);
							if (!std::isfinite(nDotL) || nDotL <= 0.0f) {
								continue;
							}

							bool occluded = false;
							if (light.castsBakedShadow) {
								NE::Math::Vec3 biasNormal = sample.geometricNormal;
								if (biasNormal.Dot(sample.shadingNormal) < 0.0f) {
									biasNormal = -biasNormal;
								}
								biasNormal = SafeNormalize(biasNormal, sample.shadingNormal);

								BakeRay ray{};
								ray.origin = sample.worldPosition + (biasNormal * input.settings.rayOriginBias);
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

						page.lighting[linearIndex] = accumulated;
						if (input.settings.generateDebugBuffers) {
							page.ownerEntity[linearIndex] = sample.entity;
							page.ownerTriangle[linearIndex] = sample.sourceTriangleIndex;
							page.worldNormal[linearIndex] = sample.shadingNormal;
						}
					}
				}
			};

			auto workerFn = [&]() {
				while (!control.cancelRequested.load()) {
					const size_t receiverIndex = nextReceiver.fetch_add(1u);
					if (receiverIndex >= input.receivers.size()) {
						break;
					}

					DirectLightBakeStats localStats{};
					processReceiver(receiverIndex, localStats);

					DirectLightBakeStats mergedStats{};
					size_t processed = 0u;
					{
						std::scoped_lock statsLock(statsMutex);
						liveStats.skippedTexelCount += localStats.skippedTexelCount;
						liveStats.raysCast += localStats.raysCast;
						liveStats.occludedRayCount += localStats.occludedRayCount;
						liveStats.visibleRayCount += localStats.visibleRayCount;
						mergedStats = liveStats;
						processed = processedReceivers.fetch_add(1u) + 1u;
					}
					PublishProgress("Evaluating direct lighting", processed, input.receivers.size(), mergedStats);
				}
			};

			PublishProgress("Evaluating direct lighting", 0u, input.receivers.size(), liveStats);
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

			{
				std::scoped_lock statsLock(statsMutex);
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
				control.state.processedInstanceCount = input.receivers.size();
				control.state.queuedInstanceCount = input.receivers.size();
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
			control.state.queuedInstanceCount = input.receivers.size();
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
