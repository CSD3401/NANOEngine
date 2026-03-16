#include "pch.h"
#include "ModelAsset.hpp"

#include <filesystem>
#include <fstream>
#include <cstdio>
#include <unordered_map>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <rapidjson/document.h>

#include <Core/SpdLogger.hpp>

#include "../AssetManager.hpp"
#include "../../Serialization/JSONReflection.hpp"
#include "ModelAsset/ModelAssetMaterialImport.hpp"
#include "ModelAsset/ModelAssetMeshCook.hpp"
#include "ModelAsset/ModelAssetMetaIO.hpp"
#include "ModelAsset/ModelAssetPrefabGen.hpp"

namespace Editor::Assets {
	namespace {
		float GuessScaleFactorFromExtension(std::string extension) {
			const std::unordered_map<std::string, float> scaleGuesses = {
				{ ".fbx", 0.01f },
				{ ".obj", 1.0f },
				{ ".dae", 1.0f },
				{ ".gltf", 1.0f },
				{ ".glb", 1.0f }
			};
			const auto it = scaleGuesses.find(extension);
			return it != scaleGuesses.end() ? it->second : 1.0f;
		}
	}

	bool ModelAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
		const std::filesystem::path out = outPath;
		std::filesystem::create_directories(out.parent_path());

		const std::filesystem::path srcPath = sourcePath;

		if (!importSettings) {
			ModelAsset* mutableSelf = const_cast<ModelAsset*>(this);
			mutableSelf->LoadImportSettings(sourcePath);
			if (!importSettings) {
				importSettings.emplace();
				importSettings->scene.scaleFactor = GuessScaleFactorFromExtension(srcPath.extension().string());
			}
		}

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

		const float scale = importSettings->scene.scaleFactor;

		ModelAssetInternal::MeshCookResult meshCookResult;
		if (!ModelAssetInternal::CookModelBinary(
			scene,
			*importSettings,
			scale,
			sourcePath,
			out,
			meshCookResult
		)) {
			return false;
		}

		const std::string metaPath = sourcePath + ".meta";
		rapidjson::Document doc;
		if (!ModelAssetInternal::LoadMetaDocumentForCook(metaPath, doc)) {
			return false;
		}

		ModelAssetInternal::WriteSubmeshesToMeta(doc, scene, meshCookResult.submeshPivots);

		m_uuid = AssetManager::GetInstance().RetrieveUUID(sourcePath);
		ModelAssetInternal::BuildSubmeshCacheFromScene(scene, m_submeshes);

		const std::string cookedModelUUID = m_uuid;
		if (cookedModelUUID.empty()) {
			SPD_WARNING("Meta missing UUID for model: " << metaPath);
			return false;
		}

		ModelAssetInternal::MaterialImportResult materialResult;
		const std::filesystem::path dirPath = srcPath.parent_path();
		if (!ModelAssetInternal::ImportAssimpMaterials(scene, dirPath, materialResult)) {
			return false;
		}

		auto& alloc = doc.GetAllocator();
		rapidjson::Value generatedPrefab(rapidjson::kObjectType);
		ModelAssetInternal::BuildGeneratedPrefab(
			scene,
			cookedModelUUID,
			materialResult.materialUUIDByAssimpMat,
			scale,
			srcPath.stem().string(),
			generatedPrefab,
			alloc
		);
		ModelAssetInternal::UpsertGeneratedPrefab(doc, generatedPrefab);

		return ModelAssetInternal::SaveMetaDocument(metaPath, doc);
	}

	bool ModelAsset::LoadImportSettings(const std::string& sourcePath) {
		if (importSettings.has_value()) return true;

		rapidjson::Document doc;
		const std::string metaPath = sourcePath + ".meta";
		if (!ModelAssetInternal::LoadMetaDocumentStrict(metaPath, doc)) {
			return false;
		}

		if (!doc.HasMember("modelImport") || !doc["modelImport"].IsObject()) {
			return true;
		}

		if (doc.HasMember("submeshes")) {
			ParseSubmeshes(doc["submeshes"]);
		}

		if (!importSettings) {
			importSettings.emplace();
		}

		Deserialization::FromJSON(doc["modelImport"], *importSettings);
		return true;
	}

	bool ModelAsset::SaveImportSettings(const std::string& sourcePath) {
		const std::filesystem::path srcPath = sourcePath;
		const std::string metaPath = sourcePath + ".meta";

		rapidjson::Document doc;
		if (!ModelAssetInternal::LoadMetaDocumentForCook(metaPath, doc)) {
			return false;
		}

		auto& alloc = doc.GetAllocator();

		if (!importSettings) {
			importSettings.emplace();
		}
		importSettings->scene.scaleFactor = GuessScaleFactorFromExtension(srcPath.extension().string());

		auto jSettings = Serialization::ToJSON(*importSettings, alloc);
		if (doc.HasMember("modelImport")) {
			doc["modelImport"].CopyFrom(jSettings, alloc);
		} else {
			doc.AddMember("modelImport", jSettings, alloc);
		}

		if (doc.HasMember("submeshes")) {
			ParseSubmeshes(doc["submeshes"]);
		} else {
			m_submeshes.clear();
		}

		return ModelAssetInternal::SaveMetaDocument(metaPath, doc);
	}

	ModelImportSettings& ModelAsset::GetImportSettings() {
		return *importSettings;
	}

	std::vector<ModelAsset::SubmeshEntry>& ModelAsset::GetSubmeshes() {
		return m_submeshes;
	}

	void ModelAsset::ParseSubmeshes(const rapidjson::Value& arr) {
		ModelAssetInternal::ParseSubmeshes(arr, m_submeshes);
	}
}
