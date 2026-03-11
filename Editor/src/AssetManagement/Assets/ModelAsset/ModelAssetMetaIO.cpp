#include "pch.h"
#include "ModelAssetMetaIO.hpp"

#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

#include <Core/SpdLogger.hpp>

#include "../../../Serialization/JSONReflection.hpp"
#include "ModelAssetAssimpUtil.hpp"

namespace Editor::Assets::ModelAssetInternal {
	namespace {
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
				if (meshIdx >= scene->mNumMeshes) {
					continue;
				}

				if (!outNameByMeshIdx[meshIdx].empty()) {
					continue;
				}

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

	bool LoadMetaDocumentForCook(const std::string& metaPath, rapidjson::Document& outDoc) {
		outDoc.SetObject();

		if (!std::filesystem::exists(metaPath)) {
			return true;
		}

		std::ifstream ifs(metaPath);
		if (!ifs) {
			SPD_WARNING("Failed to open meta file: " << metaPath);
			return false;
		}

		rapidjson::IStreamWrapper isw(ifs);
		outDoc.ParseStream(isw);
		if (outDoc.HasParseError() || !outDoc.IsObject()) {
			SPD_WARNING("Meta parse error, resetting meta object: " << metaPath);
			outDoc.SetObject();
		}

		return true;
	}

	bool LoadMetaDocumentStrict(const std::string& metaPath, rapidjson::Document& outDoc) {
		if (!std::filesystem::exists(metaPath)) {
			return false;
		}

		std::ifstream ifs(metaPath);
		if (!ifs) {
			SPD_WARNING("Failed to read meta file: " << metaPath);
			return false;
		}

		rapidjson::IStreamWrapper isw(ifs);
		outDoc.ParseStream(isw);

		if (outDoc.HasParseError() || !outDoc.IsObject()) {
			SPD_WARNING("Failed to parse JSON in meta file: " << metaPath);
			return false;
		}

		return true;
	}

	bool SaveMetaDocument(const std::string& metaPath, rapidjson::Document& doc) {
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

	void WriteSubmeshesToMeta(
		rapidjson::Document& doc,
		const aiScene* scene,
		const std::vector<NE::Math::Vec3>& submeshPivots
	) {
		if (!scene) {
			return;
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
			entry.AddMember("index", static_cast<int>(i), alloc);

			NE::Math::Vec3 pivot{};
			if (i < submeshPivots.size()) {
				pivot = submeshPivots[i];
			}
			rapidjson::Value pivotObj = Serialization::ToJSON(pivot, alloc);
			entry.AddMember("pivotOffset", pivotObj, alloc);
			submeshes.PushBack(entry, alloc);
		}

		if (doc.HasMember("submeshes")) {
			doc["submeshes"].CopyFrom(submeshes, alloc);
		} else {
			doc.AddMember("submeshes", submeshes, alloc);
		}
	}

	void BuildSubmeshCacheFromScene(
		const aiScene* scene,
		std::vector<ModelAsset::SubmeshEntry>& outSubmeshes
	) {
		outSubmeshes.clear();
		if (!scene) {
			return;
		}

		std::vector<std::string> nameByIdx(scene->mNumMeshes);
		GatherSubmeshEntries(scene, scene->mRootNode, "", nameByIdx);

		outSubmeshes.reserve(scene->mNumMeshes);
		for (int i = 0; i < static_cast<int>(scene->mNumMeshes); ++i) {
			outSubmeshes.push_back({ nameByIdx[i], i });
		}
	}

	void UpsertGeneratedPrefab(
		rapidjson::Document& doc,
		rapidjson::Value& generatedPrefab
	) {
		auto& alloc = doc.GetAllocator();
		if (doc.HasMember("generatedPrefab")) {
			doc["generatedPrefab"].CopyFrom(generatedPrefab, alloc);
		} else {
			doc.AddMember("generatedPrefab", generatedPrefab, alloc);
		}
	}

	void ParseSubmeshes(
		const rapidjson::Value& arr,
		std::vector<ModelAsset::SubmeshEntry>& outSubmeshes
	) {
		outSubmeshes.clear();
		if (!arr.IsArray()) {
			return;
		}

		outSubmeshes.reserve(arr.Size());
		for (auto& v : arr.GetArray()) {
			if (!v.IsObject()) {
				continue;
			}

			ModelAsset::SubmeshEntry entry{};
			if (v.HasMember("name") && v["name"].IsString()) {
				entry.name = v["name"].GetString();
			}
			if (v.HasMember("index") && v["index"].IsInt()) {
				entry.index = v["index"].GetInt();
			}
			outSubmeshes.push_back(std::move(entry));
		}
	}
}
