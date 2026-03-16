#include "pch.h"
#include "DirectLightmapBaker.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <variant>

#include "SceneBakeBVH.hpp"

#include "../AssetManagement/AssetManager.hpp"
#include "../AssetManagement/UUID.hpp"
#include "../AssetManagement/Assets/LightmapAsset.hpp"
#include "../EditorScene.hpp"
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Light.hpp>
#include <ECS/Components/Transform.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <Engine.hpp>
#include <ResourceManagement/ResourceManager.hpp>
#include <Serialisation/BinaryReflection.hpp>
#include <SceneManagement/SceneLightmapRuntime.hpp>

namespace Editor::Lightmapping {
	namespace {
		constexpr float kFiniteEpsilon = 1e-6f;
		constexpr size_t kMaxWarningExamples = 64;
		constexpr int kAreaLightBakeSamples = 16;

		enum class BakeLightKind : uint8_t {
			Directional,
			Point,
			Spot,
			Area
		};

		struct BakeLightSnapshot {
			uint32_t entity = NE::ECS::NO_ENTITY;
			uint64_t stableId = 0;
			std::string entityName;
			BakeLightKind kind = BakeLightKind::Directional;
			NE::Math::Vec3 position{ 0.0f, 0.0f, 0.0f };
			NE::Math::Vec3 direction{ 0.0f, -1.0f, 0.0f };
			NE::Math::Vec3 right{ 1.0f, 0.0f, 0.0f };
			NE::Math::Vec3 up{ 0.0f, 1.0f, 0.0f };
			NE::Math::Vec3 color{ 1.0f, 1.0f, 1.0f };
			float intensity = 0.0f;
			float range = 0.0f;
			float innerCos = 0.0f;
			float outerCos = 0.0f;
			float halfWidth = 0.0f;
			float halfHeight = 0.0f;
			bool castsBakedShadow = false;
		};

		struct PreparedBakeInput {
			DirectLightBakeSettings settings{};
			std::vector<LightmapAtlasPage> pages;
			std::vector<LightmapBakeReceiverSnapshot> receivers;
			LightmapBakeReceiverCollectionStats receiverCollectionStats{};
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
			float desiredPreviewExposure = 1.0f;
			std::shared_ptr<DirectLightBakeResult> pendingCpuResult;
			std::shared_ptr<DirectLightBakeResult> publishedResult;
		};

		WorkerControl& Control() {
			static WorkerControl control;
			return control;
		}

		uint64_t HashBytes(const void* data, size_t size) {
			const auto* bytes = static_cast<const uint8_t*>(data);
			uint64_t hash = 1469598103934665603ull;
			for (size_t i = 0; i < size; ++i) {
				hash ^= bytes[i];
				hash *= 1099511628211ull;
			}
			return hash;
		}

		std::string ToHexString(uint64_t value) {
			std::ostringstream stream;
			stream << std::hex << value;
			return stream.str();
		}

		std::string BuildStableLightmapRevisionId(const Assets::LightmapAssetBlob& blob) {
			Assets::LightmapAssetBlob signatureBlob = blob;
			signatureBlob.lightingRevisionId.clear();
			signatureBlob.dependencySignature.clear();

			std::vector<uint8_t> bytes;
			NE::Serialization::ToBinary(bytes, signatureBlob);
			return ToHexString(HashBytes(bytes.data(), bytes.size()));
		}

		std::vector<uint8_t> BuildDilationWriteMaskForPage(
			const DirectLightBakePageBuffers& page,
			const LightmapUvRasterResult& rasterResult) {
			const size_t texelCount = static_cast<size_t>(page.width) * static_cast<size_t>(page.height);
			std::vector<uint8_t> writeMask(texelCount, 0u);

			for (const auto& receiver : rasterResult.receivers) {
				if (receiver.pageIndex != page.pageIndex) {
					continue;
				}

				const int startX = std::max(receiver.placement.outerX, 0);
				const int startY = std::max(receiver.placement.outerY, 0);
				const int endX = std::min(
					receiver.placement.outerX + receiver.placement.outerWidth,
					static_cast<int>(page.width));
				const int endY = std::min(
					receiver.placement.outerY + receiver.placement.outerHeight,
					static_cast<int>(page.height));

				for (int y = startY; y < endY; ++y) {
					for (int x = startX; x < endX; ++x) {
						const size_t linearIndex = static_cast<size_t>(y) * static_cast<size_t>(page.width) + static_cast<size_t>(x);
						if (linearIndex < writeMask.size()) {
							writeMask[linearIndex] = 1u;
						}
					}
				}
			}

			return writeMask;
		}

		bool BuildCanonicalLightmapAssetBlob(
			const DirectLightBakeResult& result,
			const Assets::UUID& assetUuid,
			Assets::LightmapAssetBlob& outBlob,
			std::string& outErrorMessage) {
			outBlob = {};
			outErrorMessage.clear();

			if (assetUuid.empty()) {
				outErrorMessage = "missing lightmap asset UUID";
				return false;
			}

			if (!result.rasterResult) {
				outErrorMessage = "published bake result is missing raster metadata";
				return false;
			}

			const auto& allocationState = GetLightmapAllocationPreviewState();
			if (!allocationState.hasRun) {
				outErrorMessage = "lightmap allocation must be available before publishing";
				return false;
			}

			outBlob.formatVersionMajor = 1;
			outBlob.formatVersionMinor = 0;
			outBlob.lightmapAssetId = assetUuid;
			outBlob.bakeSettings.workerCount = result.settings.workerCount;
			outBlob.bakeSettings.dilationRadiusTexels = result.settings.dilationRadiusTexels;
			outBlob.bakeSettings.rebuildBvhBeforeBake = result.settings.rebuildBvhBeforeBake;
			outBlob.bakeSettings.generateDebugBuffers = result.settings.generateDebugBuffers;
			outBlob.bakeSettings.rayOriginBias = result.settings.rayOriginBias;
			outBlob.bakeSettings.rayMinDistance = result.settings.rayMinDistance;
			outBlob.bakeSettings.finiteLightDistanceEpsilon = result.settings.finiteLightDistanceEpsilon;
			outBlob.bakeSettings.previewExposure = result.settings.previewExposure;
			outBlob.bakeSettings.texelsPerUnit = allocationState.settings.texelsPerUnit;
			outBlob.bakeSettings.pageSize = static_cast<uint32_t>(std::max(allocationState.settings.pageSize, 0));
			outBlob.bakeSettings.padding = static_cast<uint32_t>(std::max(allocationState.settings.padding, 0));

			outBlob.bindings.reserve(result.rasterResult->receivers.size());
			for (const auto& receiver : result.rasterResult->receivers) {
				NE::Lighting::LightmapBindingRecord binding{};
				binding.entityLuid = receiver.stableId;
				binding.subMeshIndex = receiver.subMeshIndex;
				binding.pageId = receiver.placement.pageId;
				binding.uvScale = receiver.placement.uvScale;
				binding.uvOffset = receiver.placement.uvOffset;
				outBlob.bindings.push_back(std::move(binding));
			}

			outBlob.pages.reserve(result.pages.size());
			for (const auto& page : result.pages) {
				const uint64_t texelCount = static_cast<uint64_t>(page.width) * static_cast<uint64_t>(page.height);
				if (page.pageId.empty() ||
					page.width == 0u ||
					page.height == 0u ||
					page.lighting.size() != static_cast<size_t>(texelCount) ||
					page.validMask.size() != static_cast<size_t>(texelCount)) {
					outErrorMessage = "published bake contains invalid page buffers";
					return false;
				}

				Assets::LightmapAssetPage assetPage{};
				assetPage.pageIndex = page.pageIndex;
				assetPage.pageId = page.pageId;
				assetPage.width = page.width;
				assetPage.height = page.height;
				assetPage.validTexelCount = static_cast<uint64_t>(page.validTexelCount);
				assetPage.lighting = page.lighting;
				assetPage.validMask = page.validMask;
				assetPage.dilationWriteMask = BuildDilationWriteMaskForPage(page, *result.rasterResult);

				for (const auto& rasterPage : result.rasterResult->pageBuffers) {
					if (rasterPage.pageIndex == page.pageIndex) {
						assetPage.allocatedInnerTexelCount = static_cast<uint64_t>(rasterPage.allocatedInnerTexelCount);
						assetPage.coverage01 = rasterPage.coverage01;
						break;
					}
				}

				outBlob.pages.push_back(std::move(assetPage));
			}

			std::sort(outBlob.bindings.begin(), outBlob.bindings.end(),
				[](const NE::Lighting::LightmapBindingRecord& lhs, const NE::Lighting::LightmapBindingRecord& rhs) {
					if (lhs.entityLuid != rhs.entityLuid) {
						return lhs.entityLuid < rhs.entityLuid;
					}
					return lhs.subMeshIndex < rhs.subMeshIndex;
				});

			std::sort(outBlob.pages.begin(), outBlob.pages.end(),
				[](const Assets::LightmapAssetPage& lhs, const Assets::LightmapAssetPage& rhs) {
					return lhs.pageIndex < rhs.pageIndex;
				});

			const std::string signature = BuildStableLightmapRevisionId(outBlob);
			outBlob.lightingRevisionId = signature;
			outBlob.dependencySignature = signature;
			return true;
		}

		std::string BuildSuggestedLightmapAssetPathInternal() {
			if (EditorScene::s_currentScenePath.empty()) {
				return {};
			}

			std::filesystem::path scenePath(EditorScene::s_currentScenePath);
			if (scenePath.empty()) {
				return {};
			}

			const std::filesystem::path directory = scenePath.parent_path();
			const std::string fileName = scenePath.stem().string() + "_Lightmap.nlight";
			return (directory / fileName).string();
		}

		std::vector<NE::SceneManagement::LightmapRuntimePreviewPageInput> BuildRuntimePreviewPages(
			const LightmapBakeTextureOutput& textureOutput) {
			std::vector<NE::SceneManagement::LightmapRuntimePreviewPageInput> pages;
			pages.reserve(textureOutput.pages.size());

			for (const auto& page : textureOutput.pages) {
				if (page.preview.hdrTexture == 0u) {
					continue;
				}

				NE::SceneManagement::LightmapRuntimePreviewPageInput runtimePage{};
				runtimePage.pageId = page.descriptor.pageId;
				runtimePage.width = page.descriptor.width;
				runtimePage.height = page.descriptor.height;
				runtimePage.textureId = page.preview.hdrTexture;
				pages.push_back(std::move(runtimePage));
			}

			return pages;
		}

		bool IsFiniteFloat(float value) {
			return std::isfinite(value);
		}

		bool IsFiniteVec3(const NE::Math::Vec3& value) {
			return IsFiniteFloat(value.x) && IsFiniteFloat(value.y) && IsFiniteFloat(value.z);
		}

		NE::Math::Vec3 SafeNormalize(const NE::Math::Vec3& value, const NE::Math::Vec3& fallback) {
			if (!IsFiniteVec3(value) || value.LengthSquared() <= kFiniteEpsilon) {
				return fallback;
			}
			return value.Normalized();
		}

		NE::Math::Vec3 BuildStablePerpendicular(const NE::Math::Vec3& normal) {
			NE::Math::Vec3 reference = (std::abs(normal.y) < 0.99f)
				? NE::Math::Vec3{ 0.0f, 1.0f, 0.0f }
				: NE::Math::Vec3{ 1.0f, 0.0f, 0.0f };
			NE::Math::Vec3 tangent = reference - normal * reference.Dot(normal);
			if (!IsFiniteVec3(tangent) || tangent.LengthSquared() <= kFiniteEpsilon) {
				reference = { 0.0f, 0.0f, 1.0f };
				tangent = reference - normal * reference.Dot(normal);
			}
			return SafeNormalize(tangent, { 1.0f, 0.0f, 0.0f });
		}

		void BuildOrthonormalAreaFrame(
			const NE::Math::Vec3& rawRight,
			const NE::Math::Vec3& rawUp,
			const NE::Math::Vec3& fallbackDirection,
			NE::Math::Vec3& outRight,
			NE::Math::Vec3& outUp,
			NE::Math::Vec3& outDirection) {
			NE::Math::Vec3 direction = -rawRight.Cross(rawUp);
			if (!IsFiniteVec3(direction) || direction.LengthSquared() <= kFiniteEpsilon) {
				direction = fallbackDirection;
			}
			outDirection = SafeNormalize(direction, { 0.0f, 0.0f, -1.0f });

			NE::Math::Vec3 tangentSeed = rawRight;
			NE::Math::Vec3 tangent = tangentSeed - outDirection * tangentSeed.Dot(outDirection);
			if (!IsFiniteVec3(tangent) || tangent.LengthSquared() <= kFiniteEpsilon) {
				tangentSeed = rawUp;
				tangent = tangentSeed - outDirection * tangentSeed.Dot(outDirection);
			}
			if (!IsFiniteVec3(tangent) || tangent.LengthSquared() <= kFiniteEpsilon) {
				tangent = BuildStablePerpendicular(outDirection);
			}

			outRight = SafeNormalize(tangent, BuildStablePerpendicular(outDirection));
			outUp = SafeNormalize(outRight.Cross(outDirection), { 0.0f, 1.0f, 0.0f });
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

		std::string CategorizeBakeWarning(const std::string& warning) {
			// Temporary v1 panel surfacing: warnings are grouped by message
			// substring until the bake pipeline grows structured warning codes.
			if (warning.find("invalid page index") != std::string::npos) {
				return "Invalid atlas page index";
			}
			if (warning.find("duplicate page indices") != std::string::npos) {
				return "Duplicate atlas page indices";
			}
			if (warning.find("required components are missing") != std::string::npos) {
				return "Missing required receiver components";
			}
			if (warning.find("allocated inner rect is empty") != std::string::npos) {
				return "Empty inner rect";
			}
			if (warning.find("allocated inner rect lies outside its page") != std::string::npos) {
				return "Inner rect outside page";
			}
			if (warning.find("skipped light because the world transform is non-finite") != std::string::npos) {
				return "Non-finite light transform";
			}
			if (warning.find("world transform is non-finite") != std::string::npos) {
				return "Non-finite world transform";
			}
			if (warning.find("spot light because its cone angles are non-finite") != std::string::npos) {
				return "Non-finite spot cone angles";
			}
			if (warning.find("spot light because inner cone angle exceeds outer cone angle") != std::string::npos) {
				return "Invalid spot cone ordering";
			}
			if (warning.find("skipped light because its range is invalid") != std::string::npos) {
				return "Invalid finite light range";
			}
			if (warning.find("area light because its size is invalid") != std::string::npos) {
				return "Invalid area light size";
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
			if (warning.find("UV1 triangle data is unavailable") != std::string::npos) {
				return "Missing UV1 triangle data";
			}
			if (warning.find("no valid world-space UV1 triangles remained after validation") != std::string::npos) {
				return "No valid UV1 triangles after validation";
			}
			if (warning.find("page buffer could not be resolved") != std::string::npos) {
				return "Missing raster page buffer";
			}
			return "Other bake warning";
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
				if (light.shadowUpdateMode == NE::ECS::Component::Light::ShadowUpdateMode::Realtime) {
					continue;
				}
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
				const NE::Math::Vec3 rawRight = transform.worldMatrix.Right();
				const NE::Math::Vec3 rawUp = transform.worldMatrix.Up();
				const NE::Math::Vec3 rawForward = transform.worldMatrix.Forward();
				snapshot.right = SafeNormalize(rawRight, { 1.0f, 0.0f, 0.0f });
				snapshot.up = SafeNormalize(rawUp, { 0.0f, 1.0f, 0.0f });
				snapshot.direction = SafeNormalize(light.direction, SafeNormalize(rawForward, { 0.0f, -1.0f, 0.0f }));
				snapshot.color = light.color;
				snapshot.castsBakedShadow =
					light.shadowUpdateMode == NE::ECS::Component::Light::ShadowUpdateMode::StaticBake &&
					light.shadowType != NE::ECS::Component::Light::ShadowType::None;

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
					} else if constexpr (std::is_same_v<T, NE::ECS::Component::Light::AreaLightData>) {
						snapshot.kind = BakeLightKind::Area;
						snapshot.range = lightData.range;
						snapshot.halfWidth = 0.5f * lightData.width;
						snapshot.halfHeight = 0.5f * lightData.height;
					} else {
						supported = false;
					}
				}, light.data);

				if (!supported) {
					continue;
				}

				if (snapshot.kind == BakeLightKind::Area) {
					BuildOrthonormalAreaFrame(rawRight, rawUp, snapshot.direction, snapshot.right, snapshot.up, snapshot.direction);
				}

				if (!IsFiniteVec3(snapshot.color) || snapshot.intensity <= 0.0f || !std::isfinite(snapshot.intensity)) {
					continue;
				}

				if ((snapshot.kind == BakeLightKind::Point || snapshot.kind == BakeLightKind::Spot || snapshot.kind == BakeLightKind::Area) &&
					(!std::isfinite(snapshot.range) || snapshot.range <= 0.0f)) {
					PushWarning(warnings, snapshot.entityName + ": skipped light because its range is invalid for baking.");
					continue;
				}
				if (snapshot.kind == BakeLightKind::Area &&
					(!std::isfinite(snapshot.halfWidth) || !std::isfinite(snapshot.halfHeight) ||
					 snapshot.halfWidth <= 0.0f || snapshot.halfHeight <= 0.0f)) {
					PushWarning(warnings, snapshot.entityName + ": skipped area light because its size is invalid for baking.");
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

			outInput.receivers = CollectLightmapBakeReceiverSnapshots(placements, outInput.pages, outInput.warnings, &outInput.receiverCollectionStats);
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

		float RectDistanceAttenuation(float distanceToCenter, float range, float rectExtent) {
			if (range <= 0.0f) {
				return 0.0f;
			}

			if (distanceToCenter > range + rectExtent) {
				return 0.0f;
			}

			return DistanceAttenuation(std::max(distanceToCenter - rectExtent, 0.0f), range);
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

		void TallyWarningCounts(DirectLightBakeResult& result) {
			result.warningCounts.clear();
			for (const auto& warning : result.warnings) {
				result.warningCounts[CategorizeBakeWarning(warning)]++;
			}
		}

		bool BuildTextureOutputRequest(
			const DirectLightBakeResult& result,
			float previewExposure,
			LightmapBakeOutputBuildRequest& outRequest,
			std::string& outErrorMessage) {
			outRequest = {};
			outRequest.previewExposure = previewExposure;
			outErrorMessage.clear();

			const auto& allocationState = GetLightmapAllocationPreviewState();
			const int fallbackPadding = allocationState.hasRun
				? allocationState.settings.padding
				: kDefaultLightmapPadding;
			outRequest.resolvedDilationRadiusTexels = result.settings.dilationRadiusTexels != 0u
				? result.settings.dilationRadiusTexels
				: static_cast<uint32_t>(std::max(fallbackPadding, 0));

			std::unordered_map<int, const LightmapUvRasterPageBuffers*> rasterPagesByIndex;
			rasterPagesByIndex.reserve(result.rasterResult ? result.rasterResult->pageBuffers.size() : 0u);
			if (result.rasterResult) {
				if (result.rasterResult->pageBuffers.size() != result.pages.size()) {
					outErrorMessage = "Bake output stage rejected the bake result because raster page metadata no longer matches the baked page count.";
					return false;
				}

				for (const auto& rasterPage : result.rasterResult->pageBuffers) {
					rasterPagesByIndex.emplace(rasterPage.pageIndex, &rasterPage);
				}
			}

			// Preserve baked page order exactly as produced by the allocator/raster
			// result. The output stage and panel preview intentionally rely on this
			// stable ordering for page identity and future persistence.
			outRequest.pageDilationWriteMasks.resize(result.pages.size());
			outRequest.pages.reserve(result.pages.size());
			for (size_t pageSlot = 0; pageSlot < result.pages.size(); ++pageSlot) {
				const auto& page = result.pages[pageSlot];
				LightmapBakeOutputInputPage requestPage{};
				requestPage.pageIndex = page.pageIndex;
				requestPage.pageId = page.pageId;
				requestPage.width = page.width;
				requestPage.height = page.height;
				requestPage.validTexelCount = page.validTexelCount;
				requestPage.lighting = &page.lighting;
				requestPage.validMask = &page.validMask;
				requestPage.dilationWriteMask = &outRequest.pageDilationWriteMasks[pageSlot];

				const size_t texelCount = static_cast<size_t>(page.width) * static_cast<size_t>(page.height);
				auto& writeMask = outRequest.pageDilationWriteMasks[pageSlot];
				writeMask.assign(texelCount, 0u);

				if (result.rasterResult) {
					const auto rasterPageIt = rasterPagesByIndex.find(page.pageIndex);
					if (rasterPageIt == rasterPagesByIndex.end()) {
						outErrorMessage = "Bake output stage rejected the bake result because a page could not be matched back to raster metadata.";
						return false;
					}

					const auto* rasterPage = rasterPageIt->second;
					requestPage.allocatedInnerTexelCount = rasterPage->allocatedInnerTexelCount;
					requestPage.coverage01 = rasterPage->coverage01;
					requestPage.ownerPreviewRgba8 = &rasterPage->preview.ownerRgba8;
					std::vector<uint8_t> overlapMask(texelCount, 0u);
					size_t overlapTexelCount = 0u;

					for (const auto& receiver : result.rasterResult->receivers) {
						if (receiver.pageIndex != page.pageIndex) {
							continue;
						}

						const int startX = std::max(receiver.placement.outerX, 0);
						const int startY = std::max(receiver.placement.outerY, 0);
						const int endX = std::min(receiver.placement.outerX + receiver.placement.outerWidth, static_cast<int>(page.width));
						const int endY = std::min(receiver.placement.outerY + receiver.placement.outerHeight, static_cast<int>(page.height));
						for (int y = startY; y < endY; ++y) {
							for (int x = startX; x < endX; ++x) {
								const size_t linearIndex = static_cast<size_t>(y) * static_cast<size_t>(page.width) + static_cast<size_t>(x);
								if (linearIndex < writeMask.size()) {
									if (writeMask[linearIndex] != 0u && overlapMask[linearIndex] == 0u) {
										overlapMask[linearIndex] = 1u;
										++overlapTexelCount;
									}
									writeMask[linearIndex] = 1u;
								}
							}
						}
					}

					if (overlapTexelCount > 0u) {
						outRequest.prebuildWarnings.push_back(
							page.pageId + ": dilation write-mask outer rects overlapped on " +
							std::to_string(overlapTexelCount) +
							" texels; allocator metadata should keep receiver padding regions disjoint.");
					}
				} else if (page.width > 0u && page.height > 0u) {
					requestPage.coverage01 =
						static_cast<float>(page.validTexelCount) / static_cast<float>(page.width * page.height);
					std::fill(writeMask.begin(), writeMask.end(), static_cast<uint8_t>(1u));
				}

				outRequest.pages.push_back(requestPage);
			}

			return true;
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
			result.stats.areaLightCount = std::count_if(input.lights.begin(), input.lights.end(),
				[](const BakeLightSnapshot& light) { return light.kind == BakeLightKind::Area; });
			result.stats.collectedDiscardedTriangleCount = input.receiverCollectionStats.discardedTriangleCount;
			result.stats.collectedOutOfRangeIndexTriangleCount = input.receiverCollectionStats.outOfRangeIndexTriangleCount;
			result.stats.collectedNonFiniteUvTriangleCount = input.receiverCollectionStats.nonFiniteUvTriangleCount;
			result.stats.collectedNonFiniteWorldPositionTriangleCount = input.receiverCollectionStats.nonFiniteWorldPositionTriangleCount;
			result.stats.collectedDegenerateWorldTriangleCount = input.receiverCollectionStats.degenerateWorldTriangleCount;
			result.stats.receiversWithCollectedDiscardedTriangles = input.receiverCollectionStats.receiversWithDiscardedTriangles;
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
			result.stats.rasterSameReceiverConflictCount = rasterResult->stats.sameReceiverOwnershipConflictCount;
			result.stats.rasterCrossReceiverConflictCount = rasterResult->stats.crossReceiverOwnershipConflictCount;
			result.stats.rasterInvalidBarycentricTexelCount = rasterResult->stats.invalidBarycentricTexelCount;
			result.stats.rasterOutOfRangeTexelCount = rasterResult->stats.outOfRangeTexelCount;
			result.stats.rasterInvalidSampleTexelCount = rasterResult->stats.invalidSampleTexelCount;
			result.stats.rasterInnerRectClampedTriangleCount = rasterResult->stats.innerRectClampedTriangleCount;
			result.stats.coveredTexelCount = rasterResult->stats.coveredTexelCount;
			result.stats.skippedTexelCount =
				rasterResult->stats.invalidBarycentricTexelCount +
				rasterResult->stats.outOfRangeTexelCount +
				rasterResult->stats.invalidSampleTexelCount;
			result.rasterResult = rasterResult;
			result.warnings.insert(result.warnings.end(), rasterResult->warnings.begin(), rasterResult->warnings.end());

			result.pages.reserve(input.pages.size());
			std::unordered_map<int, size_t> pageSlotsByIndex;
			std::unordered_map<int, const LightmapUvRasterPageBuffers*> rasterPagesByIndex;
			pageSlotsByIndex.reserve(input.pages.size());
			rasterPagesByIndex.reserve(rasterResult->pageBuffers.size());
			for (const auto& rasterPage : rasterResult->pageBuffers) {
				rasterPagesByIndex.emplace(rasterPage.pageIndex, &rasterPage);
			}
			for (size_t pageIndex = 0; pageIndex < input.pages.size(); ++pageIndex) {
				const auto& page = input.pages[pageIndex];
				DirectLightBakePageBuffers pageBuffers{};
				pageBuffers.pageIndex = page.pageIndex;
				pageBuffers.pageId = page.pageId;
				pageBuffers.width = static_cast<uint32_t>(std::max(page.width, 0));
				pageBuffers.height = static_cast<uint32_t>(std::max(page.height, 0));
				const size_t texelCount = static_cast<size_t>(pageBuffers.width) * static_cast<size_t>(pageBuffers.height);
				pageBuffers.lighting.assign(texelCount, { 0.0f, 0.0f, 0.0f });
				const auto rasterPageIt = rasterPagesByIndex.find(page.pageIndex);
				if (rasterPageIt != rasterPagesByIndex.end()) {
					pageBuffers.validMask = rasterPageIt->second->validMask;
					pageBuffers.validTexelCount = rasterPageIt->second->validTexelCount;
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

			auto processReceiver = [&](size_t receiverIndex, DirectLightBakeStats& localStats) -> bool {
				const auto& receiver = input.receivers[receiverIndex];
				const auto pageSlotIt = pageSlotsByIndex.find(receiver.pageIndex);
				const auto rasterPageIt = rasterPagesByIndex.find(receiver.pageIndex);
				if (pageSlotIt == pageSlotsByIndex.end() || rasterPageIt == rasterPagesByIndex.end()) {
					++localStats.skippedTexelCount;
					return true;
				}

				auto& page = result.pages[pageSlotIt->second];
				const auto& rasterPage = *rasterPageIt->second;
				const int minX = receiver.placement.innerX;
				const int minY = receiver.placement.innerY;
				const int maxX = receiver.placement.innerX + receiver.placement.innerWidth - 1;
				const int maxY = receiver.placement.innerY + receiver.placement.innerHeight - 1;

				for (int y = minY; y <= maxY; ++y) {
					for (int x = minX; x <= maxX; ++x) {
						if (control.cancelRequested.load()) {
							return false;
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
							bool handledByAreaLight = false;

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
							case BakeLightKind::Area: {
								const NE::Math::Vec3 emitterNormal = SafeNormalize(light.direction, { 0.0f, 0.0f, -1.0f });
								const NE::Math::Vec3 toReceiverFromCenter = sample.worldPosition - light.position;
								if (emitterNormal.Dot(toReceiverFromCenter) <= 0.0f) {
									handledByAreaLight = true;
									break;
								}

								const float centerDistance = toReceiverFromCenter.Length();
								if (!std::isfinite(centerDistance) || centerDistance <= kFiniteEpsilon) {
									handledByAreaLight = true;
									break;
								}

								const float rectExtent = std::sqrt(light.halfWidth * light.halfWidth + light.halfHeight * light.halfHeight);
								const float rectAttenuation = RectDistanceAttenuation(centerDistance, light.range, rectExtent);
								if (rectAttenuation <= 0.0f) {
									handledByAreaLight = true;
									break;
								}

								const NE::Math::Vec3 lightRight = SafeNormalize(light.right, { 1.0f, 0.0f, 0.0f });
								const NE::Math::Vec3 lightUp = SafeNormalize(light.up, { 0.0f, 1.0f, 0.0f });
								NE::Math::Vec3 sampleAccumulated{ 0.0f, 0.0f, 0.0f };
								const uint64_t sampleSeedBase =
									(static_cast<uint64_t>(linearIndex) * 1315423911ull) ^
									(light.stableId * 2654435761ull);
								for (int areaSampleIndex = 0; areaSampleIndex < kAreaLightBakeSamples; ++areaSampleIndex) {
									const uint64_t sampleSeed = sampleSeedBase + static_cast<uint64_t>(areaSampleIndex);
									const float jitterX = (static_cast<float>((sampleSeed * 48271ull) % 65521ull) + 0.5f) / 65521.0f;
									const float jitterY = (static_cast<float>((sampleSeed * 69621ull + 17ull) % 65521ull) + 0.5f) / 65521.0f;
									const int gridX = areaSampleIndex & 3;
									const int gridY = areaSampleIndex >> 2;
									const float u = (static_cast<float>(gridX) + jitterX) * 0.25f - 0.5f;
									const float v = (static_cast<float>(gridY) + jitterY) * 0.25f - 0.5f;

									const NE::Math::Vec3 emitterPoint =
										light.position +
										lightRight * (u * light.halfWidth * 2.0f) +
										lightUp * (v * light.halfHeight * 2.0f);

									const NE::Math::Vec3 lightVector = emitterPoint - sample.worldPosition;
									const float sampleDistance = lightVector.Length();
									if (!std::isfinite(sampleDistance) || sampleDistance <= kFiniteEpsilon) {
										continue;
									}

									const NE::Math::Vec3 sampleLightDirection = lightVector / sampleDistance;
									const float nDotL = sample.shadingNormal.Dot(sampleLightDirection);
									if (!std::isfinite(nDotL) || nDotL <= 0.0f) {
										continue;
									}

									const float lightFacing = emitterNormal.Dot(-sampleLightDirection);
									if (!std::isfinite(lightFacing) || lightFacing <= 0.0f) {
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
										ray.direction = sampleLightDirection;
										ray.tMin = input.settings.rayMinDistance;
										ray.tMax = std::max(sampleDistance - input.settings.finiteLightDistanceEpsilon, ray.tMin);

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

									sampleAccumulated += light.color * (light.intensity * rectAttenuation * lightFacing * nDotL);
								}

								accumulated += sampleAccumulated / static_cast<float>(kAreaLightBakeSamples);
								handledByAreaLight = true;
								break;
							}
							}

							if (handledByAreaLight) {
								continue;
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
				return true;
			};

			auto workerFn = [&]() {
				while (!control.cancelRequested.load()) {
					const size_t receiverIndex = nextReceiver.fetch_add(1u);
					if (receiverIndex >= input.receivers.size()) {
						break;
					}

					DirectLightBakeStats localStats{};
					const bool completedReceiver = processReceiver(receiverIndex, localStats);

					DirectLightBakeStats mergedStats{};
					size_t processed = processedReceivers.load();
					{
						std::scoped_lock statsLock(statsMutex);
						liveStats.skippedTexelCount += localStats.skippedTexelCount;
						liveStats.raysCast += localStats.raysCast;
						liveStats.occludedRayCount += localStats.occludedRayCount;
						liveStats.visibleRayCount += localStats.visibleRayCount;
						mergedStats = liveStats;
						if (completedReceiver) {
							processed = processedReceivers.fetch_add(1u) + 1u;
						} else {
							processed = processedReceivers.load();
						}
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

			{
				std::scoped_lock lock(control.mutex);
				const size_t completedReceivers = processedReceivers.load();
				const size_t totalReceivers = input.receivers.size();
				const float progress01 = totalReceivers > 0u
					? std::clamp(static_cast<float>(completedReceivers) / static_cast<float>(totalReceivers), 0.0f, 1.0f)
					: 1.0f;
				control.state.liveStats = result.stats;
				control.state.processedInstanceCount = completedReceivers;
				control.state.queuedInstanceCount = totalReceivers;
				control.state.progress01 = progress01;
				control.state.cancelRequested = control.cancelRequested.load();
				control.pendingCpuResult.reset();

				if (control.cancelRequested.load()) {
					control.state.isRunning = false;
					control.state.lastBakeSucceeded = false;
					control.state.activeStage = "Cancelled";
					control.state.statusMessage = "Direct light bake cancelled.";
				} else {
					control.pendingCpuResult = std::make_shared<DirectLightBakeResult>(std::move(result));
					control.state.isRunning = true;
					control.state.lastBakeSucceeded = false;
					control.state.activeStage = "Preparing Output";
					control.state.statusMessage = "Direct light bake finished on the CPU. Creating HDR page textures...";
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

		std::shared_ptr<DirectLightBakeResult> pendingCpuResult;
		std::shared_ptr<DirectLightBakeResult> publishedResult;
		float desiredPreviewExposure = 1.0f;
		{
			std::scoped_lock lock(control.mutex);
			pendingCpuResult = control.pendingCpuResult;
			publishedResult = control.publishedResult;
			desiredPreviewExposure = control.desiredPreviewExposure;
		}

		if (pendingCpuResult) {
			LightmapBakeOutputBuildRequest outputRequest{};
			std::string outputErrorMessage;
			if (!BuildTextureOutputRequest(*pendingCpuResult, desiredPreviewExposure, outputRequest, outputErrorMessage) ||
				!BuildLightmapBakeTextureOutput(outputRequest, pendingCpuResult->textureOutput, pendingCpuResult->warnings, outputErrorMessage)) {
				std::scoped_lock lock(control.mutex);
				control.pendingCpuResult.reset();
				control.state.isRunning = false;
				control.state.cancelRequested = false;
				control.state.lastBakeSucceeded = false;
				control.state.activeStage = "Output Failed";
				control.state.statusMessage =
					outputErrorMessage.empty()
					? "Direct light bake output creation failed."
					: ("Direct light bake output creation failed: " + outputErrorMessage);
				return;
			}

			TallyWarningCounts(*pendingCpuResult);
			const uint64_t previousRevision = publishedResult ? publishedResult->revision : 0u;
			pendingCpuResult->revision = previousRevision + 1u;
			pendingCpuResult->textureOutput.sourceBakeRevision = pendingCpuResult->revision;
			NE::SceneManagement::SetSceneLightmapPreviewState(
				NE::GetScene(),
				BuildRuntimePreviewPages(pendingCpuResult->textureOutput));

			std::scoped_lock lock(control.mutex);
			// Publication is atomic from the editor's point of view. The previous
			// published result stays alive through shared ownership until this swap.
			control.pendingCpuResult.reset();
			control.publishedResult = pendingCpuResult;
			control.state.result = pendingCpuResult;
			control.state.hasResult = true;
			control.state.isRunning = false;
			control.state.cancelRequested = false;
			control.state.lastBakeSucceeded = true;
			control.state.activeStage = "Complete";
			control.state.statusMessage = "Direct light bake completed.";
			control.state.settings.previewExposure = desiredPreviewExposure;
			return;
		}

		if (publishedResult &&
			std::fabs(publishedResult->textureOutput.previewExposure - desiredPreviewExposure) > 1e-4f) {
			std::string refreshErrorMessage;
			if (!RefreshLightmapBakeDisplayPreviews(publishedResult->textureOutput, desiredPreviewExposure, refreshErrorMessage)) {
				std::scoped_lock lock(control.mutex);
				control.state.activeStage = "Preview Refresh Failed";
				control.state.statusMessage =
					refreshErrorMessage.empty()
					? "Failed to refresh baked light preview textures."
					: ("Failed to refresh baked light preview textures: " + refreshErrorMessage);
				return;
			}

			std::scoped_lock lock(control.mutex);
			control.state.activeStage = "Complete";
			control.state.statusMessage = "Direct light bake completed.";
			control.state.settings.previewExposure = desiredPreviewExposure;
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
			control.pendingCpuResult.reset();
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
			control.desiredPreviewExposure = settings.previewExposure;
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

	void SetDirectLightBakePreviewExposure(float previewExposure) {
		auto& control = Control();
		std::scoped_lock lock(control.mutex);
		control.desiredPreviewExposure = std::clamp(previewExposure, 0.1f, 16.0f);
		control.state.settings.previewExposure = control.desiredPreviewExposure;
	}

	std::string BuildSuggestedLightmapAssetPath() {
		return BuildSuggestedLightmapAssetPathInternal();
	}

	bool CommitPublishedLightmapAsset(std::string& outAssetPath, std::string& outErrorMessage) {
		outAssetPath.clear();
		outErrorMessage.clear();

		if (EditorScene::s_currentScenePath.empty()) {
			outErrorMessage = "Save the scene before committing a baked lightmap asset.";
			return false;
		}

		std::shared_ptr<DirectLightBakeResult> publishedResult;
		{
			auto& control = Control();
			std::scoped_lock lock(control.mutex);
			publishedResult = control.publishedResult;
		}

		if (!publishedResult) {
			outErrorMessage = "Run and complete a direct bake before committing a lightmap asset.";
			return false;
		}

		const std::string assetPath = BuildSuggestedLightmapAssetPathInternal();
		if (assetPath.empty()) {
			outErrorMessage = "Unable to determine a target path for the baked lightmap asset.";
			return false;
		}

		auto& assetManager = Assets::AssetManager::GetInstance();
		Assets::UUID assetUuid = assetManager.RetrieveUUID(assetPath);
		if (assetUuid.empty()) {
			assetUuid = Assets::GenerateUUID();
		}

		Assets::LightmapAssetBlob blob{};
		if (!BuildCanonicalLightmapAssetBlob(*publishedResult, assetUuid, blob, outErrorMessage)) {
			return false;
		}

		if (!Assets::LightmapAsset::SaveBlob(assetPath, blob, outErrorMessage)) {
			return false;
		}

		const std::filesystem::path metaPath = std::filesystem::path(assetPath).concat(".meta");
		if (std::filesystem::exists(metaPath)) {
			assetManager.ReimportAsset(assetPath);
		} else {
			assetManager.GenerateMetadata(assetPath, assetUuid);
		}

		auto& scene = NE::GetScene();
		auto& lightingContainer = scene.GetLightingContainer();
		lightingContainer.enabled = true;
		lightingContainer.lightingAssetRef = assetUuid;
		lightingContainer.lightingRevisionId = blob.lightingRevisionId;
		lightingContainer.dependencySignature = blob.dependencySignature;
		lightingContainer.resolvedAsset.reset();
		lightingContainer.pageIdToSlot.clear();
		lightingContainer.statusMessage.clear();

		NE::Resource::ResourceManager::GetInstance().UnloadResource(assetUuid);
		NE::SceneManagement::ResolveSceneLightmapRuntimeState(scene);

		EditorScene::isDirty = true;
		outAssetPath = assetPath;
		return true;
	}

	void ShutdownDirectLightBakeSession() {
		CancelSceneDirectLightBake();
		auto& control = Control();
		if (control.workerThread.joinable()) {
			control.workerThread.join();
			control.workerFinished.store(false);
		}
		NE::SceneManagement::ClearSceneLightmapPreviewState(NE::GetScene());
		std::scoped_lock lock(control.mutex);
		control.pendingCpuResult.reset();
		control.publishedResult.reset();
		control.state.result.reset();
		control.state.hasResult = false;
		control.state.isRunning = false;
	}
}
