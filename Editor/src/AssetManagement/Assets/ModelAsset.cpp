#include "ModelAsset.hpp"

#include <filesystem>
#include <fstream>

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

#include "../AssetManager.hpp"
#include "../../Serialization/JSONReflection.hpp"

namespace Editor::Assets {
	namespace {
		const double PI = 3.14159265358979323846;

		struct CookVertex {
			float px, py, pz;
			float nx, ny, nz;
			float u, v;
		};

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
	}

	bool ModelAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
		std::filesystem::path out = outPath;
		std::filesystem::create_directories(out.parent_path());

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

		// bounds
		bool hasBounds = false;
		NE::Math::Vec3 minP(FLT_MAX, FLT_MAX, FLT_MAX);
		NE::Math::Vec3 maxP(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
			const aiMesh* mesh = scene->mMeshes[m];

			RawBlob& rb = blobs[m];
			rb.vertices.resize(mesh->mNumVertices * sizeof(CookVertex));
			auto* vout = reinterpret_cast<CookVertex*>(rb.vertices.data());

			for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
				CookVertex v{};
				v.px = mesh->mVertices[i].x;
				v.py = mesh->mVertices[i].y;
				v.pz = mesh->mVertices[i].z;

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
				minP.x = std::min(minP.x, v.px);
				minP.y = std::min(minP.y, v.py);
				minP.z = std::min(minP.z, v.pz);
				maxP.x = std::max(maxP.x, v.px);
				maxP.y = std::max(maxP.y, v.py);
				maxP.z = std::max(maxP.z, v.pz);
				hasBounds = true;
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
		}

		// compute sphere from AABB
		//NE::Math::Vec3 center(0, 0, 0);
		//float radius = 0.0f;
		//if (hasBounds) {
		//    center = (minP + maxP) * 0.5f;
		//    NE::Math::Vec3 ext = maxP - center;
		//    radius = std::max(std::max(ext.x, ext.y), ext.z);
		//}s

		std::ofstream ofs(outPath, std::ios::binary);
		if (!ofs) return false;

		NE::Resource::NanoMeshHeader header{};
		header.submeshCount = static_cast<uint16_t>(scene->mNumMeshes);

		//header.sphereCenter[0] = center.x;
		//header.sphereCenter[1] = center.y;
		//header.sphereCenter[2] = center.z;
		//header.sphereRadius = radius;

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

		std::vector<std::string> materialUUIDByAssimpMat(scene->mNumMaterials); // empty strings for now

		BuildGeneratedPrefabRecursive(
			scene,
			scene->mRootNode,
			0,
			cookedModelUUID,
			materialUUIDByAssimpMat,
			ents,
			alloc,
			next
		);

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