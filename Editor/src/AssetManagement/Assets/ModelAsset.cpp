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
#include <Serialisation/ReflectionJson.hpp>
#include <Math/Vec3.hpp>

#include "../../Serialization/JSONReflection.hpp"

namespace Editor::Assets {
	namespace {
		struct CookVertex {
			float px, py, pz;
			float nx, ny, nz;
			float u, v;
		};
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

		// placeholder submesh table
		const std::streamoff subTablePos = ofs.tellp();
		ofs.write(reinterpret_cast<const char*>(subdescs.data()),
			subdescs.size() * sizeof(NE::Resource::NanoSubmeshDesc));

		// actual data + capture offsets
		for (size_t m = 0; m < blobs.size(); ++m) {
			auto& rb = blobs[m];

			uint32_t vertexOffset = static_cast<uint32_t>(ofs.tellp());
			ofs.write(reinterpret_cast<const char*>(rb.vertices.data()), rb.vertices.size());

			uint32_t indexOffset = static_cast<uint32_t>(ofs.tellp());
			ofs.write(reinterpret_cast<const char*>(rb.indices.data()), rb.indices.size());

			subdescs[m].vertexDataOffset = vertexOffset;
			subdescs[m].indexDataOffset = indexOffset;
		}

		// go back and patch submesh table
		ofs.seekp(subTablePos, std::ios::beg);
		ofs.write(reinterpret_cast<const char*>(subdescs.data()),
			subdescs.size() * sizeof(NE::Resource::NanoSubmeshDesc));

		std::printf("[CookMesh] wrote %s\n", outPath.c_str());
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
}