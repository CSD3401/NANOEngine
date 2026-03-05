#include "pch.h"
#include "ModelAssetMaterialImport.hpp"

#include <array>
#include <cmath>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Core/SpdLogger.hpp>
#include <Graphics/Core/Material.hpp>
#include <Math/Vec3.hpp>
#include <ResourceManagement/ResourcePaths.hpp>

#include "../../AssetManager.hpp"
#include "../../Interfaces/MaterialEditor.hpp"
#include "../TextureAsset.hpp"

namespace Editor::Assets::ModelAssetInternal {
	namespace {
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

		struct TextureSlotConfig {
			aiTextureType type;
			const char* debugLabel;
			bool setSRGBFalse;
			bool markAsNormalMap;
			std::string ImportedMatDesc::*uuidField;
		};

		std::string SanitizeFileName(std::string s) {
			const std::string illegal = "\\/:*?\"<>|";
			for (char& c : s) {
				if (illegal.find(c) != std::string::npos) c = '_';
			}
			while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
			if (s.empty()) s = "Material";
			return s;
		}

		bool GetTexturePath(const aiMaterial* mat, aiTextureType type, aiString& out) {
			if (!mat || mat->GetTextureCount(type) == 0) return false;
			return mat->GetTexture(type, 0, &out) == AI_SUCCESS;
		}

		void ApplyTextureImportOverrides(TextureAsset& asset, const std::string& texturePath, const TextureSlotConfig& cfg) {
			if (!cfg.setSRGBFalse && !cfg.markAsNormalMap) {
				asset.SaveImportSettings(texturePath);
				return;
			}

			auto& settings = asset.GetImportSettings(texturePath);
			if (cfg.setSRGBFalse) {
				settings.sRGB = false;
			}
			if (cfg.markAsNormalMap) {
				settings.type = Assets::TexType::NormalMap;
			}
			asset.SaveImportSettings(texturePath);
		}

		void TryImportTextureSlot(
			const aiMaterial* mat,
			const std::filesystem::path& mandatoryTexDir,
			const TextureSlotConfig& cfg,
			ImportedMatDesc& desc
		) {
			aiString p;
			if (!GetTexturePath(mat, cfg.type, p)) {
				return;
			}

			const std::filesystem::path texName = std::filesystem::path(p.C_Str()).filename();
			SPD_DEBUG(cfg.debugLabel << ": " << texName.string());

			const std::filesystem::path texturePath = (mandatoryTexDir / texName).lexically_normal();
			if (!std::filesystem::exists(texturePath)) {
				return;
			}

			AssetManager::GetInstance().GenerateMetadata(texturePath.string());
			auto* rec = AssetManager::GetInstance().GetRecordBySource(texturePath.string());
			if (!rec || !rec->asset) {
				return;
			}

			auto* asset = dynamic_cast<TextureAsset*>(rec->asset.get());
			if (!asset) {
				return;
			}

			ApplyTextureImportOverrides(*asset, texturePath.string(), cfg);

			std::string textureUUID = AssetManager::GetInstance().RetrieveUUID(texturePath.string());
			desc.*(cfg.uuidField) = textureUUID;

			if (!rec->isLoaded) {
				asset->Cook(
					texturePath.string(),
					NE::Resource::ComputeArtifactPathFromUUID(textureUUID, NE::Resource::ResourceType::Texture)
				);
				rec->isLoaded = true;
			}
		}

		ImportedMatDesc ExtractMaterialFBX(const aiMaterial* mat, const std::filesystem::path& parentDir) {
			ImportedMatDesc desc{};

			const std::filesystem::path mandatoryTexDir = parentDir / "Textures";

			aiString matName;
			if (mat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
				desc.name = matName.C_Str();
			}
			if (desc.name.empty()) {
				desc.name = "Material";
			}

			aiColor3D kd(1, 1, 1), ke(0, 0, 0);
			float opacity = 1.0f;
			float shininess = 0.0f;

			mat->Get(AI_MATKEY_COLOR_DIFFUSE, kd);
			mat->Get(AI_MATKEY_COLOR_EMISSIVE, ke);
			mat->Get(AI_MATKEY_OPACITY, opacity);
			mat->Get(AI_MATKEY_SHININESS, shininess);

			desc.baseColor = { kd.r, kd.g, kd.b };
			desc.emissive = { ke.r, ke.g, ke.b };
			desc.opacity = opacity;

			if (shininess > 0.0f) {
				const float roughness = std::sqrt(2.0f / (shininess + 2.0f));
				desc.roughness = std::clamp(roughness, 0.0f, 1.0f);
			}

			SPD_WARNING("Material Name: " << desc.name);

			const std::array<TextureSlotConfig, 7> slots = { {
				{ aiTextureType_DIFFUSE, "AlbedoMap", false, false, &ImportedMatDesc::albedoMapUUID },
				{ aiTextureType_NORMALS, "NormalMap", false, true, &ImportedMatDesc::normalMapUUID },
				{ aiTextureType_EMISSIVE, "EmissiveMap", false, false, &ImportedMatDesc::emissionMapUUID },
				{ aiTextureType_OPACITY, "OpacityMap", true, false, &ImportedMatDesc::opacityMapUUID },
				{ aiTextureType_METALNESS, "MetallicMap", true, false, &ImportedMatDesc::metallicMapUUID },
				{ aiTextureType_DIFFUSE_ROUGHNESS, "RoughnessMap", true, false, &ImportedMatDesc::roughnessMapUUID },
				{ aiTextureType_AMBIENT_OCCLUSION, "AOMap", true, false, &ImportedMatDesc::ambientOcclusionMapUUID }
			} };

			for (const TextureSlotConfig& slot : slots) {
				TryImportTextureSlot(mat, mandatoryTexDir, slot, desc);
			}

			return desc;
		}

		std::string ImportMaterialAsset(const std::string& materialPath, ImportedMatDesc& desc) {
			namespace fs = std::filesystem;

			fs::path targetDir = materialPath + "/Materials";
			fs::create_directories(targetDir);

			const std::string safe = SanitizeFileName(desc.name);
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

			AssetManager::GetInstance().GenerateMetadata(matPath.string());

			MaterialEditor matEd;
			matEd.LoadMaterial(
				matPath.string(),
				AssetManager::GetInstance().RetrieveUUID(matPath.string())
			);

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
			return AssetManager::GetInstance().RetrieveUUID(matPath.string());
		}
	}

	bool ImportAssimpMaterials(
		const aiScene* scene,
		const std::filesystem::path& sourceDir,
		MaterialImportResult& outResult
	) {
		outResult = {};

		if (!scene) {
			return false;
		}

		outResult.materialUUIDByAssimpMat.resize(scene->mNumMaterials);
		for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
			ImportedMatDesc desc = ExtractMaterialFBX(scene->mMaterials[i], sourceDir);
			outResult.materialUUIDByAssimpMat[i] = ImportMaterialAsset(sourceDir.string(), desc);
		}

		return true;
	}
}
