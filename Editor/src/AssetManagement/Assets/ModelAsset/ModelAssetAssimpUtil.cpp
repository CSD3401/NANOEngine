#include "pch.h"
#include "ModelAssetAssimpUtil.hpp"

#include <cstring>

namespace Editor::Assets::ModelAssetInternal {
	std::string SafeName(const aiString& s, const char* fallback) {
		const char* c = s.C_Str();
		if (!c || c[0] == '\0') return fallback;
		return c;
	}

	std::string JoinPath(const std::string& a, const std::string& b) {
		if (a.empty()) return b;
		return a + "/" + b;
	}

	NE::Math::Mat4 ToMat4(const aiMatrix4x4& m) {
		return NE::Math::Mat4(
			m.a1, m.a2, m.a3, m.a4,
			m.b1, m.b2, m.b3, m.b4,
			m.c1, m.c2, m.c3, m.c4,
			m.d1, m.d2, m.d3, m.d4
		);
	}

	bool TryGetAIMetaVec3(const aiNode* node, const char* key, aiVector3D& out) {
		if (!node || !node->mMetaData) return false;
		return node->mMetaData->Get(key, out);
	}

	bool IsFbxPivotHelper(const aiNode* n) {
		std::string name = n->mName.C_Str();
		if (name.find("$AssimpFbx$") != std::string::npos) return true;

		auto endsWith = [&](const char* suffix) {
			const size_t suffixLen = strlen(suffix);
			if (name.size() < suffixLen) return false;
			return name.compare(name.size() - suffixLen, suffixLen, suffix) == 0;
			};

		return endsWith("_RotationPivot") ||
			endsWith("_RotationPivotInverse") ||
			endsWith("_ScalingPivot") ||
			endsWith("_ScalingPivotInverse");
	}

	NE::Math::Mat4 MakeGeomMat(const aiNode* node) {
		aiVector3D gt(0, 0, 0), gr(0, 0, 0), gs(1, 1, 1);
		TryGetAIMetaVec3(node, "GeometricTranslation", gt);
		TryGetAIMetaVec3(node, "GeometricRotation", gr);
		TryGetAIMetaVec3(node, "GeometricScaling", gs);

		NE::Math::Mat4 t{};
		t = t.BuildTranslation(gt.x, gt.y, gt.z);

		NE::Math::Mat4 rx{};
		rx = rx.BuildXRotation(gr.x);
		NE::Math::Mat4 ry{};
		ry = ry.BuildYRotation(gr.y);
		NE::Math::Mat4 rz{};
		rz = rz.BuildZRotation(gr.z);
		NE::Math::Mat4 r = rz * ry * rx;

		NE::Math::Mat4 s{};
		s = s.BuildScaling(gs.x, gs.y, gs.z);

		return t * r * s;
	}
}
