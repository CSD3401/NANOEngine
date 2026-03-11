#include "pch.h"
#include "LightmapAtlasAllocator.hpp"

#include "../EditorScene.hpp"

#include <Core/SpdLogger.hpp>
#include <ECS/Components/LightmapBinding.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <Graphics/Core/Model.hpp>
#include <Graphics/Core/Vertex.hpp>
#include <Math/Vec3.hpp>
#include <ResourceManagement/ResourceManager.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace Editor::Lighting {
    namespace {
        struct UvBounds {
            NE::Math::Vec2 min = { 0.0f, 0.0f };
            NE::Math::Vec2 max = { 0.0f, 0.0f };
            bool valid = false;
        };

        struct MeshMetrics {
            float worldSurfaceArea = 0.0f;
            float uvCoverage = 0.0f;
            float uvAspectRatio = 1.0f;
        };

        struct FreeRect {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
        };

        struct AtlasPageState {
            uint32_t pageIndex = 0;
            std::vector<FreeRect> freeRects;
            std::vector<LightmapPlacement> placements;
        };

        bool IsFiniteMatrix(const NE::Math::Mat4& matrix) {
            for (float value : matrix.a) {
                if (!std::isfinite(value)) {
                    return false;
                }
            }

            return true;
        }

        bool IsFiniteVec2(const NE::Math::Vec2& value) {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        NE::Math::Vec3 TransformPoint(const NE::Math::Mat4& matrix, const NE::Math::Vec3& point) {
            const NE::Math::Vec4 transformed = matrix * NE::Math::Vec4(point.x, point.y, point.z, 1.0f);
            return { transformed.x, transformed.y, transformed.z };
        }

        std::string BuildWarningDetail(LightmapFailureReason reason, const std::string& detail) {
            if (!detail.empty()) {
                return detail;
            }

            switch (reason) {
            case LightmapFailureReason::MissingModel: return "Missing or unloaded model asset.";
            case LightmapFailureReason::InvalidSubmesh: return "Renderer resolves to an invalid submesh.";
            case LightmapFailureReason::MissingUv1: return "Resolved submesh has no UV1 data.";
            case LightmapFailureReason::InvalidTransform: return "World transform contains non-finite values.";
            case LightmapFailureReason::ZeroAreaGeometry: return "Geometry produced zero world-space area.";
            case LightmapFailureReason::RectExceedsPageSize: return "Required rectangle exceeds the configured page size.";
            case LightmapFailureReason::None:
            default: return {};
            }
        }

        LightmapAllocationWarning MakeWarning(
            uint32_t entity,
            const std::string& entityName,
            LightmapFailureReason reason,
            const std::string& detail = {}) {
            return LightmapAllocationWarning{
                entity,
                entityName,
                reason,
                BuildWarningDetail(reason, detail)
            };
        }

        std::string ReasonLabel(LightmapFailureReason reason) {
            switch (reason) {
            case LightmapFailureReason::MissingModel: return "missing model";
            case LightmapFailureReason::InvalidSubmesh: return "invalid submesh";
            case LightmapFailureReason::MissingUv1: return "missing UV1";
            case LightmapFailureReason::InvalidTransform: return "invalid transform";
            case LightmapFailureReason::ZeroAreaGeometry: return "zero-area geometry";
            case LightmapFailureReason::RectExceedsPageSize: return "rect exceeds page size";
            case LightmapFailureReason::None:
            default: return "none";
            }
        }

        UvBounds ComputeUvBounds(const NE::Graphics::SubMesh& subMesh) {
            UvBounds bounds{};

            for (const auto& vertex : subMesh.vertices) {
                const NE::Math::Vec2 uv = vertex.texCoord1;
                if (!IsFiniteVec2(uv)) {
                    continue;
                }

                if (!bounds.valid) {
                    bounds.min = uv;
                    bounds.max = uv;
                    bounds.valid = true;
                    continue;
                }

                bounds.min.x = std::min(bounds.min.x, uv.x);
                bounds.min.y = std::min(bounds.min.y, uv.y);
                bounds.max.x = std::max(bounds.max.x, uv.x);
                bounds.max.y = std::max(bounds.max.y, uv.y);
            }

            return bounds;
        }

        MeshMetrics ComputeMetrics(const LightmapAllocationInput& input, const LightmapAllocatorConfig& config) {
            MeshMetrics metrics{};
            if (!input.subMesh) {
                return metrics;
            }

            const auto& subMesh = *input.subMesh;
            const UvBounds bounds = ComputeUvBounds(subMesh);

            float uvAreaSum = 0.0f;
            float worldAreaSum = 0.0f;

            for (size_t index = 0; index + 2 < subMesh.indices.size(); index += 3) {
                const uint32_t ia = subMesh.indices[index + 0];
                const uint32_t ib = subMesh.indices[index + 1];
                const uint32_t ic = subMesh.indices[index + 2];

                if (ia >= subMesh.vertices.size() || ib >= subMesh.vertices.size() || ic >= subMesh.vertices.size()) {
                    continue;
                }

                const auto& va = subMesh.vertices[ia];
                const auto& vb = subMesh.vertices[ib];
                const auto& vc = subMesh.vertices[ic];

                const NE::Math::Vec3 p0 = TransformPoint(input.worldMatrix, va.position);
                const NE::Math::Vec3 p1 = TransformPoint(input.worldMatrix, vb.position);
                const NE::Math::Vec3 p2 = TransformPoint(input.worldMatrix, vc.position);

                const NE::Math::Vec3 worldEdgeA = p1 - p0;
                const NE::Math::Vec3 worldEdgeB = p2 - p0;
                worldAreaSum += 0.5f * worldEdgeA.Cross(worldEdgeB).Length();

                const NE::Math::Vec2 uv0 = va.texCoord1;
                const NE::Math::Vec2 uv1 = vb.texCoord1;
                const NE::Math::Vec2 uv2 = vc.texCoord1;
                if (IsFiniteVec2(uv0) && IsFiniteVec2(uv1) && IsFiniteVec2(uv2)) {
                    const NE::Math::Vec2 uvEdgeA = uv1 - uv0;
                    const NE::Math::Vec2 uvEdgeB = uv2 - uv0;
                    uvAreaSum += 0.5f * std::fabs(uvEdgeA.Cross(uvEdgeB));
                }
            }

            metrics.worldSurfaceArea = worldAreaSum;
            metrics.uvCoverage = std::clamp(uvAreaSum, config.minUvCoverage, 1.0f);

            if (bounds.valid) {
                const float width = std::max(bounds.max.x - bounds.min.x, 0.0f);
                const float height = std::max(bounds.max.y - bounds.min.y, 0.0f);
                if (width > 1e-5f && height > 1e-5f) {
                    metrics.uvAspectRatio = std::clamp(width / height, 1.0f / config.maxAspectRatio, config.maxAspectRatio);
                }
            }

            return metrics;
        }

        LightmapRectRequest BuildRequest(const LightmapAllocationInput& input, const LightmapAllocatorConfig& config) {
            const MeshMetrics metrics = ComputeMetrics(input, config);

            LightmapRectRequest request{};
            request.entity = input.entity;
            request.entityName = input.entityName;
            request.worldSurfaceArea = metrics.worldSurfaceArea;
            request.uvCoverage = metrics.uvCoverage;
            request.uvAspectRatio = metrics.uvAspectRatio;

            const double density = static_cast<double>(config.texelsPerUnit) * static_cast<double>(config.texelsPerUnit);
            const double requiredArea = std::ceil(
                static_cast<double>(metrics.worldSurfaceArea) * density /
                std::max(static_cast<double>(metrics.uvCoverage), static_cast<double>(config.minUvCoverage)));

            request.requiredPixelArea = static_cast<uint64_t>(std::max(requiredArea, 1.0));

            const double aspect = std::max(static_cast<double>(request.uvAspectRatio), 1.0 / static_cast<double>(config.maxAspectRatio));
            const double innerHeight = std::sqrt(static_cast<double>(request.requiredPixelArea) / aspect);
            const double innerWidth = innerHeight * aspect;

            request.innerWidth = static_cast<uint32_t>(std::max(std::ceil(innerWidth), 1.0));
            request.innerHeight = static_cast<uint32_t>(std::max(std::ceil(innerHeight), 1.0));
            request.paddedWidth = request.innerWidth + (config.padding * 2u);
            request.paddedHeight = request.innerHeight + (config.padding * 2u);
            return request;
        }
        bool Intersects(const FreeRect& a, const FreeRect& b) {
            return !(b.x >= a.x + a.width ||
                b.x + b.width <= a.x ||
                b.y >= a.y + a.height ||
                b.y + b.height <= a.y);
        }

        bool Contains(const FreeRect& outer, const FreeRect& inner) {
            return inner.x >= outer.x &&
                inner.y >= outer.y &&
                inner.x + inner.width <= outer.x + outer.width &&
                inner.y + inner.height <= outer.y + outer.height;
        }

        void SplitFreeRect(const FreeRect& freeRect, const FreeRect& usedRect, std::vector<FreeRect>& outRects) {
            if (!Intersects(freeRect, usedRect)) {
                outRects.push_back(freeRect);
                return;
            }

            if (usedRect.x > freeRect.x) {
                outRects.push_back(FreeRect{
                    freeRect.x,
                    freeRect.y,
                    usedRect.x - freeRect.x,
                    freeRect.height
                });
            }

            if (usedRect.x + usedRect.width < freeRect.x + freeRect.width) {
                outRects.push_back(FreeRect{
                    usedRect.x + usedRect.width,
                    freeRect.y,
                    (freeRect.x + freeRect.width) - (usedRect.x + usedRect.width),
                    freeRect.height
                });
            }

            if (usedRect.y > freeRect.y) {
                outRects.push_back(FreeRect{
                    freeRect.x,
                    freeRect.y,
                    freeRect.width,
                    usedRect.y - freeRect.y
                });
            }

            if (usedRect.y + usedRect.height < freeRect.y + freeRect.height) {
                outRects.push_back(FreeRect{
                    freeRect.x,
                    usedRect.y + usedRect.height,
                    freeRect.width,
                    (freeRect.y + freeRect.height) - (usedRect.y + usedRect.height)
                });
            }
        }

        void PruneFreeRects(std::vector<FreeRect>& freeRects) {
            for (size_t i = 0; i < freeRects.size(); ++i) {
                for (size_t j = i + 1; j < freeRects.size();) {
                    if (Contains(freeRects[i], freeRects[j])) {
                        freeRects.erase(freeRects.begin() + static_cast<std::ptrdiff_t>(j));
                        continue;
                    }

                    if (Contains(freeRects[j], freeRects[i])) {
                        freeRects.erase(freeRects.begin() + static_cast<std::ptrdiff_t>(i));
                        --i;
                        break;
                    }

                    ++j;
                }
            }

            freeRects.erase(
                std::remove_if(
                    freeRects.begin(),
                    freeRects.end(),
                    [](const FreeRect& rect) {
                        return rect.width <= 0 || rect.height <= 0;
                    }),
                freeRects.end());
        }

        std::optional<FreeRect> TryPlaceRect(AtlasPageState& page, const LightmapRectRequest& request) {
            int bestShortSideFit = std::numeric_limits<int>::max();
            int bestLongSideFit = std::numeric_limits<int>::max();
            std::optional<FreeRect> bestRect;

            for (const auto& freeRect : page.freeRects) {
                if (static_cast<int>(request.paddedWidth) > freeRect.width ||
                    static_cast<int>(request.paddedHeight) > freeRect.height) {
                    continue;
                }

                const int leftoverHoriz = freeRect.width - static_cast<int>(request.paddedWidth);
                const int leftoverVert = freeRect.height - static_cast<int>(request.paddedHeight);
                const int shortSideFit = std::min(leftoverHoriz, leftoverVert);
                const int longSideFit = std::max(leftoverHoriz, leftoverVert);

                const bool isBetter = shortSideFit < bestShortSideFit ||
                    (shortSideFit == bestShortSideFit && longSideFit < bestLongSideFit) ||
                    (shortSideFit == bestShortSideFit && longSideFit == bestLongSideFit &&
                        (freeRect.y < bestRect->y || (freeRect.y == bestRect->y && freeRect.x < bestRect->x)));

                if (!bestRect.has_value() || isBetter) {
                    bestShortSideFit = shortSideFit;
                    bestLongSideFit = longSideFit;
                    bestRect = FreeRect{
                        freeRect.x,
                        freeRect.y,
                        static_cast<int>(request.paddedWidth),
                        static_cast<int>(request.paddedHeight)
                    };
                }
            }

            if (!bestRect.has_value()) {
                return std::nullopt;
            }

            std::vector<FreeRect> updatedFreeRects;
            updatedFreeRects.reserve(page.freeRects.size() * 2);
            for (const auto& freeRect : page.freeRects) {
                SplitFreeRect(freeRect, *bestRect, updatedFreeRects);
            }

            page.freeRects = std::move(updatedFreeRects);
            PruneFreeRects(page.freeRects);
            return bestRect;
        }

        std::string BuildPageId(uint32_t pageIndex) {
            std::ostringstream stream;
            stream << "lm_page_" << std::setw(3) << std::setfill('0') << pageIndex;
            return stream.str();
        }

        std::shared_ptr<NE::Graphics::Model> ResolveModel(NE::ECS::Component::Renderer& renderer) {
            if (renderer.model && !renderer.isDirty) {
                return renderer.model;
            }

            if (renderer.modelUUID.empty()) {
                renderer.model.reset();
                return nullptr;
            }

            renderer.model = NE::Resource::ResourceManager::GetInstance().LoadResource<NE::Graphics::Model>(renderer.modelUUID);
            renderer.isDirty = false;
            return renderer.model;
        }

        std::string GetEntityNameOrFallback(uint32_t entity) {
            if (!NE::ECS::Query::HasEntityMeta(entity)) {
                return "Entity " + std::to_string(entity);
            }

            const auto& meta = NE::ECS::Query::GetEntityMeta(entity);
            return meta.name.empty() ? ("Entity " + std::to_string(entity)) : meta.name;
        }

        std::vector<LightmapCollectionEntry> CollectSceneInputs() {
            std::vector<LightmapCollectionEntry> entries;
            auto& ecs = NE::GetScene().GetECSCoordinator();
            auto& usedEntities = ecs.GetUsedEntities();
            entries.reserve(usedEntities.size());

            for (NE::ECS::Entity entity : usedEntities) {
                if (!NE::ECS::Query::HasEntityMeta(entity)) {
                    continue;
                }

                const auto& meta = NE::ECS::Query::GetEntityMeta(entity);
                LightmapCollectionEntry entry{};
                entry.entity = entity;
                entry.entityName = GetEntityNameOrFallback(entity);

                if (!meta.staticLightmap) {
                    entry.disposition = LightmapCollectionDisposition::OptedOut;
                    entries.push_back(std::move(entry));
                    continue;
                }

                if (!NE::ECS::Query::GetActive(entity)) {
                    entry.disposition = LightmapCollectionDisposition::OptedOut;
                    entry.warning = MakeWarning(entity, entry.entityName, LightmapFailureReason::None, "Entity is inactive.");
                    entries.push_back(std::move(entry));
                    continue;
                }

                if (!NE::ECS::Query::HasRenderer(entity) || !NE::ECS::Query::HasTransform(entity)) {
                    entry.disposition = LightmapCollectionDisposition::Unresolved;
                    entry.warning = MakeWarning(entity, entry.entityName, LightmapFailureReason::MissingModel, "Entity is missing a renderer or transform.");
                    entries.push_back(std::move(entry));
                    continue;
                }

                auto& renderer = NE::ECS::Command::GetEntityRenderer(entity);
                const auto model = ResolveModel(renderer);
                if (!model || model->meshes.empty()) {
                    entry.disposition = LightmapCollectionDisposition::Unresolved;
                    entry.warning = MakeWarning(entity, entry.entityName, LightmapFailureReason::MissingModel);
                    entries.push_back(std::move(entry));
                    continue;
                }

                const int32_t resolvedSubMeshIndex =
                    (renderer.subMeshIndex >= 0 && renderer.subMeshIndex < static_cast<int32_t>(model->meshes.size()))
                    ? renderer.subMeshIndex
                    : 0;

                if (resolvedSubMeshIndex < 0 || resolvedSubMeshIndex >= static_cast<int32_t>(model->meshes.size())) {
                    entry.disposition = LightmapCollectionDisposition::Unresolved;
                    entry.warning = MakeWarning(entity, entry.entityName, LightmapFailureReason::InvalidSubmesh);
                    entries.push_back(std::move(entry));
                    continue;
                }

                const auto& transform = NE::ECS::Query::GetEntityTransform(entity);
                if (!IsFiniteMatrix(transform.worldMatrix)) {
                    entry.disposition = LightmapCollectionDisposition::Eligible;
                    entry.warning = MakeWarning(entity, entry.entityName, LightmapFailureReason::InvalidTransform);
                    entries.push_back(std::move(entry));
                    continue;
                }

                const auto& subMesh = model->meshes[resolvedSubMeshIndex];
                if (!subMesh.hasUv1) {
                    entry.disposition = LightmapCollectionDisposition::Eligible;
                    entry.warning = MakeWarning(entity, entry.entityName, LightmapFailureReason::MissingUv1);
                    entries.push_back(std::move(entry));
                    continue;
                }

                entry.disposition = LightmapCollectionDisposition::Eligible;
                entry.input = LightmapAllocationInput{
                    entity,
                    entry.entityName,
                    renderer.modelUUID,
                    model.get(),
                    &subMesh,
                    resolvedSubMeshIndex,
                    transform.worldMatrix
                };
                entries.push_back(std::move(entry));
            }

            std::stable_sort(
                entries.begin(),
                entries.end(),
                [](const LightmapCollectionEntry& lhs, const LightmapCollectionEntry& rhs) {
                    if (lhs.entityName != rhs.entityName) {
                        return lhs.entityName < rhs.entityName;
                    }
                    return lhs.entity < rhs.entity;
                });

            return entries;
        }
    } // namespace
