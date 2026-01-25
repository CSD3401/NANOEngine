#include "ModelAsset.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/istreamwrapper.h>

#include <ResourceManagement/BinaryHeaders/NanoModelHeader.hpp>
#include <Math/Vec3.hpp>
#include <Core/SpdLogger.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Hierarchy.hpp>
#include <ECS/Components/Renderer.hpp>
#include <Graphics/Core/Material.hpp>

#include "../AssetManager.hpp"
#include "../../Serialization/JSONReflection.hpp"
#include "../Interfaces/MaterialEditor.hpp"
#include "TextureAsset.hpp"
#include <ResourceManagement/ResourcePaths.hpp>

namespace Editor::Assets {
	namespace {
		const double PI = 3.14159265358979323846;

		float GuessScaleFactorFromExtension(std::string extension) {
			const std::unordered_map<std::string, float> scaleGuesses = {
				{ ".fbx", 0.01f },
				{ ".obj", 1.0f },
				{ ".dae", 1.0f },
				{ ".gltf", 1.0f },
				{ ".glb", 1.0f }
			};
			auto it = scaleGuesses.find(extension);
			if (it != scaleGuesses.end()) {
				return it->second;
			}
			return 1.0f;
		}

		struct CookVertex {
			float px, py, pz;
			float nx, ny, nz;
			float u, v;
		};

		struct Bounds {
			NE::Math::Vec3 minP{ FLT_MAX, FLT_MAX, FLT_MAX };
			NE::Math::Vec3 maxP{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
			bool valid = false;

			void Expand(float x, float y, float z) {
				minP.x = std::min(minP.x, x); minP.y = std::min(minP.y, y); minP.z = std::min(minP.z, z);
				maxP.x = std::max(maxP.x, x); maxP.y = std::max(maxP.y, y); maxP.z = std::max(maxP.z, z);
				valid = true;
			}
		};

		inline void WriteBoundsToDesc(const Bounds& b,
			float aabbMin[3], float aabbMax[3],
			float sphereCenter[3], float& sphereRadius)
		{
			if (!b.valid) {
				aabbMin[0] = aabbMin[1] = aabbMin[2] = 0;
				aabbMax[0] = aabbMax[1] = aabbMax[2] = 0;
				sphereCenter[0] = sphereCenter[1] = sphereCenter[2] = 0;
				sphereRadius = 0;
				return;
			}

			aabbMin[0] = b.minP.x; aabbMin[1] = b.minP.y; aabbMin[2] = b.minP.z;
			aabbMax[0] = b.maxP.x; aabbMax[1] = b.maxP.y; aabbMax[2] = b.maxP.z;

			NE::Math::Vec3 c = (b.minP + b.maxP) * 0.5f;
			NE::Math::Vec3 e = (b.maxP - b.minP) * 0.5f;

			sphereCenter[0] = c.x; sphereCenter[1] = c.y; sphereCenter[2] = c.z;
			sphereRadius = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);
		}

		NE::Resource::ResourceType GetResourceTypeFromAssetType(Assets::AssetType type) {
			switch (type) {
			case Assets::AssetType::Texture:    return NE::Resource::ResourceType::Texture;
			case Assets::AssetType::Model:      return NE::Resource::ResourceType::Model;
			case Assets::AssetType::Material:   return NE::Resource::ResourceType::Material;
			case Assets::AssetType::Shader:     return NE::Resource::ResourceType::Shader;
			case Assets::AssetType::Scene:      return NE::Resource::ResourceType::Scene;
			case Assets::AssetType::Prefab:     return NE::Resource::ResourceType::Prefab;
			default:                            return NE::Resource::ResourceType::Unknown;
			}
		}

		NE::Math::Vec3 QuatToEulerXYZ_Degrees(const aiQuaternion& q) {
			const double x = q.x, y = q.y, z = q.z, w = q.w;

			double sinr_cosp = 2.0 * (w * x + y * z);
			double cosr_cosp = 1.0 - 2.0 * (x * x + y * y);
			double roll = std::atan2(sinr_cosp, cosr_cosp);

			double sinp = 2.0 * (w * y - z * x);
			double pitch;
			if (std::abs(sinp) >= 1.0) {
				pitch = std::copysign(PI / 2.0, sinp);
			} else {
				pitch = std::asin(sinp);
			}

			double siny_cosp = 2.0 * (w * z + x * y);
			double cosy_cosp = 1.0 - 2.0 * (y * y + z * z);
			double yaw = std::atan2(siny_cosp, cosy_cosp);

			const double rad2deg = 180.0 / PI;
			roll *= rad2deg;
			pitch *= rad2deg;
			yaw *= rad2deg;

			return NE::Math::Vec3((float)roll, (float)pitch, (float)yaw);
		}

		struct BuiltEntity {
			uint64_t luid;
			rapidjson::Value json;
		};

		std::string SafeName(const aiString& s, const char* fallback) {
			const char* c = s.C_Str();
			if (!c || c[0] == '\0') return fallback;
			return c;
		}

		bool IsFbxPivotHelper(const aiNode* n) {
			std::string name = n->mName.C_Str();
			if (name.find("$AssimpFbx$") != std::string::npos) return true;

			auto endsWith = [&](const char* s) {
				if (name.size() < strlen(s)) return false;
				return name.compare(name.size() - strlen(s), strlen(s), s) == 0;
				};

			return endsWith("_RotationPivot") ||
				endsWith("_RotationPivotInverse") ||
				endsWith("_ScalingPivot") ||
				endsWith("_ScalingPivotInverse");
		}

		NE::Math::Mat4 ToMat4(const aiMatrix4x4& m) {
			NE::Math::Mat4 r{ 
				m.a1, m.b1, m.c1, m.d1, 
				m.a2, m.b2, m.c2, m.d2, 
				m.a3, m.b3, m.c3, m.d3, 
				m.a4, m.b4, m.c4, m.d4 
			};
			//r[0][0] = m.a1; r[1][0] = m.a2; r[2][0] = m.a3; r[3][0] = m.a4;
			//r[0][1] = m.b1; r[1][1] = m.b2; r[2][1] = m.b3; r[3][1] = m.b4;
			//r[0][2] = m.c1; r[1][2] = m.c2; r[2][2] = m.c3; r[3][2] = m.c4;
			//r[0][3] = m.d1; r[1][3] = m.d2; r[2][3] = m.d3; r[3][3] = m.d4;
			return r;
		}

		//void FlattenNode(const aiScene* scene,
		//	const aiNode* node,
		//	const NE::Math::Mat4& parentWorld,
		//	/*out*/ std::vector<ImportedMeshNode>& outMeshes)
		//{
		//	NE::Math::Mat4 local = ToMat4(node->mTransformation);
		//	NE::Math::Mat4 world = parentWorld * local;

		//	// If this node contains meshes, emit them with the final baked world transform.
		//	for (unsigned i = 0; i < node->mNumMeshes; ++i) {
		//		unsigned meshIndex = node->mMeshes[i];
		//		outMeshes.push_back({
		//			.name = node->mName.C_Str(),
		//			.meshIndex = meshIndex,
		//			.worldTransform = world
		//			});
		//	}

		//	// Recurse children normally, but you can optionally suppress creating entities for helper nodes
		//	// by just never "emitting" non-mesh nodes in your importer.
		//	for (unsigned c = 0; c < node->mNumChildren; ++c)
		//		FlattenNode(scene, node->mChildren[c], world, outMeshes);
		//}

		void EmitEntityMeta(rapidjson::Value& ent, rapidjson::Document::AllocatorType& a,
			const std::string& name, uint64_t luid) {
			NE::ECS::Component::EntityMeta meta{};
			meta.name = name;
			meta.isActive = 1;
			meta.luid = luid;
			ent.AddMember("EntityMeta", Editor::Serialization::ToJSON(meta, a), a);
		}

		void EmitHierarchy(rapidjson::Value& ent, rapidjson::Document::AllocatorType& a,
			uint64_t luid, uint64_t parentLuid) {
			NE::ECS::Component::Hierarchy h{};
			h.luid = luid;
			h.parentLuid = parentLuid;
			ent.AddMember("Hierarchy", Editor::Serialization::ToJSON(h, a), a);
		}

		void EmitTransformFromAssimp(rapidjson::Value& ent, rapidjson::Document::AllocatorType& a,
			uint64_t luid, const aiMatrix4x4& localM) {
			aiVector3D s, t;
			aiQuaternion r;
			localM.Decompose(s, r, t);

			NE::ECS::Component::Transform tr{};
			tr.luid = luid;
			tr.localPosition = NE::Math::Vec3(t.x, t.y, t.z);
			tr.localScale = NE::Math::Vec3(s.x, s.y, s.z);
			tr.localRotationEuler = QuatToEulerXYZ_Degrees(r);
			ent.AddMember("Transform", Editor::Serialization::ToJSON(tr, a), a);
		}

		void EmitTransformFromMat4(
			rapidjson::Value& ent,
			rapidjson::Document::AllocatorType& a,
			uint64_t luid,
			const NE::Math::Mat4& m
		) {
			NE::ECS::Component::Transform tr{};
			tr.luid = luid;

			tr.localPosition = m.GetTranslation();
			tr.localScale = m.GetScale();
			tr.localRotationEuler = m.GetRotation();
			ent.AddMember("Transform", Editor::Serialization::ToJSON(tr, a), a);
		}

		void EmitRenderer(rapidjson::Value& ent, rapidjson::Document::AllocatorType& a,
			uint64_t luid, const std::string& modelUUID, const std::string& materialUUID,
			int32_t subMeshIndex) {
			NE::ECS::Component::Renderer r{};
			r.luid = luid;
			r.modelUUID = modelUUID;
			r.materialUUID = materialUUID;
			r.subMeshIndex = subMeshIndex;
			ent.AddMember("Renderer", Editor::Serialization::ToJSON(r, a), a);
		}

		void AddEntityFromNode(
			const aiScene* scene,
			const NE::Math::Mat4& finalLocal,
			const aiNode* node,
			uint64_t parentEnt,
			const std::string& modelUUID,
			const std::vector<std::string>& materialUUIDByAssimpMat,
			rapidjson::Value& ents,
			rapidjson::Document::AllocatorType& alloc,
			uint64_t& thisEnt,
			uint64_t& next)
		{
			// ----- Node entity -----
			rapidjson::Value ent(rapidjson::kObjectType);
			ent.AddMember("Layer", 0, alloc);

			const std::string nodeName = SafeName(node->mName, "Node");
			EmitEntityMeta(ent, alloc, nodeName, thisEnt);
			EmitHierarchy(ent, alloc, thisEnt, parentEnt);

			// IMPORTANT: use flattened matrix, not node->mTransformation
			// You must implement this helper (see below)
			EmitTransformFromMat4(ent, alloc, thisEnt, finalLocal);

			ents.PushBack(ent, alloc);

			// ----- One renderer entity per mesh -----
			for (unsigned i = 0; i < node->mNumMeshes; ++i) {
				const unsigned meshIdx = node->mMeshes[i];
				const aiMesh* mesh = scene->mMeshes[meshIdx];

				const uint64_t rendLuid = next++;

				rapidjson::Value rend(rapidjson::kObjectType);
				rend.AddMember("Layer", 0, alloc);

				std::string meshName = (mesh && mesh->mName.length > 0)
					? std::string(mesh->mName.C_Str())
					: (nodeName + "_Mesh" + std::to_string(i));

				// Keep your exact behavior: meta luid = 0
				EmitEntityMeta(rend, alloc, meshName, 0);
				EmitHierarchy(rend, alloc, rendLuid, thisEnt);

				// Keep your exact behavior: renderer entity transform = identity
				NE::ECS::Component::Transform tr{};
				tr.luid = 0;
				tr.localPosition = { 0,0,0 };
				tr.localScale = { 1,1,1 };
				tr.localRotationEuler = { 0,0,0 };
				rend.AddMember("Transform", Editor::Serialization::ToJSON(tr, alloc), alloc);

				std::string matUUID;
				if (mesh && mesh->mMaterialIndex < materialUUIDByAssimpMat.size())
					matUUID = materialUUIDByAssimpMat[mesh->mMaterialIndex]; // "" for default

				// Keep your exact behavior: renderer luid param = 0
				EmitRenderer(rend, alloc, 0, modelUUID, matUUID, (int32_t)meshIdx);

				ents.PushBack(rend, alloc);
			}
		}

		void BuildGeneratedPrefabRecursiveFlat(
			const aiScene* scene,
			const aiNode* node,
			uint64_t parentEnt,
			const NE::Math::Mat4& accumLocal,
			const std::string& modelUUID,
			const std::vector<std::string>& materialUUIDByAssimpMat,
			rapidjson::Value& ents,
			rapidjson::Document::AllocatorType& alloc,
			uint64_t& next
		) {
			NE::Math::Mat4 nodeLocal = ToMat4(node->mTransformation);

			if (IsFbxPivotHelper(node)) {
				NE::Math::Mat4 nextAccum = accumLocal * nodeLocal;
				for (unsigned c = 0; c < node->mNumChildren; ++c)
					BuildGeneratedPrefabRecursiveFlat(scene, node->mChildren[c], parentEnt, nextAccum,
						modelUUID, materialUUIDByAssimpMat, ents, alloc, next);
				return;
			}

			NE::Math::Mat4 finalLocal = accumLocal * nodeLocal;

			uint64_t thisEnt = next++;
			AddEntityFromNode(scene, finalLocal, node, parentEnt, modelUUID,
				materialUUIDByAssimpMat, ents, alloc, thisEnt, next);

			NE::Math::Mat4 I; I.SetToIdentity();
			for (unsigned c = 0; c < node->mNumChildren; ++c)
				BuildGeneratedPrefabRecursiveFlat(scene, node->mChildren[c], thisEnt, I,
					modelUUID, materialUUIDByAssimpMat, ents, alloc, next);
		}

		void BuildGeneratedPrefabRecursive(
			const aiScene* scene,
			const aiNode* node,
			uint64_t parentLuid,
			const std::string& cookedModelUUID,
			const std::vector<std::string>& materialUUIDByAssimpMat,
			rapidjson::Value& outEntitiesArray,
			rapidjson::Document::AllocatorType& a,
			uint64_t& nextLuid
		) {
			const uint64_t nodeLuid = nextLuid++;

			rapidjson::Value ent(rapidjson::kObjectType);
			ent.AddMember("Layer", 0, a);

			const std::string nodeName = SafeName(node->mName, "Node");
			EmitEntityMeta(ent, a, nodeName, nodeLuid);
			EmitHierarchy(ent, a, nodeLuid, parentLuid);
			EmitTransformFromAssimp(ent, a, nodeLuid, node->mTransformation);

			outEntitiesArray.PushBack(ent, a);

			for (unsigned i = 0; i < node->mNumMeshes; ++i) {
				const unsigned meshIdx = node->mMeshes[i];
				const aiMesh* mesh = scene->mMeshes[meshIdx];

				const uint64_t rendLuid = nextLuid++;

				rapidjson::Value rend(rapidjson::kObjectType);
				rend.AddMember("Layer", 0, a);

				std::string meshName = (mesh && mesh->mName.length > 0)
					? std::string(mesh->mName.C_Str())
					: (nodeName + "_Mesh" + std::to_string(i));

				EmitEntityMeta(rend, a, meshName, 0);
				EmitHierarchy(rend, a, rendLuid, nodeLuid);

				NE::ECS::Component::Transform tr{};
				tr.luid = 0;
				tr.localPosition = { 0,0,0 };
				tr.localScale = { 1,1,1 };
				tr.localRotationEuler = { 0,0,0 };
				rend.AddMember("Transform", Editor::Serialization::ToJSON(tr, a), a);

				std::string matUUID;
				if (mesh && mesh->mMaterialIndex < materialUUIDByAssimpMat.size())
					matUUID = materialUUIDByAssimpMat[mesh->mMaterialIndex]; // "" for default

				EmitRenderer(rend, a, 0, cookedModelUUID, matUUID, (int32_t)meshIdx);

				outEntitiesArray.PushBack(rend, a);
			}

			for (unsigned c = 0; c < node->mNumChildren; ++c) {
				BuildGeneratedPrefabRecursive(scene, node->mChildren[c], nodeLuid,
					cookedModelUUID, materialUUIDByAssimpMat,
					outEntitiesArray, a, nextLuid);
			}
		}

		std::string JoinPath(const std::string& a, const std::string& b) {
			if (a.empty()) return b;
			return a + "/" + b;
		}

		void GatherSubmeshEntries(
			const aiScene* scene,
			const aiNode* node,
			const std::string& parentPath,
			std::vector<std::string>& outNameByMeshIdx
		) {
			const std::string nodeName = SafeName(node->mName, "Node");
			const std::string nodePath = JoinPath(parentPath, nodeName);

			for (unsigned i = 0; i < node->mNumMeshes; ++i) {
				const unsigned meshIdx = node->mMeshes[i];
				if (meshIdx >= scene->mNumMeshes) continue;

				if (!outNameByMeshIdx[meshIdx].empty())
					continue;

				const aiMesh* mesh = scene->mMeshes[meshIdx];

				std::string meshName;
				if (mesh && mesh->mName.length > 0) {
					meshName = std::string(mesh->mName.C_Str());
				} else {
					meshName = nodeName + "_Mesh" + std::to_string(i);
				}

				outNameByMeshIdx[meshIdx] = nodePath + "/" + meshName;
			}

			for (unsigned c = 0; c < node->mNumChildren; ++c) {
				GatherSubmeshEntries(scene, node->mChildren[c], nodePath, outNameByMeshIdx);
			}
		}

		struct ImportedMatDesc {
			std::string name;

			NE::Math::Vec3 baseColor{ 1.f, 1.f, 1.f };
			float metallic = 0.f;
			float roughness = 0.f;
			float opacity = 1.0f;
			NE::Math::Vec3 emissive{ 0,0,0 };

			std::string albedoMapUUID;
			std::string normalMapUUID;
			std::string metallicMapUUID;
			std::string roughnessMapUUID;
			std::string ambientOcclusionMapUUID;
			std::string emissionMapUUID;
			std::string opacityMapUUID;
		};

		bool GetTexturePath(const aiMaterial* mat, aiTextureType type, aiString& out) {
			if (!mat || mat->GetTextureCount(type) == 0) return false;
			return mat->GetTexture(type, 0, &out) == AI_SUCCESS;
		}

		ImportedMatDesc ExtractMaterialFBX(const aiMaterial* mat, const std::filesystem::path& parentDir) {
			ImportedMatDesc d{};

			const std::filesystem::path mandatoryTexDir = parentDir / "Textures";

			aiString n;
			if (mat->Get(AI_MATKEY_NAME, n) == AI_SUCCESS) d.name = n.C_Str();
			if (d.name.empty()) d.name = "Material";

			aiColor3D kd(1, 1, 1), ke(0, 0, 0);
			float opacity = 1.0f;
			float shininess = 0.0f;

			mat->Get(AI_MATKEY_COLOR_DIFFUSE, kd);
			mat->Get(AI_MATKEY_COLOR_EMISSIVE, ke);
			mat->Get(AI_MATKEY_OPACITY, opacity);
			mat->Get(AI_MATKEY_SHININESS, shininess);

			d.baseColor = { kd.r, kd.g, kd.b };
			d.emissive = { ke.r, ke.g, ke.b };
			d.opacity = opacity;

			if (shininess > 0.0f) {
				float r = std::sqrt(2.0f / (shininess + 2.0f));
				d.roughness = std::clamp(r, 0.0f, 1.0f);
			}

			SPD_WARNING("Material Name: " << d.name);

			aiString p;
			if (GetTexturePath(mat, aiTextureType_DIFFUSE, p)) {
				std::filesystem::path texName = std::filesystem::path(p.C_Str()).filename();
				SPD_DEBUG("AlbedoMap: " << texName.string());
				std::filesystem::path albedoMapPath = (mandatoryTexDir / texName).lexically_normal();
				if (std::filesystem::exists(albedoMapPath)) {
					AssetManager::GetInstance().GenerateMetadata(albedoMapPath.string());
					auto rec = AssetManager::GetInstance().
						GetRecordBySource(albedoMapPath.string());

					auto asset = dynamic_cast<TextureAsset*>(rec->asset.get());

					asset->SaveImportSettings(albedoMapPath.string());
					d.albedoMapUUID = AssetManager::GetInstance().RetrieveUUID(albedoMapPath.string());

					if (!rec->isLoaded) {
						asset->Cook(albedoMapPath.string(), 
							NE::Resource::ComputeArtifactPathFromUUID(d.albedoMapUUID, GetResourceTypeFromAssetType(Assets::AssetType::Texture)));
						rec->isLoaded = true;
					}
				}
			}

			if (GetTexturePath(mat, aiTextureType_NORMALS, p)) {
				std::filesystem::path texName = std::filesystem::path(p.C_Str()).filename();
				SPD_DEBUG("NormalMap: " << texName.string());
				std::filesystem::path normalMapPath = (mandatoryTexDir / texName).lexically_normal();
				if (std::filesystem::exists(normalMapPath)) {
					AssetManager::GetInstance().GenerateMetadata(normalMapPath.string());
					auto rec = AssetManager::GetInstance().
						GetRecordBySource(normalMapPath.string());

					auto asset = dynamic_cast<TextureAsset*>(rec->asset.get());

					asset->GetImportSettings(normalMapPath.string()).type = Assets::TexType::NormalMap;
					asset->SaveImportSettings(normalMapPath.string());

					d.normalMapUUID = AssetManager::GetInstance().RetrieveUUID(normalMapPath.string());

					if (!rec->isLoaded) {
						asset->Cook(normalMapPath.string(),
							NE::Resource::ComputeArtifactPathFromUUID(d.normalMapUUID, GetResourceTypeFromAssetType(Assets::AssetType::Texture)));
						rec->isLoaded = true;
					}
				}
			}
			//else if (GetTexturePath(mat, aiTextureType_HEIGHT, p)) {
			//	d.normalMap = (mandatoryTexDir / p.C_Str()).lexically_normal();

			//}

			if (GetTexturePath(mat, aiTextureType_EMISSIVE, p)) {
				std::filesystem::path texName = std::filesystem::path(p.C_Str()).filename();
				SPD_DEBUG("EmissiveMap: " << texName.string());
				std::filesystem::path emissionMapPath = (mandatoryTexDir / texName).lexically_normal();
				if (std::filesystem::exists(emissionMapPath)) {
					AssetManager::GetInstance().GenerateMetadata(emissionMapPath.string());
					auto rec = AssetManager::GetInstance().
						GetRecordBySource(emissionMapPath.string());

					auto asset = dynamic_cast<TextureAsset*>(rec->asset.get());

					asset->SaveImportSettings(emissionMapPath.string());
					d.emissionMapUUID = AssetManager::GetInstance().RetrieveUUID(emissionMapPath.string());

					if (!rec->isLoaded) {
						asset->Cook(emissionMapPath.string(),
							NE::Resource::ComputeArtifactPathFromUUID(d.emissionMapUUID, GetResourceTypeFromAssetType(Assets::AssetType::Texture)));
						rec->isLoaded = true;
					}
				}
			}
			if (GetTexturePath(mat, aiTextureType_OPACITY, p)) {
				std::filesystem::path texName = std::filesystem::path(p.C_Str()).filename();
				SPD_DEBUG("OpacityMap: " << texName.string());
				std::filesystem::path opacityMapPath = (mandatoryTexDir / texName).lexically_normal();
				if (std::filesystem::exists(opacityMapPath)) {
					AssetManager::GetInstance().GenerateMetadata(opacityMapPath.string());
					auto rec = AssetManager::GetInstance().
						GetRecordBySource(opacityMapPath.string());

					auto asset = dynamic_cast<TextureAsset*>(rec->asset.get());

					asset->GetImportSettings(opacityMapPath.string()).sRGB = false;
					asset->SaveImportSettings(opacityMapPath.string());

					d.opacityMapUUID = AssetManager::GetInstance().RetrieveUUID(opacityMapPath.string());

					if (!rec->isLoaded) {
						asset->Cook(opacityMapPath.string(),
							NE::Resource::ComputeArtifactPathFromUUID(d.opacityMapUUID, GetResourceTypeFromAssetType(Assets::AssetType::Texture)));
						rec->isLoaded = true;
					}
				}
			}

			if (GetTexturePath(mat, aiTextureType_METALNESS, p)) {
				std::filesystem::path texName = std::filesystem::path(p.C_Str()).filename();
				SPD_DEBUG("MetallicMap: " << texName.string());
				std::filesystem::path metallicMapPath = (mandatoryTexDir / texName).lexically_normal();
				if (std::filesystem::exists(metallicMapPath)) {
					AssetManager::GetInstance().GenerateMetadata(metallicMapPath.string());
					auto rec = AssetManager::GetInstance().
						GetRecordBySource(metallicMapPath.string());

					auto asset = dynamic_cast<TextureAsset*>(rec->asset.get());

					asset->GetImportSettings(metallicMapPath.string()).sRGB = false;
					asset->SaveImportSettings(metallicMapPath.string());
					
					d.metallicMapUUID = AssetManager::GetInstance().RetrieveUUID(metallicMapPath.string());

					if (!rec->isLoaded) {
						asset->Cook(metallicMapPath.string(),
							NE::Resource::ComputeArtifactPathFromUUID(d.metallicMapUUID, GetResourceTypeFromAssetType(Assets::AssetType::Texture)));
						rec->isLoaded = true;
					}
				}
			}
			if (GetTexturePath(mat, aiTextureType_DIFFUSE_ROUGHNESS, p)) {
				std::filesystem::path texName = std::filesystem::path(p.C_Str()).filename();
				SPD_DEBUG("RoughnessMap: " << texName.string());
				std::filesystem::path roughnessMapPath = (mandatoryTexDir / texName).lexically_normal();
				if (std::filesystem::exists(roughnessMapPath)) {
					AssetManager::GetInstance().GenerateMetadata(roughnessMapPath.string());
					auto rec = AssetManager::GetInstance().
						GetRecordBySource(roughnessMapPath.string());

					auto asset = dynamic_cast<TextureAsset*>(rec->asset.get());

					asset->GetImportSettings(roughnessMapPath.string()).sRGB = false;
					asset->SaveImportSettings(roughnessMapPath.string());

					d.roughnessMapUUID = AssetManager::GetInstance().RetrieveUUID(roughnessMapPath.string());

					if (!rec->isLoaded) {
						asset->Cook(roughnessMapPath.string(),
							NE::Resource::ComputeArtifactPathFromUUID(d.roughnessMapUUID, GetResourceTypeFromAssetType(Assets::AssetType::Texture)));
						rec->isLoaded = true;
					}
				}
			}
			if (GetTexturePath(mat, aiTextureType_AMBIENT_OCCLUSION, p)) {
				std::filesystem::path texName = std::filesystem::path(p.C_Str()).filename();
				SPD_DEBUG("AOMap: " << texName.string());
				std::filesystem::path ambientOcclusionMapPath = (mandatoryTexDir / texName).lexically_normal();
				if (std::filesystem::exists(ambientOcclusionMapPath)) {
					AssetManager::GetInstance().GenerateMetadata(ambientOcclusionMapPath.string());
					auto rec = AssetManager::GetInstance().
						GetRecordBySource(ambientOcclusionMapPath.string());

					auto asset = dynamic_cast<TextureAsset*>(rec->asset.get());

					asset->GetImportSettings(ambientOcclusionMapPath.string()).sRGB = false;
					asset->SaveImportSettings(ambientOcclusionMapPath.string());

					d.ambientOcclusionMapUUID = AssetManager::GetInstance().RetrieveUUID(ambientOcclusionMapPath.string());

					if (!rec->isLoaded) {
						asset->Cook(ambientOcclusionMapPath.string(),
							NE::Resource::ComputeArtifactPathFromUUID(d.ambientOcclusionMapUUID, GetResourceTypeFromAssetType(Assets::AssetType::Texture)));
						rec->isLoaded = true;
					}
				}
			}

			return d;
		}

		std::string SanitizeFileName(std::string s) {
			const std::string illegal = "\\/:*?\"<>|";
			for (char& c : s) {
				if (illegal.find(c) != std::string::npos) c = '_';
			}
			while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
			if (s.empty()) s = "Material";
			return s;
		}

		std::string ImportMaterials(const std::string& materialPath, ImportedMatDesc& desc) {
			namespace fs = std::filesystem;
			
			fs::path targetDir = materialPath + "/Materials";
			fs::create_directories(targetDir);

			std::string safe = SanitizeFileName(desc.name);
			fs::path matPath = targetDir / (safe + ".nanomat");

			rapidjson::Document doc;
			doc.SetObject();
			auto& alloc = doc.GetAllocator();

			doc.AddMember("Shader", rapidjson::Value("nelitpbr", alloc), alloc);

			doc.AddMember("DepthTest", true, alloc);
			doc.AddMember("BlendMode", true, alloc);

			doc.AddMember("CullMode", 1029, alloc);
			doc.AddMember("PolygonMode", 6914, alloc);

			doc.AddMember("RenderQueueBase", rapidjson::Value("Geometry", alloc), alloc);
			doc.AddMember("RenderQueueOffset", 0, alloc);

			rapidjson::Value props(rapidjson::kObjectType);
			doc.AddMember("Properties", props, alloc);

			rapidjson::StringBuffer buffer;
			rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);

			std::ofstream out(matPath);
			if (out.is_open()) {
				out << buffer.GetString();
				out.close();
			}

			Assets::AssetManager::GetInstance().GenerateMetadata(matPath.string()); // let it cook first

			MaterialEditor matEd;
			matEd.LoadMaterial(matPath.string(),
				Assets::AssetManager::GetInstance().RetrieveUUID(matPath.string()));

			matEd.SetShader("nelitpbr");

			auto mat = matEd.GetMaterial();

			mat->SetUniformVec3("u_BaseColor", desc.baseColor);
			mat->SetUniformFloat("u_Metallic", desc.metallic);
			mat->SetUniformFloat("u_Roughness", desc.roughness);
			mat->SetUniformFloat("u_Opacity", desc.opacity);
			mat->SetUniformVec3("u_Emissive", desc.emissive);

			mat->SetTexture("u_AlbedoMap", desc.albedoMapUUID);
			mat->SetUniformInt("h_HasAlbedoMap", desc.albedoMapUUID != "" ? 1 : 0);

			mat->SetTexture("u_NormalMap", desc.normalMapUUID);
			mat->SetUniformInt("h_HasNormalMap", desc.normalMapUUID != "" ? 1 : 0);

			mat->SetTexture("u_RoughnessMap", desc.roughnessMapUUID);
			mat->SetUniformInt("h_HasRoughnessMap", desc.roughnessMapUUID != "" ? 1 : 0);

			mat->SetTexture("u_MetallicMap", desc.metallicMapUUID);
			mat->SetUniformInt("h_HasMetallicMap", desc.metallicMapUUID != "" ? 1 : 0);

			mat->SetTexture("u_AmbientOcclusion", desc.ambientOcclusionMapUUID);
			mat->SetUniformInt("h_HasAmbientOcclusionMap", desc.ambientOcclusionMapUUID != "" ? 1 : 0);

			matEd.Save();

			return Assets::AssetManager::GetInstance().RetrieveUUID(matPath.string());
		}
	}

	bool ModelAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
		std::filesystem::path out = outPath;
		std::filesystem::create_directories(out.parent_path());

		std::filesystem::path srcPath = sourcePath;

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(sourcePath,
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_FlipUVs |
			aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace);

		if (!scene || !scene->HasMeshes()) {
			std::fprintf(stderr, "[CookMesh] Failed to load %s\n", sourcePath.c_str());
			return false;
		}

		// one desc per mesh
		std::vector<NE::Resource::NanoSubmeshDesc> subdescs(scene->mNumMeshes);

		// temp storage for actual vertex/index bytes
		struct RawBlob {
			std::vector<uint8_t> vertices;
			std::vector<uint8_t> indices;
		};
		std::vector<RawBlob> blobs(scene->mNumMeshes);

		Bounds globalB;
		const float scale = importSettings ? importSettings->scene.scaleFactor : GuessScaleFactorFromExtension(srcPath.extension().string());

		for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
			const aiMesh* mesh = scene->mMeshes[m];
			Bounds subB;

			RawBlob& rb = blobs[m];
			rb.vertices.resize(mesh->mNumVertices * sizeof(CookVertex));
			auto* vout = reinterpret_cast<CookVertex*>(rb.vertices.data());

			for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
				CookVertex v{};
				v.px = mesh->mVertices[i].x * scale;
				v.py = mesh->mVertices[i].y * scale;
				v.pz = mesh->mVertices[i].z * scale;

				if (mesh->HasNormals()) {
					v.nx = mesh->mNormals[i].x;
					v.ny = mesh->mNormals[i].y;
					v.nz = mesh->mNormals[i].z;
				} else {
					v.nx = v.ny = v.nz = 0.0f;
				}

				if (mesh->HasTextureCoords(0)) {
					v.u = mesh->mTextureCoords[0][i].x;
					v.v = mesh->mTextureCoords[0][i].y;
				} else {
					v.u = v.v = 0.0f;
				}

				vout[i] = v;

				// expand bounds
				subB.Expand(v.px, v.py, v.pz);
				globalB.Expand(v.px, v.py, v.pz);
			}

			// indices
			std::vector<uint32_t> idx;
			idx.reserve(mesh->mNumFaces * 3);
			for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
				const aiFace& face = mesh->mFaces[f];
				for (unsigned j = 0; j < face.mNumIndices; ++j)
					idx.push_back(face.mIndices[j]);
			}
			rb.indices.resize(idx.size() * sizeof(uint32_t));
			std::memcpy(rb.indices.data(), idx.data(), rb.indices.size());

			subdescs[m].vertexCount = mesh->mNumVertices;
			subdescs[m].indexCount = static_cast<uint32_t>(idx.size());

			WriteBoundsToDesc(subB,
				subdescs[m].aabbMin, subdescs[m].aabbMax,
				subdescs[m].sphereCenter, subdescs[m].sphereRadius);
		}

		std::ofstream ofs(outPath, std::ios::binary);
		if (!ofs) return false;

		NE::Resource::NanoMeshHeader header{};
		header.submeshCount = static_cast<uint16_t>(scene->mNumMeshes);
		WriteBoundsToDesc(globalB,
			header.aabbMin, header.aabbMax,
			header.sphereCenter, header.sphereRadius);

		ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));

		const std::streamoff subTablePos = ofs.tellp();
		ofs.write(reinterpret_cast<const char*>(subdescs.data()),
			subdescs.size() * sizeof(NE::Resource::NanoSubmeshDesc));

		for (size_t m = 0; m < blobs.size(); ++m) {
			auto& rb = blobs[m];

			uint32_t vertexOffset = static_cast<uint32_t>(ofs.tellp());
			ofs.write(reinterpret_cast<const char*>(rb.vertices.data()), rb.vertices.size());

			uint32_t indexOffset = static_cast<uint32_t>(ofs.tellp());
			ofs.write(reinterpret_cast<const char*>(rb.indices.data()), rb.indices.size());

			subdescs[m].vertexDataOffset = vertexOffset;
			subdescs[m].indexDataOffset = indexOffset;
		}

		ofs.seekp(subTablePos, std::ios::beg);
		ofs.write(reinterpret_cast<const char*>(subdescs.data()),
			subdescs.size() * sizeof(NE::Resource::NanoSubmeshDesc));

		std::printf("[CookMesh] wrote %s\n", outPath.c_str());

		using rapidjson::IStreamWrapper;
		using rapidjson::Document;
		std::string metaPath = sourcePath + ".meta";

		rapidjson::Document doc;
		doc.SetObject();

		if (std::filesystem::exists(metaPath)) {
			std::ifstream ifs(metaPath);
			if (!ifs) {
				SPD_WARNING("Failed to open meta file: " << metaPath);
				return false;
			}
			rapidjson::IStreamWrapper isw(ifs);
			doc.ParseStream(isw);
			if (doc.HasParseError() || !doc.IsObject()) {
				SPD_WARNING("Meta parse error, resetting meta object: " << metaPath);
				doc.SetObject();
			}
		}

		auto& alloc = doc.GetAllocator();

		std::vector<std::string> nameByIdx(scene->mNumMeshes);
		GatherSubmeshEntries(scene, scene->mRootNode, "", nameByIdx);

		rapidjson::Value submeshes(rapidjson::kArrayType);
		submeshes.Reserve(scene->mNumMeshes, alloc);

		for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
			rapidjson::Value entry(rapidjson::kObjectType);

			std::string name = nameByIdx[i];
			if (name.empty()) {
				const aiMesh* mesh = scene->mMeshes[i];
				name = (mesh && mesh->mName.length > 0)
					? std::string(mesh->mName.C_Str())
					: ("Mesh" + std::to_string(i));
			}

			entry.AddMember("name", rapidjson::Value(name.c_str(), alloc), alloc);
			entry.AddMember("index", (int)i, alloc);
			submeshes.PushBack(entry, alloc);
		}

		if (doc.HasMember("submeshes"))
			doc["submeshes"].CopyFrom(submeshes, alloc);
		else
			doc.AddMember("submeshes", submeshes, alloc);

		m_uuid = AssetManager::GetInstance().RetrieveUUID(sourcePath);
		m_submeshes.clear();
		m_submeshes.reserve(scene->mNumMeshes);
		for (int i = 0; i < (int)scene->mNumMeshes; ++i) {
			m_submeshes.push_back({ nameByIdx[i], i });
		}
		std::string cookedModelUUID = m_uuid;
		if (cookedModelUUID.empty()) {
			SPD_WARNING("Meta missing UUID for model: " << metaPath);
			return false;
		}

		rapidjson::Value gen(rapidjson::kObjectType);
		gen.AddMember("schemaVersion", 1, alloc);

		rapidjson::Value ents(rapidjson::kArrayType);
		uint64_t next = 1;

		std::vector<std::string> materialUUIDByAssimpMat(scene->mNumMaterials);
		std::filesystem::path dirPath = std::filesystem::path(sourcePath).parent_path();

		for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
			ImportedMatDesc desc = ExtractMaterialFBX(scene->mMaterials[i], dirPath);
			materialUUIDByAssimpMat[i] = ImportMaterials(dirPath.string(), desc);
		}

		NE::Math::Mat4 I; I.SetToIdentity();
		BuildGeneratedPrefabRecursiveFlat(scene,
			scene->mRootNode,
			0,
			I,
			cookedModelUUID,
			materialUUIDByAssimpMat,
			ents,
			alloc,
			next
		);

		//BuildGeneratedPrefabRecursive(
		//	scene,
		//	scene->mRootNode,
		//	0,
		//	cookedModelUUID,
		//	materialUUIDByAssimpMat,
		//	ents,
		//	alloc,
		//	next
		//);

		gen.AddMember("Entities", ents, alloc);

		// 3) Append/replace into doc
		if (doc.HasMember("generatedPrefab"))
			doc["generatedPrefab"].CopyFrom(gen, alloc);
		else
			doc.AddMember("generatedPrefab", gen, alloc);

		// 4) Write meta back
		{
			std::ofstream ofs(metaPath);
			if (!ofs) {
				SPD_WARNING("Failed to write meta file: " << metaPath);
				return false;
			}

			rapidjson::OStreamWrapper osw(ofs);
			rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
			writer.SetIndent(' ', 4);
			doc.Accept(writer);
		}

		return true;
	}

	bool ModelAsset::LoadImportSettings(const std::string& sourcePath) {
		if (importSettings.has_value()) return true;

		using rapidjson::IStreamWrapper;
		using rapidjson::Document;

		std::string metaPath = sourcePath + ".meta";

		if (!std::filesystem::exists(metaPath))
			return false;

		std::ifstream ifs(metaPath);
		if (!ifs) {
			SPD_WARNING("Failed to read meta file: " << metaPath);
			return false;
		}

		IStreamWrapper isw(ifs);
		Document doc;
		doc.ParseStream(isw);

		if (doc.HasParseError() || !doc.IsObject()) {
			SPD_WARNING("Failed to parse JSON in meta file: " << metaPath);
			return false;
		}

		if (!doc.HasMember("modelImport") || !doc["modelImport"].IsObject())
			return true;

		const auto& jSettings = doc["modelImport"];

		if (doc.HasMember("submeshes"))
			ParseSubmeshes(doc["submeshes"]);

		if (!importSettings)
			importSettings.emplace();

		Deserialization::FromJSON(jSettings, *importSettings);

		return true;
	}

	bool ModelAsset::SaveImportSettings(const std::string& sourcePath) {
		using rapidjson::Value;

		std::string metaPath = sourcePath + ".meta";

		rapidjson::Document doc;
		doc.SetObject();

		if (std::filesystem::exists(metaPath)) {
			std::ifstream ifs(metaPath);
			if (ifs) {
				rapidjson::IStreamWrapper isw(ifs);
				doc.ParseStream(isw);
				if (doc.HasParseError() || !doc.IsObject()) {
					doc.SetObject();
				}
			}
		}

		auto& alloc = doc.GetAllocator();

		if (!importSettings) importSettings.emplace();
		auto jSettings = Serialization::ToJSON(*importSettings, alloc);

		if (doc.HasMember("modelImport"))
			doc["modelImport"].CopyFrom(jSettings, alloc);
		else
			doc.AddMember("modelImport", jSettings, alloc);

		if (doc.HasMember("submeshes"))
			ParseSubmeshes(doc["submeshes"]);
		else
			m_submeshes.clear();

		std::ofstream ofs(metaPath);
		if (!ofs) {
			SPD_WARNING("Failed to write meta file: " << metaPath);
			return false;
		}

		rapidjson::OStreamWrapper osw(ofs);
		rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
		writer.SetIndent(' ', 4);
		doc.Accept(writer);

		return true;
	}

	ModelImportSettings& ModelAsset::GetImportSettings() {
		return *importSettings;
	}

	std::vector<ModelAsset::SubmeshEntry>& ModelAsset::GetSubmeshes() {
		return m_submeshes;
	}

	void ModelAsset::ParseSubmeshes(const rapidjson::Value& arr) {
		m_submeshes.clear();
		if (!arr.IsArray()) return;
		m_submeshes.reserve(arr.Size());
		for (auto& v : arr.GetArray()) {
			if (!v.IsObject()) continue;
			SubmeshEntry e{};
			if (v.HasMember("name") && v["name"].IsString()) e.name = v["name"].GetString();
			if (v.HasMember("index") && v["index"].IsInt()) e.index = v["index"].GetInt();
			m_submeshes.push_back(std::move(e));
		}
	}
}