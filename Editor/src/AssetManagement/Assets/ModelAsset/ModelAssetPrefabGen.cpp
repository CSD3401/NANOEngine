#include "pch.h"
#include "ModelAssetPrefabGen.hpp"

#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Hierarchy.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Transform.hpp>

#include "../../../Serialization/JSONReflection.hpp"
#include "ModelAssetAssimpUtil.hpp"

namespace Editor::Assets::ModelAssetInternal {
	namespace {
		const double PI = 3.14159265358979323846;

		void EmitEntityMeta(
			rapidjson::Value& ent,
			rapidjson::Document::AllocatorType& alloc,
			const std::string& name,
			uint64_t luid
		) {
			NE::ECS::Component::EntityMeta meta{};
			meta.name = name;
			meta.isActive = 1;
			meta.luid = luid;
			ent.AddMember("EntityMeta", Editor::Serialization::ToJSON(meta, alloc), alloc);
		}

		void EmitHierarchy(
			rapidjson::Value& ent,
			rapidjson::Document::AllocatorType& alloc,
			uint64_t luid,
			uint64_t parentLuid
		) {
			NE::ECS::Component::Hierarchy hierarchy{};
			hierarchy.luid = luid;
			hierarchy.parentLuid = parentLuid;
			ent.AddMember("Hierarchy", Editor::Serialization::ToJSON(hierarchy, alloc), alloc);
		}

		void EmitTransformFromMat4(
			rapidjson::Value& ent,
			rapidjson::Document::AllocatorType& alloc,
			uint64_t luid,
			const NE::Math::Mat4& m,
			float sceneScale
		) {
			NE::ECS::Component::Transform tr{};
			tr.luid = luid;
			tr.localPosition = m.GetTranslation() * sceneScale;
			tr.localScale = m.GetScale();
			tr.localRotationEuler = m.GetRotation() * static_cast<float>(180.0 / PI);
			ent.AddMember("Transform", Editor::Serialization::ToJSON(tr, alloc), alloc);
		}

		void EmitRenderer(
			rapidjson::Value& ent,
			rapidjson::Document::AllocatorType& alloc,
			uint64_t luid,
			const std::string& modelUUID,
			const std::string& materialUUID,
			int32_t subMeshIndex
		) {
			NE::ECS::Component::Renderer renderer{};
			renderer.luid = luid;
			renderer.modelUUID = modelUUID;
			renderer.materialUUID = materialUUID;
			renderer.subMeshIndex = subMeshIndex;
			ent.AddMember("Renderer", Editor::Serialization::ToJSON(renderer, alloc), alloc);
		}

		void BuildMeshesUnderRootFlat(
			const aiScene* scene,
			const aiNode* node,
			uint64_t rootEnt,
			const NE::Math::Mat4& accumLocal,
			float sceneScale,
			const std::string& modelUUID,
			const std::vector<std::string>& materialUUIDByAssimpMat,
			rapidjson::Value& ents,
			rapidjson::Document::AllocatorType& alloc,
			uint64_t& next
		) {
			const NE::Math::Mat4 nodeLocal = ToMat4(node->mTransformation);
			const NE::Math::Mat4 nextAccum = accumLocal * nodeLocal;

			if (IsFbxPivotHelper(node)) {
				for (unsigned c = 0; c < node->mNumChildren; ++c) {
					BuildMeshesUnderRootFlat(
						scene,
						node->mChildren[c],
						rootEnt,
						nextAccum,
						sceneScale,
						modelUUID,
						materialUUIDByAssimpMat,
						ents,
						alloc,
						next
					);
				}
				return;
			}

			const std::string nodeName = SafeName(node->mName, "Node");
			for (unsigned i = 0; i < node->mNumMeshes; ++i) {
				const unsigned meshIdx = node->mMeshes[i];
				const aiMesh* mesh = scene->mMeshes[meshIdx];

				const uint64_t meshEnt = next++;

				rapidjson::Value rend(rapidjson::kObjectType);
				rend.AddMember("Layer", 0, alloc);

				std::string meshName = (mesh && mesh->mName.length > 0)
					? std::string(mesh->mName.C_Str())
					: (nodeName + "_Mesh" + std::to_string(i));

				EmitEntityMeta(rend, alloc, meshName, 0);
				EmitHierarchy(rend, alloc, meshEnt, rootEnt);

				const NE::Math::Mat4 geom = MakeGeomMat(node);
				const NE::Math::Mat4 finalLocal = nextAccum * geom;
				EmitTransformFromMat4(rend, alloc, meshEnt, finalLocal, sceneScale);

				std::string matUUID;
				if (mesh && mesh->mMaterialIndex < materialUUIDByAssimpMat.size()) {
					matUUID = materialUUIDByAssimpMat[mesh->mMaterialIndex];
				}

				EmitRenderer(rend, alloc, 0, modelUUID, matUUID, static_cast<int32_t>(meshIdx));
				ents.PushBack(rend, alloc);
			}

			for (unsigned c = 0; c < node->mNumChildren; ++c) {
				BuildMeshesUnderRootFlat(
					scene,
					node->mChildren[c],
					rootEnt,
					nextAccum,
					sceneScale,
					modelUUID,
					materialUUIDByAssimpMat,
					ents,
					alloc,
					next
				);
			}
		}
	}

	void BuildGeneratedPrefab(
		const aiScene* scene,
		const std::string& modelUUID,
		const std::vector<std::string>& materialUUIDByAssimpMat,
		float sceneScale,
		const std::string& rootName,
		rapidjson::Value& outPrefabObj,
		rapidjson::Document::AllocatorType& alloc
	) {
		outPrefabObj.SetObject();
		outPrefabObj.AddMember("schemaVersion", 1, alloc);

		rapidjson::Value ents(rapidjson::kArrayType);
		if (!scene || !scene->mRootNode) {
			outPrefabObj.AddMember("Entities", ents, alloc);
			return;
		}

		uint64_t next = 1;
		const uint64_t rootEnt = next++;

		rapidjson::Value root(rapidjson::kObjectType);
		root.AddMember("Layer", 0, alloc);

		const NE::Math::Mat4 rootTransform = ToMat4(scene->mRootNode->mTransformation);
		EmitEntityMeta(root, alloc, rootName, rootEnt);
		EmitHierarchy(root, alloc, rootEnt, 0);
		EmitTransformFromMat4(root, alloc, rootEnt, rootTransform, sceneScale);

		ents.PushBack(root, alloc);

		NE::Math::Mat4 identity;
		identity.SetToIdentity();
		BuildMeshesUnderRootFlat(
			scene,
			scene->mRootNode,
			rootEnt,
			identity,
			sceneScale,
			modelUUID,
			materialUUIDByAssimpMat,
			ents,
			alloc,
			next
		);

		outPrefabObj.AddMember("Entities", ents, alloc);
	}
}
