#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Math/Vec3.hpp>

namespace Editor::Lightmapping {

	enum class BakeGeometryFailureReason {
		None,
		NotStatic,
		Inactive,
		MissingRenderer,
		MissingTransform,
		MissingModel,
		InvalidSubmesh,
		InvalidTransform,
		ShadowCastingDisabled,
		ZeroValidTriangles
	};

	struct BakeAABB {
		NE::Math::Vec3 min{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 max{ 0.0f, 0.0f, 0.0f };
	};

	struct BakePrimitiveSource {
		uint32_t entity = 0;
		uint64_t rendererLuid = 0;
		std::string entityName;
		std::string modelUUID;
		std::string materialUUID;
		uint32_t subMeshIndex = 0;
	};

	struct BakeTriangle {
		NE::Math::Vec3 p0{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 p1{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 p2{ 0.0f, 0.0f, 0.0f };
		NE::Math::Vec3 shadingNormal0{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec3 shadingNormal1{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec3 shadingNormal2{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec3 geometricNormal{ 0.0f, 1.0f, 0.0f };
		NE::Math::Vec3 centroid{ 0.0f, 0.0f, 0.0f };
		BakeAABB bounds{};
		float area = 0.0f;
		uint32_t sourceIndex = 0;
		uint32_t originalIndex = 0;
	};

	struct BakeGeometryWarning {
		uint32_t entity = 0;
		std::string entityName;
		BakeGeometryFailureReason reason = BakeGeometryFailureReason::None;
		std::string message;
	};

	struct BakeGeometryCollectionStats {
		size_t consideredEntityCount = 0;
		size_t eligibleEntityCount = 0;
		size_t includedEntityCount = 0;
		size_t skippedEntityCount = 0;
		size_t triangleCount = 0;
		size_t skippedTriangleCount = 0;
	};

	struct BakeGeometryCollection {
		BakeGeometryCollectionStats stats{};
		std::vector<BakePrimitiveSource> sources;
		std::vector<BakeTriangle> triangles;
		std::vector<BakeGeometryWarning> warnings;
	};

	const char* ToString(BakeGeometryFailureReason reason);
	BakeGeometryCollection CollectSceneBakeGeometry();

}
