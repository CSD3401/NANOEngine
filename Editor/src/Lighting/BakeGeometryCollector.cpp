#include "pch.h"
#include "BakeGeometryCollector.hpp"

#include <cmath>

#include <Core/SpdLogger.hpp>
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
		constexpr float kTriangleAreaEpsilon = 1e-8f;

		bool IsFiniteFloat(float value) {
			return std::isfinite(value);
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

		BakeAABB ComputeTriangleBounds(
			const NE::Math::Vec3& p0,
			const NE::Math::Vec3& p1,
			const NE::Math::Vec3& p2) {
			BakeAABB bounds{};
			bounds.min.x = std::min(p0.x, std::min(p1.x, p2.x));
			bounds.min.y = std::min(p0.y, std::min(p1.y, p2.y));
			bounds.min.z = std::min(p0.z, std::min(p1.z, p2.z));
			bounds.max.x = std::max(p0.x, std::max(p1.x, p2.x));
			bounds.max.y = std::max(p0.y, std::max(p1.y, p2.y));
			bounds.max.z = std::max(p0.z, std::max(p1.z, p2.z));
			return bounds;
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

		int32_t ResolveSubMeshIndex(const NE::ECS::Component::Renderer& renderer, const NE::Graphics::Model& model) {
			if (model.meshes.empty()) {
				return -1;
			}

			if (renderer.subMeshIndex >= 0 && renderer.subMeshIndex < static_cast<int32_t>(model.meshes.size())) {
				return renderer.subMeshIndex;
			}

			return 0;
		}

		std::string BuildWarningMessage(BakeGeometryFailureReason reason) {
			switch (reason) {
			case BakeGeometryFailureReason::NotStatic: return "Entity is not marked Static Lightmap.";
			case BakeGeometryFailureReason::Inactive: return "Entity is inactive in the scene.";
			case BakeGeometryFailureReason::MissingRenderer: return "Entity is missing a renderer component.";
			case BakeGeometryFailureReason::MissingTransform: return "Entity is missing a transform component.";
			case BakeGeometryFailureReason::MissingModel: return "Renderer has no loaded cooked model CPU data.";
			case BakeGeometryFailureReason::InvalidSubmesh: return "Renderer resolved to an invalid submesh.";
			case BakeGeometryFailureReason::InvalidTransform: return "Entity world transform contains non-finite values.";
			case BakeGeometryFailureReason::ShadowCastingDisabled: return "Renderer shadow casting is disabled.";
			case BakeGeometryFailureReason::ZeroValidTriangles: return "Entity produced no valid bake triangles.";
			case BakeGeometryFailureReason::None:
			default: return {};
			}
		}

		void PushWarning(
			BakeGeometryCollection& collection,
			uint32_t entity,
			const std::string& entityName,
			BakeGeometryFailureReason reason,
			const std::string& message = {}) {
			collection.warnings.push_back({
				entity,
				entityName,
				reason,
				message.empty() ? BuildWarningMessage(reason) : message
			});
		}

		NE::Math::Vec3 ResolveShadingNormal(
			const NE::Math::Vec3& sourceNormal,
			const NE::Math::Mat4& normalMatrix,
			bool hasValidNormalMatrix,
			const NE::Math::Vec3& geometricNormal) {
			if (!IsFiniteVec3(sourceNormal) || sourceNormal.LengthSquared() <= kTriangleAreaEpsilon) {
				return geometricNormal;
			}

			NE::Math::Vec3 transformed = hasValidNormalMatrix ? TransformDirection(normalMatrix, sourceNormal) : sourceNormal;
			if (!IsFiniteVec3(transformed) || transformed.LengthSquared() <= kTriangleAreaEpsilon) {
				return geometricNormal;
			}

			return transformed.Normalized();
		}
	}

	const char* ToString(BakeGeometryFailureReason reason) {
		switch (reason) {
		case BakeGeometryFailureReason::NotStatic: return "Not Static";
		case BakeGeometryFailureReason::Inactive: return "Inactive";
		case BakeGeometryFailureReason::MissingRenderer: return "Missing Renderer";
		case BakeGeometryFailureReason::MissingTransform: return "Missing Transform";
		case BakeGeometryFailureReason::MissingModel: return "Missing Model";
		case BakeGeometryFailureReason::InvalidSubmesh: return "Invalid Submesh";
		case BakeGeometryFailureReason::InvalidTransform: return "Invalid Transform";
		case BakeGeometryFailureReason::ShadowCastingDisabled: return "Shadow Casting Disabled";
		case BakeGeometryFailureReason::ZeroValidTriangles: return "Zero Valid Triangles";
		case BakeGeometryFailureReason::None:
		default: return "None";
		}
	}

	BakeGeometryCollection CollectSceneBakeGeometry() {
		BakeGeometryCollection collection{};

		const auto& entities = NE::GetNumEntities();
		collection.sources.reserve(entities.size());

		for (uint32_t entity : entities) {
			if (!NE::ECS::Query::HasEntityMeta(entity)) {
				continue;
			}

			++collection.stats.consideredEntityCount;

			const auto& meta = NE::ECS::Query::GetEntityMeta(entity);
			const std::string entityName = GetEntityNameOrFallback(entity);

			if (!meta.isStatic) {
				++collection.stats.skippedEntityCount;
				continue;
			}

			++collection.stats.eligibleEntityCount;

			if (!NE::ECS::Query::GetActive(entity)) {
				++collection.stats.skippedEntityCount;
				PushWarning(collection, entity, entityName, BakeGeometryFailureReason::Inactive);
				continue;
			}

			if (!NE::ECS::Query::HasRenderer(entity)) {
				++collection.stats.skippedEntityCount;
				PushWarning(collection, entity, entityName, BakeGeometryFailureReason::MissingRenderer);
				continue;
			}

			if (!NE::ECS::Query::HasTransform(entity)) {
				++collection.stats.skippedEntityCount;
				PushWarning(collection, entity, entityName, BakeGeometryFailureReason::MissingTransform);
				continue;
			}

			auto& renderer = NE::ECS::Command::GetEntityRenderer(entity);
			if (renderer.shadowCastMode == NE::ECS::Component::Renderer::ShadowCastMode::Off) {
				++collection.stats.skippedEntityCount;
				PushWarning(collection, entity, entityName, BakeGeometryFailureReason::ShadowCastingDisabled);
				continue;
			}

			const auto model = ResolveModel(entity, renderer);
			if (!model || model->meshes.empty()) {
				++collection.stats.skippedEntityCount;
				PushWarning(collection, entity, entityName, BakeGeometryFailureReason::MissingModel);
				continue;
			}

			const int32_t subMeshIndex = ResolveSubMeshIndex(renderer, *model);
			if (subMeshIndex < 0 || subMeshIndex >= static_cast<int32_t>(model->meshes.size())) {
				++collection.stats.skippedEntityCount;
				PushWarning(collection, entity, entityName, BakeGeometryFailureReason::InvalidSubmesh);
				continue;
			}

			const auto& transform = NE::ECS::Query::GetEntityTransform(entity);
			if (!IsFiniteMatrix(transform.worldMatrix)) {
				++collection.stats.skippedEntityCount;
				PushWarning(collection, entity, entityName, BakeGeometryFailureReason::InvalidTransform);
				continue;
			}

			const auto& subMesh = model->meshes[static_cast<size_t>(subMeshIndex)];
			if (subMesh.vertices.empty() || subMesh.indices.size() < 3) {
				++collection.stats.skippedEntityCount;
				PushWarning(collection, entity, entityName, BakeGeometryFailureReason::ZeroValidTriangles, "Resolved submesh has no triangle data.");
				continue;
			}

			const uint32_t sourceIndex = static_cast<uint32_t>(collection.sources.size());
			collection.sources.push_back({
				entity,
				renderer.luid,
				entityName,
				renderer.modelUUID,
				renderer.materialUUID,
				static_cast<uint32_t>(subMeshIndex)
			});

			const float determinant = transform.worldMatrix.Determinant();
			const bool hasValidNormalMatrix = std::isfinite(determinant) && std::fabs(determinant) > 1e-8f;
			const NE::Math::Mat4 normalMatrix = hasValidNormalMatrix
				? transform.worldMatrix.Inverse().Transpose()
				: transform.worldMatrix;

			const size_t firstTriangle = collection.triangles.size();
			for (size_t index = 0; index + 2 < subMesh.indices.size(); index += 3) {
				const uint32_t ia = subMesh.indices[index + 0];
				const uint32_t ib = subMesh.indices[index + 1];
				const uint32_t ic = subMesh.indices[index + 2];
				if (ia >= subMesh.vertices.size() || ib >= subMesh.vertices.size() || ic >= subMesh.vertices.size()) {
					++collection.stats.skippedTriangleCount;
					continue;
				}

				const auto& va = subMesh.vertices[ia];
				const auto& vb = subMesh.vertices[ib];
				const auto& vc = subMesh.vertices[ic];

				const NE::Math::Vec3 p0 = TransformPoint(transform.worldMatrix, va.position);
				const NE::Math::Vec3 p1 = TransformPoint(transform.worldMatrix, vb.position);
				const NE::Math::Vec3 p2 = TransformPoint(transform.worldMatrix, vc.position);
				if (!IsFiniteVec3(p0) || !IsFiniteVec3(p1) || !IsFiniteVec3(p2)) {
					++collection.stats.skippedTriangleCount;
					continue;
				}

				const NE::Math::Vec3 geometricCross = (p1 - p0).Cross(p2 - p0);
				const float doubleArea = geometricCross.Length();
				if (!std::isfinite(doubleArea) || doubleArea <= kTriangleAreaEpsilon) {
					++collection.stats.skippedTriangleCount;
					continue;
				}

				const NE::Math::Vec3 geometricNormal = geometricCross / doubleArea;
				BakeTriangle triangle{};
				triangle.p0 = p0;
				triangle.p1 = p1;
				triangle.p2 = p2;
				triangle.shadingNormal0 = ResolveShadingNormal(va.normal, normalMatrix, hasValidNormalMatrix, geometricNormal);
				triangle.shadingNormal1 = ResolveShadingNormal(vb.normal, normalMatrix, hasValidNormalMatrix, geometricNormal);
				triangle.shadingNormal2 = ResolveShadingNormal(vc.normal, normalMatrix, hasValidNormalMatrix, geometricNormal);
				triangle.geometricNormal = geometricNormal;
				triangle.centroid = (p0 + p1 + p2) / 3.0f;
				triangle.bounds = ComputeTriangleBounds(p0, p1, p2);
				triangle.area = 0.5f * doubleArea;
				triangle.sourceIndex = sourceIndex;
				triangle.originalIndex = static_cast<uint32_t>(collection.triangles.size());
				collection.triangles.push_back(triangle);
			}

			if (collection.triangles.size() == firstTriangle) {
				collection.sources.pop_back();
				++collection.stats.skippedEntityCount;
				PushWarning(collection, entity, entityName, BakeGeometryFailureReason::ZeroValidTriangles);
				continue;
			}

			++collection.stats.includedEntityCount;
		}

		collection.stats.triangleCount = collection.triangles.size();

		SPD_INFO(
			"Collected scene bake geometry: "
			<< collection.stats.includedEntityCount << " renderers, "
			<< collection.stats.triangleCount << " triangles, "
			<< collection.stats.skippedTriangleCount << " skipped triangles.");

		return collection;
	}
}
