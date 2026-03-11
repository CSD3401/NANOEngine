#include "pch.h"
#include "AssetManager.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <array>
#include <algorithm>
#include <string_view>
#include "UUID.hpp"
#include <ResourceManagement/ResourcePaths.hpp>
#include <glad/glad.h>
#include <Core/SpdLogger.hpp>
#include <Engine.hpp>
#include "../Serialization/JSONReflection.hpp"
#include "Assets/TextureAsset.hpp"
#include "Assets/ModelAsset.hpp"
#include "Assets/MaterialAsset.hpp"
#include "Assets/ShaderAsset.hpp"
#include "Assets/SceneAsset.hpp"
#include "Assets/PrefabAsset.hpp"
#include "Assets/AnimationClipAsset.hpp"
#include "Assets/FontAsset.hpp"
#include "Assets/LightingAsset.hpp"
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>



namespace Editor::Assets {

    namespace {
	    std::string ToLower(std::string s) { 
		    for (auto& c : s) 
			    c = (char)std::tolower((unsigned char)c); 
		    return s;
	    }

        bool LooksLikeUUID(std::string_view s) {
            if (s.size() != 36) return false;
            if (s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-') return false;

            auto isHex = [](char c) {
                return (c >= '0' && c <= '9') ||
                    (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F');
            };

            for (size_t i = 0; i < s.size(); ++i) {
                if (i == 8 || i == 13 || i == 18 || i == 23) continue;
                if (!isHex(s[i])) return false;
            }
            return true;
        }

        bool IsKnownArtifactExtension(std::string_view ext) {
            static constexpr std::array<std::string_view, 11> exts{
                ".ntexbin", ".nmodbin", ".nshdbin", ".nmatbin", ".naudbin",
                ".nfabbin", ".nscebin", ".nancbin", ".nconbin", ".nfntbin",
                ".nlgtbin"
            };

            for (auto e : exts) {
                if (ext == e) return true;
            }
            return false;
        }

        std::string AssetTypeToString(Editor::Assets::AssetType type) {
            switch (type) {
            case Editor::Assets::AssetType::Texture:                return "Texture";
            case Editor::Assets::AssetType::Model:                  return "Model";
            case Editor::Assets::AssetType::Shader:                 return "Shader";
            case Editor::Assets::AssetType::Material:               return "Material";
            case Editor::Assets::AssetType::Audio:                  return "Audio";
            case Editor::Assets::AssetType::Scene:                  return "Scene";
            case Editor::Assets::AssetType::Prefab:                 return "Prefab";
            case Editor::Assets::AssetType::AnimationClip:          return "AnimationClip";
            case Editor::Assets::AssetType::AnimatorController:     return "AnimationController";
            case Editor::Assets::AssetType::Font:                   return "Font";
            case Editor::Assets::AssetType::Lighting:               return "Lighting";
            default:                                                return "Unknown";
            }
        }

        std::unique_ptr<Assets::IAsset> CreateImporterForType(Assets::AssetType type,
            const std::string& uuid, const std::string& filename) {
            switch (type) {
            case Assets::AssetType::Texture:                return std::make_unique<Assets::TextureAsset>();
            case Assets::AssetType::Model:                  return std::make_unique<Assets::ModelAsset>();
            case Assets::AssetType::Material:               return std::make_unique<Assets::MaterialAsset>();
            case Assets::AssetType::Shader:                 return std::make_unique<Assets::ShaderAsset>();
            case Assets::AssetType::Scene:                  return std::make_unique<Assets::SceneAsset>();
            case Assets::AssetType::Prefab:                 return std::make_unique<Assets::PrefabAsset>();
            case Assets::AssetType::AnimationClip:          return std::make_unique<Assets::AnimationClipAsset>();
            case Assets::AssetType::Font:                   return std::make_unique<Assets::FontAsset>();
            case Assets::AssetType::Lighting:               return std::make_unique<Assets::LightingAsset>();
            default:                                        return nullptr;
            }
        }

        NE::Resource::ResourceType GetResourceTypeFromAssetType(Assets::AssetType type) {
            switch (type) {
            case Assets::AssetType::Texture:            return NE::Resource::ResourceType::Texture;
            case Assets::AssetType::Model:              return NE::Resource::ResourceType::Model;
            case Assets::AssetType::Material:           return NE::Resource::ResourceType::Material;
            case Assets::AssetType::Shader:             return NE::Resource::ResourceType::Shader;
            case Assets::AssetType::Scene:              return NE::Resource::ResourceType::Scene;
            case Assets::AssetType::Prefab:             return NE::Resource::ResourceType::Prefab;
            case Assets::AssetType::AnimationClip:      return NE::Resource::ResourceType::AnimationClip;
            case Assets::AssetType::Font:               return NE::Resource::ResourceType::Font;
            case Assets::AssetType::Lighting:           return NE::Resource::ResourceType::Lighting;
            default:                                    return NE::Resource::ResourceType::Unknown;
            }
        }

        std::string CanonicalKey(const std::filesystem::path& p) {
            return std::filesystem::weakly_canonical(p).generic_string();
        }
    }
    
    AssetManager::AssetManager() {
        // Register Builtin
        auto typeIndex = static_cast<size_t>(AssetType::Model);
        auto& meshes = m_assetsByType[typeIndex];

        {
            AssetRecord rec;
            rec.id = "builtin:model/cube";
            rec.type = AssetType::Model;
            rec.sourcePath = "Cube";
            rec.isLoaded = true;
            rec.asset = nullptr;
            m_idByPath[rec.sourcePath.string()] = rec.id;
            m_assetsByID[rec.id] = std::move(rec);
            meshes.push_back({ "Cube",     "builtin:model/cube" });
        }

        {
            AssetRecord rec;
            rec.id = "builtin:model/sphere";
            rec.type = AssetType::Model;
            rec.sourcePath = "Sphere";
            rec.isLoaded = true;
            rec.asset = nullptr;
            m_idByPath[rec.sourcePath.string()] = rec.id;
            m_assetsByID[rec.id] = std::move(rec);
            meshes.push_back({ "Sphere",   "builtin:model/sphere" });
        }

        {
            AssetRecord rec;
            rec.id = "builtin:model/cylinder";
            rec.type = AssetType::Model;
            rec.sourcePath = "Cylinder";
            rec.isLoaded = true;
            rec.asset = nullptr;
            m_idByPath[rec.sourcePath.string()] = rec.id;
            m_assetsByID[rec.id] = std::move(rec);
            meshes.push_back({ "Cylinder", "builtin:model/cylinder" });
        }

        {
            AssetRecord rec;
            rec.id = "builtin:model/capsule";
            rec.type = AssetType::Model;
            rec.sourcePath = "Capsule";
            rec.isLoaded = true;
            rec.asset = nullptr;
            m_idByPath[rec.sourcePath.string()] = rec.id;
            m_assetsByID[rec.id] = std::move(rec);
            meshes.push_back({ "Capsule",  "builtin:model/capsule" });
        }

        {
            AssetRecord rec;
            rec.id = "builtin:model/plane";
            rec.type = AssetType::Model;
            rec.sourcePath = "Plane";
            rec.isLoaded = true;
            rec.asset = nullptr;
            m_idByPath[rec.sourcePath.string()] = rec.id;
            m_assetsByID[rec.id] = std::move(rec);
            meshes.push_back({ "Plane",    "builtin:model/plane" });
        }
        
        typeIndex = static_cast<size_t>(AssetType::Shader);
        auto& shaders = m_assetsByType[typeIndex];

        {
            AssetRecord rec;
            rec.id = "neunlit";
            rec.type = AssetType::Shader;
            rec.sourcePath = "Unlit";
            rec.isLoaded = true;
            rec.asset = nullptr;
            m_idByPath[rec.sourcePath.string()] = rec.id;
            m_assetsByID[rec.id] = std::move(rec);
            shaders.push_back({ "Unlit",     "neunlit" });
        }

        {
            AssetRecord rec;
            rec.id = "nelitpbr";
            rec.type = AssetType::Shader;
            rec.sourcePath = "Lit";
            rec.isLoaded = true;
            rec.asset = nullptr;
            m_idByPath[rec.sourcePath.string()] = rec.id;
            m_assetsByID[rec.id] = std::move(rec);
            shaders.push_back({ "Lit",     "nelitpbr" });
        }

        typeIndex = static_cast<size_t>(AssetType::Material);
        auto& materials = m_assetsByType[typeIndex];

        {
            AssetRecord rec;
            rec.id = "neunlitmat";
            rec.type = AssetType::Material;
            rec.sourcePath = "Unlit Material";
            rec.isLoaded = true;
            rec.asset = nullptr;
            m_idByPath[rec.sourcePath.string()] = rec.id;
            m_assetsByID[rec.id] = std::move(rec);
            materials.push_back({ "Default Unlit",     "neunlitmat" });
        }

        {
            AssetRecord rec;
            rec.id = "nelitmat";
            rec.type = AssetType::Material;
            rec.sourcePath = "Lit Material";
            rec.isLoaded = true;
            rec.asset = nullptr;
            m_idByPath[rec.sourcePath.string()] = rec.id;
            m_assetsByID[rec.id] = std::move(rec);
            materials.push_back({ "Default Lit",     "nelitmat" });
        }
    }

	AssetManager& AssetManager::GetInstance() {
		static AssetManager am;
		return am;
	}

    void AssetManager::GenerateMetadata(const std::string& sourcePath, std::string _uuid) {
        namespace fs = std::filesystem;
        using rapidjson::Document;
        using rapidjson::IStreamWrapper;
        using rapidjson::Value;
        using rapidjson::OStreamWrapper;
        using rapidjson::PrettyWriter;

        fs::path fsSourcePath = sourcePath;
        fs::path metaPath = fsSourcePath;
        metaPath += ".meta";
        
        UUID uuid;
        AssetType type = AssetType::Unknown;

        if (!std::filesystem::is_directory(fsSourcePath) && fs::exists(metaPath)) {
            std::ifstream ifs(metaPath);
            if (!ifs) {
                SPD_WARNING("Failed to read meta file: " << metaPath.string());
                return;
            }

            Document doc;
            IStreamWrapper isw(ifs);
            doc.ParseStream(isw);
            if (doc.HasParseError() || !doc.IsObject()) {
                SPD_WARNING("Failed to parse meta JSON: " << metaPath.string());
                return;
            }

            if (doc.HasMember("uuid") && doc["uuid"].IsString())
                uuid = doc["uuid"].GetString();
            if (doc.HasMember("assetType") && doc["assetType"].IsString())
                type = GetAssetTypeFromString(doc["assetType"].GetString());

            if (uuid.empty())
                uuid = _uuid.empty() ? GenerateUUID() : std::move(_uuid);
            if (type == AssetType::Unknown)
                type = GetAssetTypeFromExtension(fsSourcePath.extension().string());

            AssetRecord& rec = RegisterAsset(uuid, type, fsSourcePath);
            if (!rec.asset) {
                rec.asset = CreateImporterForType(type, uuid, fsSourcePath.filename().string());
            }

            if (fs::is_directory(fsSourcePath)) return;
            
            const auto cookedPath = 
                NE::Resource::ComputeArtifactPathFromUUID(uuid, GetResourceTypeFromAssetType(type));
            if (!fs::exists(cookedPath) && rec.asset) {
                rec.asset->Cook(fsSourcePath.string(), cookedPath);
                rec.isLoaded = true;
            }

            return;
        }

        uuid = _uuid.empty() ? GenerateUUID() : std::move(_uuid);
        type = GetAssetTypeFromExtension(fsSourcePath.extension().string());

        AssetRecord& rec = RegisterAsset(uuid, type, fsSourcePath);

        if (!rec.asset) {
            rec.asset = CreateImporterForType(type, uuid, fsSourcePath.filename().string());
        }

        Document outDoc;
        outDoc.SetObject();
        auto& a = outDoc.GetAllocator();

        outDoc.AddMember("fileFormatVersion", CURRENT_META_SCHEMA_VERSION, a);
        outDoc.AddMember("uuid",
            Value(uuid.c_str(), (rapidjson::SizeType)uuid.size(), a), a);

        std::string typeStr = AssetTypeToString(type);
        outDoc.AddMember("assetType",
            Value(typeStr.c_str(), (rapidjson::SizeType)typeStr.size(), a), a);

        std::string srcStr = fsSourcePath.string();
        outDoc.AddMember("sourcePath",
            Value(srcStr.c_str(), (rapidjson::SizeType)srcStr.size(), a), a);

        std::ofstream ofs(metaPath);
        if (!ofs) {
            SPD_WARNING("Failed to write meta file: " << metaPath.string());
            return;
        }

        if (fs::is_directory(fsSourcePath)) return;

        OStreamWrapper osw(ofs);
        PrettyWriter<OStreamWrapper> writer(osw);
        writer.SetIndent(' ', 4);
        outDoc.Accept(writer);

        if (rec.type != AssetType::Scene && rec.type != AssetType::Prefab) {
            if (rec.asset) {
                rec.asset->SaveImportSettings(sourcePath); // create and save default settings first

                const auto cookedPath =
                    NE::Resource::ComputeArtifactPathFromUUID(uuid, GetResourceTypeFromAssetType(type));
                rec.asset->Cook(fsSourcePath.string(), cookedPath);
                rec.isLoaded = true;
            }
        } else if (rec.type == AssetType::Prefab) {
            dynamic_cast<Assets::PrefabAsset*>(rec.asset.get())->SavePrefab(sourcePath);
        }
    }

    void AssetManager::ReimportAsset(const std::string& sourcePathOrMeta) {
        namespace fs = std::filesystem;
        using rapidjson::Document;
        using rapidjson::IStreamWrapper;

        fs::path fsSourcePath = sourcePathOrMeta;
        fs::path metaPath = sourcePathOrMeta;

        if (metaPath.extension() != ".meta") {
            metaPath = fsSourcePath;
            metaPath += ".meta";
        } else {
            fsSourcePath.replace_extension();
        }

        if (!fs::exists(metaPath)) {
            GenerateMetadata(fsSourcePath.string());
            return;
        }

        std::ifstream ifs(metaPath);
        if (!ifs) {
            SPD_WARNING("Failed to read meta file: " << metaPath.string());
            return;
        }

        IStreamWrapper isw(ifs);
        Document doc;
        doc.ParseStream(isw);

        if (doc.HasParseError() || !doc.IsObject()) {
            SPD_WARNING("Failed to parse meta JSON: " << metaPath.string());
            return;
        }

        UUID uuid;
        if (doc.HasMember("uuid") && doc["uuid"].IsString())
            uuid = doc["uuid"].GetString();

        if (uuid.empty()) {
            GenerateMetadata(fsSourcePath.string());
            return;
        }

        AssetType type = GetAssetTypeFromExtension(fsSourcePath.extension().string());

        AssetRecord& rec = RegisterAsset(uuid, type, fsSourcePath);

        if (!rec.asset) {
            rec.asset = CreateImporterForType(type, uuid, fsSourcePath.filename().string());
            if (!rec.asset) return;
        }

        rec.asset->LoadImportSettings(fsSourcePath.string());
        rec.asset->Cook(fsSourcePath.string(), 
            NE::Resource::ComputeArtifactPathFromUUID(uuid, GetResourceTypeFromAssetType(type)));
        rec.isLoaded = true;
    }

    std::string AssetManager::RetrieveUUID(const std::string& sourcePath) {
        if (auto* rec = GetRecordBySource(sourcePath))
            return rec->id;

        std::filesystem::path metaPath = sourcePath + ".meta";
        std::string uuid;

        if (std::filesystem::exists(metaPath)) {
            std::ifstream ifs(metaPath);
            if (!ifs) {
                SPD_WARNING("Failed to read meta file: " << metaPath.string());
                return std::string();
            }

            rapidjson::IStreamWrapper isw(ifs);
            rapidjson::Document doc;
            doc.ParseStream(isw);

            if (doc.HasParseError() || !doc.IsObject()) {
                SPD_WARNING("Failed to parse meta JSON: " << metaPath.string());
                return std::string();
            }

            if (doc.HasMember("uuid") && doc["uuid"].IsString())
                uuid = doc["uuid"].GetString();
        }
        return uuid;
    }

    std::string AssetManager::RetrieveFilename(const std::string& uuid) {
        if (auto* rec = GetRecord(uuid))
            return rec->sourcePath.string();

        return {};
    }

    AssetRecord* AssetManager::GetRecord(const UUID& id) {
        auto it = m_assetsByID.find(id);
        return (it != m_assetsByID.end()) ? &it->second : nullptr;
    }

    const AssetRecord* AssetManager::GetRecord(const UUID& id) const {
        auto it = m_assetsByID.find(id);
        return (it != m_assetsByID.end()) ? &it->second : nullptr;
    }

    AssetRecord* AssetManager::GetRecordBySource(const std::string& sourcePath) {
        std::filesystem::path p = sourcePath;
        const std::string key = CanonicalKey(p);
        auto itID = m_idByPath.find(key);
        if (itID == m_idByPath.end()) return nullptr;
        return GetRecord(itID->second);
    }

    const AssetRecord* AssetManager::GetRecordBySource(const std::string& sourcePath) const {
        std::filesystem::path p = sourcePath;
        const std::string key = CanonicalKey(p);
        auto itID = m_idByPath.find(key);
        if (itID == m_idByPath.end()) return nullptr;
        return GetRecord(itID->second);
    }

    const std::vector<std::pair<std::string, UUID>>&
        AssetManager::GetAssetsOfType(AssetType type) const {
        const auto idx = static_cast<size_t>(type);
        static const std::vector<std::pair<std::string, UUID>> empty;
        return (idx < m_assetsByType.size())
            ? m_assetsByType[idx]
            : empty;
    }

    AssetRecord& AssetManager::RegisterAsset(
        const UUID& id,
        AssetType type,
        const std::filesystem::path& sourcePath)
    {
        const std::string canonical = CanonicalKey(sourcePath);

        // 1) Main map (UUID -> AssetRecord)
        auto it = m_assetsByID.find(id);
        if (it == m_assetsByID.end()) {
            AssetRecord rec;
            rec.id = id;
            rec.type = type;
            rec.sourcePath = sourcePath;
            rec.isLoaded = false;

            auto [newIt, inserted] = m_assetsByID.emplace(id, std::move(rec));
            it = newIt;
        } else {
            // Update record if it already exists
            it->second.type = type;
            it->second.sourcePath = sourcePath;
        }

        AssetRecord& rec = it->second;

        // 2) Path -> UUID map
        m_idByPath[canonical] = id;

        // 3) Per-type listing for UI
        const auto typeIndex = static_cast<size_t>(type);
        if (typeIndex < m_assetsByType.size()) {
            auto& vec = m_assetsByType[typeIndex];
            const std::string displayName = sourcePath.filename().string();

            // Optional: avoid duplicates if rescanning
            auto exists = std::find_if(vec.begin(), vec.end(),
                [&](const auto& p) { return p.second == id; });
            if (exists == vec.end()) {
                vec.emplace_back(displayName, id);
            }
        }

        return rec;
    }

    void AssetManager::UnregisterAsset(const UUID& id) {
        auto it = m_assetsByID.find(id);
        if (it == m_assetsByID.end())
            return;

        AssetType type = it->second.type;
        std::filesystem::path sourcePath = it->second.sourcePath;

        // Remove from per-type listing
        const auto typeIndex = static_cast<size_t>(type);
        if (typeIndex < m_assetsByType.size()) {
            auto& vec = m_assetsByType[typeIndex];
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [&](const auto& p) { return p.second == id; }),
                vec.end());
        }

        // Remove from path map
        m_idByPath.erase(CanonicalKey(sourcePath));

        // Remove from main map
        m_assetsByID.erase(it);
    }

    void AssetManager::CleanupOrphanArtifacts() {
        namespace fs = std::filesystem;

        const fs::path artifactRoot = std::string(NE::Resource::artifactPath);

        std::error_code ec;
        if (!fs::exists(artifactRoot, ec) || !fs::is_directory(artifactRoot, ec)) {
            return;
        }

        size_t deletedCount = 0;
        std::vector<fs::path> directories;

        fs::recursive_directory_iterator it(
            artifactRoot,
            fs::directory_options::skip_permission_denied,
            ec
        );
        fs::recursive_directory_iterator end;

        for (; it != end; it.increment(ec)) {
            if (ec) {
                SPD_WARNING("Artifact cleanup: traversal error under '" << artifactRoot.string()
                    << "': " << ec.message());
                ec.clear();
                continue;
            }

            const auto& entry = *it;
            const fs::path p = entry.path();

            if (entry.is_directory(ec)) {
                if (!ec) directories.push_back(p);
                ec.clear();
                continue;
            }
            ec.clear();

            if (!entry.is_regular_file(ec)) {
                ec.clear();
                continue;
            }
            ec.clear();

            std::string ext = ToLower(p.extension().string());
            if (!IsKnownArtifactExtension(ext)) {
                continue;
            }

            const std::string uuid = p.stem().string();
            if (!LooksLikeUUID(uuid)) {
                continue;
            }

            AssetRecord* rec = GetRecord(uuid);
            bool isOrphan = (rec == nullptr);

            if (!isOrphan) {
                if (!fs::exists(rec->sourcePath, ec)) {
                    isOrphan = true;
                }
                ec.clear();
            }

            if (!isOrphan) continue;

            if (!fs::remove(p, ec)) {
                SPD_WARNING("Artifact cleanup: failed to delete '" << p.string()
                    << "': " << ec.message());
                ec.clear();
                continue;
            }
            ec.clear();
            ++deletedCount;
        }

        std::sort(directories.begin(), directories.end(),
            [](const fs::path& a, const fs::path& b) {
                return a.native().size() > b.native().size();
            });

        for (const auto& dir : directories) {
            if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
                ec.clear();
                continue;
            }

            if (!fs::is_empty(dir, ec)) {
                ec.clear();
                continue;
            }
            ec.clear();

            if (!fs::remove(dir, ec)) {
                SPD_WARNING("Artifact cleanup: failed to remove empty directory '" << dir.string()
                    << "': " << ec.message());
                ec.clear();
            }
            ec.clear();
        }

        if (deletedCount > 0) {
            SPD_INFO("Artifact cleanup: deleted " << deletedCount << " orphan artifact(s).");
        }
    }

    Assets::AssetType AssetManager::GetAssetTypeFromString(std::string_view extension) {
        std::string e = ToLower(std::string(extension));
        if (e == "texture")                 return Assets::AssetType::Texture;
        else if (e == "model")              return Assets::AssetType::Model;
        else if (e == "shader")             return Assets::AssetType::Shader;
        else if (e == "material")           return Assets::AssetType::Material;
        else if (e == "audio")              return Assets::AssetType::Audio;
        else if (e == "scene")              return Assets::AssetType::Scene;
		else if (e == "prefab")             return Assets::AssetType::Prefab;
        else if (e == "animationclip")      return Assets::AssetType::AnimationClip;
		else if (e == "animatorcontroller") return Assets::AssetType::AnimatorController;
        else if (e == "lighting")           return Assets::AssetType::Lighting;
        else if (e == "folder")             return Assets::AssetType::Folder;
        return Assets::AssetType::Unknown;
    }

    Assets::AssetType AssetManager::GetAssetTypeFromExtension(std::string_view extension) {
		std::string e = ToLower(std::string(extension));
        if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga")  return Assets::AssetType::Texture;
        else if (e == ".fbx" || e == ".obj")                            return Assets::AssetType::Model;
        else if (e == ".nanoshader")                                    return Assets::AssetType::Shader;
        else if (e == ".nanomat")                                       return Assets::AssetType::Material;
        else if (e == ".wav" || e == ".mp3")                            return Assets::AssetType::Audio;
        else if (e == ".scene")                                         return Assets::AssetType::Scene;
        else if (e == ".nfab")                                          return Assets::AssetType::Prefab;
        else if (e == ".nanim")                                         return Assets::AssetType::AnimationClip;
        else if (e == ".ncontroller")                                   return Assets::AssetType::AnimatorController;
        else if (e == ".ttf" || e == ".otf")                            return Assets::AssetType::Font;
        else if (e == ".nlight")                                        return Assets::AssetType::Lighting;
		else if (e == "")                                               return Assets::AssetType::Folder;
		return Assets::AssetType::Unknown;
	}

}
