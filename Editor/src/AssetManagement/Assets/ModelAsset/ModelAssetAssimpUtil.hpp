#pragma once

#include <string>

#include <assimp/scene.h>

#include <Math/Mat4.hpp>

namespace Editor::Assets::ModelAssetInternal {
	std::string SafeName(const aiString& s, const char* fallback);
	std::string JoinPath(const std::string& a, const std::string& b);
	NE::Math::Mat4 ToMat4(const aiMatrix4x4& m);
	bool TryGetAIMetaVec3(const aiNode* node, const char* key, aiVector3D& out);
	bool IsFbxPivotHelper(const aiNode* n);
	NE::Math::Mat4 MakeGeomMat(const aiNode* node);
}
