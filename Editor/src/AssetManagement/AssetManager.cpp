#include "AssetManager.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "stb_image/stb_image.h"
#include "compressonator/cmp_compressonatorlib/compressonator.h"
#include "UUID.hpp"
#include <ResourceManagement/BinaryHeaders/NanoTexHeader.hpp>
#include <ResourceManagement/BinaryHeaders/NanoShdHeader.hpp>
#include <ResourceManagement/BinaryHeaders/NanoMatHeader.hpp>
#include <ResourceManagement/BinaryHeaders/NanoMeshHeader.hpp>
#include <ResourceManagement/ResourcePaths.hpp>
#include <rapidjson/document.h>
#include <glad/glad.h>
#include <Core/SpdLogger.hpp>
#include <Engine.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <Math/Vec3.hpp>
#include "Settings/ModelImportSettings.hpp"
#include <Serialisation/ReflectionJson.hpp>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>


namespace {
	std::string ToLower(std::string s) { 
		for (auto& c : s) 
			c = (char)std::tolower((unsigned char)c); 
		return s; 
	}

    // Texture Helpers
    static bool CMP_API Progress(float fProgress, CMP_DWORD_PTR, CMP_DWORD_PTR) {
        std::printf("\r[BC7] %3.0f%%", fProgress);
        return false;
    }

    // Heuristic: choose sRGB for typical LDR images
    static bool GuessSRGBFromExt(const std::filesystem::path& p) {
        auto ext = p.extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        return (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga");
    }

    // load image as rgba8
    static bool LoadRGBA8(const std::string& path, std::vector<uint8_t>& rgba, uint32_t& w, uint32_t& h) {
        int x = 0, y = 0, n = 0;
        stbi_uc* data = stbi_load(path.c_str(), &x, &y, &n, 4);
        if (!data) return false;
        w = static_cast<uint32_t>(x);
        h = static_cast<uint32_t>(y);
        rgba.assign(data, data + (size_t)w * h * 4);
        stbi_image_free(data);
        return true;
    }

    //enum class TexShape : uint8_t { D2 = 0, Cube = 1, D3 = 2, D2Array = 3 };
    enum class TexFormat : uint8_t { BC7_UNORM = 0, BC7_UNORM_SRGB = 1, BC5_UNORM = 2 };

    // rgba8 to bc7
    static CMP_ERROR CompressRGBA8ToBC7(const uint8_t* rgba8, uint32_t w, uint32_t h,
        float quality, uint32_t threads,
        std::vector<uint8_t>& outBC7)
    {
        CMP_Texture src{};
        src.dwSize = sizeof(src);
        src.dwWidth = w;
        src.dwHeight = h;
        src.dwPitch = 0; // let SDK compute for uncompressed inputs
        src.format = CMP_FORMAT_RGBA_8888;
        src.dwDataSize = w * h * 4;
        src.pData = (CMP_BYTE*)rgba8;

        CMP_Texture dst{};
        dst.dwSize = sizeof(dst);
        dst.dwWidth = w;
        dst.dwHeight = h;
        dst.dwPitch = 0;
        dst.format = CMP_FORMAT_BC7;
        dst.dwDataSize = CMP_CalculateBufferSize(&dst);
        dst.pData = (CMP_BYTE*)std::malloc(dst.dwDataSize);
        if (!dst.pData) return CMP_ERR_MEM_ALLOC_FOR_MIPSET;

        CMP_CompressOptions opts{};
        opts.dwSize = sizeof(opts);
        opts.fquality = quality;   // 0 to 1
        opts.dwnumThreads = threads;   // 0 = auto

        CMP_ERROR err = CMP_ConvertTexture(&src, &dst, &opts, &Progress);
        if (err == CMP_OK) {
            outBC7.assign(dst.pData, dst.pData + dst.dwDataSize);
        }

        std::free(dst.pData);
        return err;
    }

    static CMP_ERROR CompressRGBA8ToBC5(const uint8_t* rgba8, uint32_t w, uint32_t h,
        float quality, uint32_t threads,
        std::vector<uint8_t>& outBC5)
    {
        CMP_Texture src{};
        src.dwSize = sizeof(src);
        src.dwWidth = w;
        src.dwHeight = h;
        src.dwPitch = 0;
        src.format = CMP_FORMAT_RGBA_8888;
        src.dwDataSize = w * h * 4;
        src.pData = (CMP_BYTE*)rgba8;

        CMP_Texture dst{};
        dst.dwSize = sizeof(dst);
        dst.dwWidth = w;
        dst.dwHeight = h;
        dst.dwPitch = 0;
        dst.format = CMP_FORMAT_BC5;
        dst.dwDataSize = CMP_CalculateBufferSize(&dst);
        dst.pData = (CMP_BYTE*)std::malloc(dst.dwDataSize);
        if (!dst.pData) return CMP_ERR_MEM_ALLOC_FOR_MIPSET;

        CMP_CompressOptions opts{};
        opts.dwSize = sizeof(opts);
        opts.fquality = quality;
        opts.dwnumThreads = threads;

        CMP_ERROR err = CMP_ConvertTexture(&src, &dst, &opts, &Progress);
        if (err == CMP_OK) {
            outBC5.assign(dst.pData, dst.pData + dst.dwDataSize);
        }

        std::free(dst.pData);
        return err;
    }

    static bool WriteNanoTex(const std::string& outPath,
        uint32_t w, uint32_t h,
        bool isSRGB, Editor::TexShape shape, TexFormat fmt,
        uint16_t mipCount, uint16_t layers,
        const std::vector<uint8_t>& payload)
    {
        using namespace NE::Resource;

        NanoTexHeader hdr{};
        hdr.magic = 0x4E544558;
        hdr.importerVersion = CURRENT_NANOTEX_FORMAT_VERSION;
        hdr.width = w;
        hdr.height = h;
        hdr.mipCount = mipCount;
        hdr.layers = layers;
        hdr.isSRGB = isSRGB ? 1u : 0u;
        hdr.shape = static_cast<uint8_t>(shape);
        hdr.format = static_cast<uint8_t>(fmt);

        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs) return false;

        ofs.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        ofs.write(reinterpret_cast<const char*>(payload.data()), (std::streamsize)payload.size());
        return ofs.good();
    }

    // Shader Helpers
    static std::string Trim(const std::string& str) {
        const char* whitespace = " \t\n\r";
        size_t start = str.find_first_not_of(whitespace);
        if (start == std::string::npos)
            return "";
        size_t end = str.find_last_not_of(whitespace);
        return str.substr(start, end - start + 1);
    }

    static GLenum ShaderTypeFromString(std::string& type) {
        type = Trim(type);
        if (type == "vertex") return GL_VERTEX_SHADER;
        if (type == "fragment") return GL_FRAGMENT_SHADER;
        throw std::runtime_error("Unknown shader type: " + type);
    }

    static std::string LoadShaderSource(const std::string& path)
    {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    std::unordered_map<GLenum, std::string> Preprocess(const std::string& source)
    {
        std::unordered_map<GLenum, std::string> shaderSources;

        const std::string typeToken = "#type";
        size_t pos = source.find(typeToken);
        while (pos != std::string::npos) {
            size_t eol = source.find_first_of("\r\n", pos);
            std::string type = source.substr(pos + typeToken.length(), eol - pos - typeToken.length());
            size_t nextLinePos = source.find_first_not_of("\r\n", eol);
            size_t nextTypePos = source.find(typeToken, nextLinePos);
            shaderSources[ShaderTypeFromString(type)] = source.substr(nextLinePos, nextTypePos - nextLinePos);
            pos = nextTypePos;
        }

        return shaderSources;
    }

    struct CookVertex {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    std::string AssetTypeToString(Editor::AssetType type) {
        switch (type) {
        case Editor::AssetType::Texture:    return "Texture";
        case Editor::AssetType::Mesh:       return "Mesh";
        case Editor::AssetType::Shader:     return "Shader";
        case Editor::AssetType::Material:   return "Material";
        case Editor::AssetType::Audio:      return "Audio";
        default:                            return "Unknown";
        }
    }

    bool SaveModelImportSettings(const std::string& metaPath, const Editor::ModelImportSettings& settings) {
        using namespace rapidjson;
        using namespace NE::Serialization;

        Document doc;
        doc.SetObject();

        // Try to load existing meta to preserve other unrelated fields (uuid, assetType, etc.)
        if (std::filesystem::exists(metaPath)) {
            std::ifstream ifs(metaPath);
            if (ifs) {
                IStreamWrapper isw(ifs);
                doc.ParseStream(isw);
                if (doc.HasParseError() || !doc.IsObject()) {
                    doc.SetObject(); // Reset on failure
                }
            }
        }

        auto& alloc = doc.GetAllocator();

        // Serialize settings via reflection
        RJson jSettings = to_json(settings, alloc); // object with { scene, mesh, rig, animation, material }

        // Attach / replace "modelImport" in the root doc
        if (doc.HasMember("modelImport"))
            doc["modelImport"].CopyFrom(jSettings, alloc);
        else
            doc.AddMember("modelImport", jSettings, alloc);

        std::ofstream ofs(metaPath);
        if (!ofs) {
            SPD_WARNING("Failed to write meta file: " << metaPath);
            return false;
        }

        OStreamWrapper osw(ofs);
        PrettyWriter<OStreamWrapper> writer(osw);
        writer.SetIndent(' ', 4);
        doc.Accept(writer);

        return true;
    }
}

namespace Editor {
    
    AssetManager::AssetManager() {
        // Register Builtin
        GetAssetsOfType<AssetType::Mesh>().push_back({ "Cube", "builtin:model/cube" });
        GetAssetsOfType<AssetType::Mesh>().push_back({ "Plane", "builtin:model/plane" });
        GetAssetsOfType<AssetType::Mesh>().push_back({ "Cylinder", "builtin:model/cylinder" });
        GetAssetsOfType<AssetType::Mesh>().push_back({ "Sphere", "builtin:model/sphere" });
        GetAssetsOfType<AssetType::Mesh>().push_back({ "Capsule", "builtin:model/capsule" });

        for (const auto& [name, uuid] : GetAssetsOfType<AssetType::Mesh>()) {
            m_assets[uuid] = { uuid, AssetType::Mesh, name };
        }
    }

	AssetManager& AssetManager::GetInstance() {
		static AssetManager am;
		return am;
	}

    void AssetManager::GenerateMetadata(const std::string& sourcePath) {
        namespace fs = std::filesystem;
        using namespace rapidjson;
        using namespace NE::Serialization;

        fs::path fsSourcePath = sourcePath;
        fs::path metaPath = sourcePath + ".meta";

        AssetMetadata metadata{};
        metadata.sourcePath = sourcePath;

        if (fs::exists(metaPath)) {
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

            if (doc.HasMember("uuid") && doc["uuid"].IsString())
                metadata.uuid = doc["uuid"].GetString();
            if (doc.HasMember("assetType") && doc["assetType"].IsString())
                metadata.type = GetAssetTypeFromString(doc["assetType"].GetString());
            if (doc.HasMember("sourcePath") && doc["sourcePath"].IsString())
                metadata.sourcePath = doc["sourcePath"].GetString();

            if (metadata.uuid.empty())
                metadata.uuid = GenerateUUID();
            if (metadata.type == AssetType::Unknown)
                metadata.type = GetAssetTypeFromExtension(fsSourcePath.extension().string());

            switch (metadata.type) {
            case AssetType::Texture:
                GetAssetsOfType<AssetType::Texture>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
                break;
            case AssetType::Mesh:
                GetAssetsOfType<AssetType::Mesh>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
                break;
            case AssetType::Shader:
                GetAssetsOfType<AssetType::Shader>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
                break;
            case AssetType::Material:
                GetAssetsOfType<AssetType::Material>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
                break;
            case AssetType::Audio:
                GetAssetsOfType<AssetType::Audio>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
                break;
            default:
                break;
            }

            m_assets[metadata.uuid] = std::move(metadata);
            return;
        }

        std::string uuid = GenerateUUID();
        std::string outPath = NE::Resource::ComputeArtifactPathFromUUID(uuid);

        AssetType assetType = GetAssetTypeFromExtension(fsSourcePath.extension().string());
        metadata.uuid = uuid;
        metadata.type = assetType;

        Document doc;
        doc.SetObject();
        auto& alloc = doc.GetAllocator();

        doc.AddMember("fileFormatVersion", CURRENT_META_SCHEMA_VERSION, alloc);
        doc.AddMember("uuid", Value(uuid.c_str(), (rapidjson::SizeType)uuid.size(), alloc), alloc);
        std::string assetTypeStr = AssetTypeToString(assetType);
        doc.AddMember("assetType", Value(assetTypeStr.c_str(), (rapidjson::SizeType)assetTypeStr.size(), alloc), alloc);
        doc.AddMember("sourcePath", Value(sourcePath.c_str(), (rapidjson::SizeType)sourcePath.size(), alloc), alloc);

        switch (assetType) {
        case AssetType::Texture: {
            TextureImportSettings defaultSettings{};
            CookTexture(sourcePath, outPath, defaultSettings);

            GetAssetsOfType<AssetType::Texture>().push_back({ fsSourcePath.filename().string(), metadata.uuid });

            doc.AddMember("textureImporterVersion", TEXTURE_IMPORTER_VERSION, alloc);

            RJson texImportJson = to_json(defaultSettings, alloc);
            doc.AddMember("textureImport", texImportJson, alloc);
            break;
        }
        case AssetType::Mesh: {
            ModelImportSettings defaultSettings{};
            CookMesh(sourcePath, outPath);

            GetAssetsOfType<AssetType::Mesh>().push_back({ fsSourcePath.filename().string(), metadata.uuid });

            doc.AddMember("modelImporterVersion", MODEL_IMPORTER_VERSION, alloc);
            RJson modelImportJson = to_json(defaultSettings, alloc);
            doc.AddMember("modelImport", modelImportJson, alloc);
            break;
        }
        case AssetType::Shader: {
            CookShader(sourcePath, outPath);
            GetAssetsOfType<AssetType::Shader>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
            break;
        }
        case AssetType::Material: {
            CookMaterial(sourcePath, outPath);
            GetAssetsOfType<AssetType::Material>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
            break;
        }
        case AssetType::Audio: {
            // TODO: audio import settings later
            break;
        }
        default:
            break;
        }


        {
            std::ofstream ofs(metaPath);
            if (!ofs) {
                SPD_WARNING("Failed to write meta file: " << metaPath.string());
            } else {
                OStreamWrapper osw(ofs);
                PrettyWriter<OStreamWrapper> writer(osw);
                writer.SetIndent(' ', 4);
                doc.Accept(writer);
            }
        }

        m_assets[uuid] = std::move(metadata);
    }

    void AssetManager::ReimportAsset(const std::string& sourcePath) {
        std::filesystem::path fsSourcePath = sourcePath;
        std::filesystem::path metaPath = sourcePath + ".meta";

        AssetMetadata metadata{};
        metadata.sourcePath = sourcePath;

        TextureImportSettings texSettings{};
        if (std::filesystem::exists(metaPath)) {
            std::ifstream ifs(metaPath);

            std::string line;
            while (std::getline(ifs, line)) {
                if (line.starts_with("uuid:")) {
                    metadata.uuid = line.substr(line.find(':') + 1);
                    metadata.uuid.erase(0, metadata.uuid.find_first_not_of(" \t"));
                } else if (line.starts_with("assetType:")) {
                    std::string typeStr = line.substr(line.find(':') + 1);
                    typeStr.erase(0, typeStr.find_first_not_of(" \t"));
                    metadata.type = GetAssetTypeFromString(typeStr);
                } else if (line.starts_with("sourcePath:")) {
                    metadata.sourcePath = line.substr(line.find(':') + 1);
                    metadata.sourcePath.erase(0, metadata.sourcePath.find_first_not_of(" \t"));
                } else if (line.rfind("textureType:", 0) == 0) {
                    std::string texStr = line.substr(line.find(':') + 1);
                    texStr.erase(0, texStr.find_first_not_of(" \t"));
                    if (!texStr.empty()) {
                        int t = std::stoi(texStr); // 0 = Default, 1 = Normal, ...
                        texSettings.type =
                            static_cast<Editor::TexType>(t);
                    }
                }
            }
        }

        std::string outPath = NE::Resource::ComputeArtifactPathFromUUID(metadata.uuid);

        AssetType assetType = GetAssetTypeFromExtension(fsSourcePath.extension().string());
        switch (assetType) {
        case AssetType::Texture: {


            CookTexture(sourcePath, outPath, texSettings);


            break;
        }
        case AssetType::Mesh: {
            CookMesh(sourcePath, outPath);
            break;
        }
        case AssetType::Shader: {
            CookShader(sourcePath, outPath);
            break;
        }
        case AssetType::Material: {
            CookMaterial(sourcePath, outPath);
            break;
        }
        case AssetType::Audio: {

            break;
        }
        default:
            break;
        }

        m_assets[metadata.uuid] = std::move(metadata);
    }

    std::string AssetManager::RetrieveUUID(const std::string& sourcePath) {
        std::filesystem::path metaPath = sourcePath + ".meta";
        std::string uuid;

        if (std::filesystem::exists(metaPath)) {
            std::ifstream ifs(metaPath);
            if (!ifs) {
                SPD_WARNING("Failed to read meta file: " << metaPath.string());
                return "";
            }

            std::string line;
            while (std::getline(ifs, line)) {
                if (line.starts_with("uuid:")) {
                    uuid = line.substr(line.find(':') + 1);
                    uuid.erase(0, uuid.find_first_not_of(" \t"));
                    break;
                }
            }

        }
        return uuid;
    }

    std::string AssetManager::RetrieveFileName(const std::string& uuid) {

        if (m_assets.contains(uuid))
            return m_assets.at(uuid).sourcePath;

        return "";
    }

    AssetType AssetManager::GetAssetTypeFromString(std::string_view extension) {
        std::string e = ToLower(std::string(extension));
        if (e == "Texture") return AssetType::Texture;
        else if (e == "Mesh") return AssetType::Mesh;
        else if (e == "Shader") return AssetType::Shader;
        else if (e == "Material") return AssetType::Material;
        else if (e == "Audio") return AssetType::Audio;
        return AssetType::Unknown;
    }

	AssetType AssetManager::GetAssetTypeFromExtension(std::string_view extension) {
		std::string e = ToLower(std::string(extension));
        if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga") return AssetType::Texture;
        else if (e == ".fbx" || e == ".obj") return AssetType::Mesh;
        else if (e == ".nanoshader") return AssetType::Shader;
        else if (e == ".nanomat") return AssetType::Material;
        else if (e == ".wav" || e == ".mp3") return AssetType::Audio;
		return AssetType::Unknown;
	}

    bool AssetManager::CookTexture(const std::string& sourcePath,
        const std::string& outPath,
        const TextureImportSettings& settings)
    {
        std::filesystem::path src = sourcePath;
        std::filesystem::path out = outPath;
        std::filesystem::create_directories(out.parent_path());

        // Decide sRGB based on type:
        //  - Color textures -> heuristic
        //  - Normal maps    -> ALWAYS linear (no sRGB)
        bool srgb = GuessSRGBFromExt(src);
        const bool isNormalMap = (settings.type == TexType::NormalMap);
        if (isNormalMap) {
            srgb = false;
        }

        std::vector<uint8_t> rgba8;
        uint32_t w = 0, h = 0;
        if (!LoadRGBA8(src.string(), rgba8, w, h)) {
            std::fprintf(stderr, "[CookTexture] Failed to load: %s\n", src.string().c_str());
            return false;
        }

        std::vector<uint8_t> compressed;
        TexFormat fmt = TexFormat::BC7_UNORM;

        const float    quality = 0.6f;
        const uint32_t threads = 0;

        if (isNormalMap) {
            if (CMP_ERROR err = CompressRGBA8ToBC5(rgba8.data(), w, h, quality, threads, compressed);
                err != CMP_OK) {
                std::fprintf(stderr, "[CookTexture] BC5 compress error %d on: %s\n",
                    (int)err, src.string().c_str());
                return false;
            }
            fmt = TexFormat::BC5_UNORM;
            std::printf("[CookTexture] Using BC5 for normal map: %s\n", src.string().c_str());
        } else {
            if (CMP_ERROR err = CompressRGBA8ToBC7(rgba8.data(), w, h, quality, threads, compressed);
                err != CMP_OK) {
                std::fprintf(stderr, "[CookTexture] BC7 compress error %d on: %s\n",
                    (int)err, src.string().c_str());
                return false;
            }
            fmt = srgb ? TexFormat::BC7_UNORM_SRGB : TexFormat::BC7_UNORM;
        }

        const uint16_t mipCount = 1;
        const uint16_t layers = 1;
        const TexShape shape = TexShape::TwoD;

        if (!WriteNanoTex(out.string(), w, h, srgb, shape, fmt, mipCount, layers, compressed)) {
            std::fprintf(stderr, "[CookTexture] Failed to write: %s\n", out.string().c_str());
            return false;
        }

        std::printf("\n[CookTexture] OK: %s -> %s (%ux%u, %zu bytes, %s)\n",
            src.string().c_str(), out.string().c_str(), w, h, compressed.size(),
            isNormalMap ? "BC5" : (srgb ? "BC7 sRGB" : "BC7 UNORM"));
        return true;
    }


    bool AssetManager::CookShader(const std::string& sourcePath, const std::string& outPath) {
        std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());

        const std::string source = LoadShaderSource(sourcePath);
        auto shaderStages = Preprocess(source);

        return NE::CookShader(sourcePath, outPath, shaderStages);
    }

    bool AssetManager::CookMaterial(const std::string& sourcePath, const std::string& outPath) {
        std::filesystem::path out = outPath;
        std::filesystem::create_directories(out.parent_path());

        std::ifstream in(sourcePath);
        if (!in) return false;
        std::string j((std::istreambuf_iterator<char>(in)), {});
        rapidjson::Document doc; doc.Parse(j.c_str());
        if (!doc.IsObject()) return false;

        NE::Resource::NanoMatHeader h{};
        h.depthTest = doc.HasMember("DepthTest") ? (doc["DepthTest"].GetBool() ? 1 : 0) : 1;
        h.blendMode = doc.HasMember("BlendMode") ? (doc["BlendMode"].GetBool() ? 1 : 0) : 0;
        h.cullMode = doc.HasMember("CullMode") ? doc["CullMode"].GetUint() : 0;
        h.polygonMode = doc.HasMember("PolygonMode") ? doc["PolygonMode"].GetUint() : 0;

        const char* shaderName = doc.HasMember("Shader") ? doc["Shader"].GetString() : "Basic";
        const uint32_t shaderNameLen = (uint32_t)std::strlen(shaderName);

        // Tables to build
        std::vector<NE::Resource::MatPropRecord> propRecs;
        std::vector<NE::Resource::MatTexRecord>  texRecs;

        std::string strings; // shared string blob (for BOTH prop names and tex names)
        std::string payload; // only for prop data (ints/floats/matrices)

        if (doc.HasMember("Properties") && doc["Properties"].IsObject()) {
            for (auto it = doc["Properties"].MemberBegin(); it != doc["Properties"].MemberEnd(); ++it) {
                const std::string name = it->name.GetString();
                const auto& v = it->value;

                // case A: numeric / array -> goes to prop table
                if (v.IsInt() || v.IsNumber() ||
                    (v.IsArray() && (v.Size() == 3 || v.Size() == 16))) {

                    NE::Resource::MatPropRecord r{};
                    r.nameLen = (uint32_t)name.size();
                    r.count = 1;

                    size_t dataStart = payload.size();
                    if (v.IsInt()) {
                        int32_t x = v.GetInt();
                        r.type = (uint8_t)NE::Resource::MatPropType::INT;
                        payload.append(reinterpret_cast<const char*>(&x), sizeof(x));
                    } else if (v.IsNumber()) {
                        float f = (float)v.GetDouble();
                        r.type = (uint8_t)NE::Resource::MatPropType::FLOAT;
                        payload.append(reinterpret_cast<const char*>(&f), sizeof(f));
                    } else if (v.IsArray() && v.Size() == 3) {
                        float f[3] = {
                            (float)v[0].GetDouble(),
                            (float)v[1].GetDouble(),
                            (float)v[2].GetDouble()
                        };
                        SPD_INFO("uniform name: " << name << " value: " << v[0].GetDouble() << ", " << v[1].GetDouble() << ", " << v[2].GetDouble());
                        r.type = (uint8_t)NE::Resource::MatPropType::VEC3;
                        payload.append(reinterpret_cast<const char*>(f), sizeof(f));
                    } else if (v.IsArray() && v.Size() == 16) {
                        float m[16];
                        for (rapidjson::SizeType i = 0; i < 16; ++i)
                            m[i] = (float)v[i].GetDouble();
                        r.type = (uint8_t)NE::Resource::MatPropType::MAT4;
                        payload.append(reinterpret_cast<const char*>(m), sizeof(m));
                    }

                    r.dataOffset = (uint32_t)dataStart;
                    r.dataSize = (uint32_t)(payload.size() - dataStart);

                    // add name to shared strings
                    uint32_t nameOff = (uint32_t)strings.size();
                    strings.append(name.data(), name.size());

                    r.nameOffset = nameOff; // relative for now
                    propRecs.push_back(r);
                }
                //// case B: string -> treat as texture uuid
                //else if (v.IsString()) {
                //    const char* uuidStr = v.GetString();
                //    // we assume editor saved actual uuid or empty string
                //    if (uuidStr[0] != '\0') {
                //        NE::Resource::MatTexRecord tr{};
                //        tr.nameLen = (uint32_t)name.size();

                //        // put name in shared strings
                //        uint32_t nameOff = (uint32_t)strings.size();
                //        strings.append(name.data(), name.size());
                //        tr.nameOffset = nameOff;

                //        // copy up to 36 chars (your struct is exactly 36, no null)
                //        std::memset(tr.uuid, 0, 36);
                //        std::memcpy(tr.uuid, uuidStr, std::min<size_t>(std::strlen(uuidStr), 36));

                //        texRecs.push_back(tr);
                //    }
                //    // if empty string, just skip (no texture bound)
                //}
                // else: unknown type -> ignore
                else if (v.IsString()) {
                    const char* uuidStr = v.GetString();

                    NE::Resource::MatTexRecord tr{};
                    tr.nameLen = (uint32_t)name.size();

                    // put name in shared strings
                    uint32_t nameOff = (uint32_t)strings.size();
                    strings.append(name.data(), name.size());
                    tr.nameOffset = nameOff;

                    // write 36 bytes no matter what
                    std::memset(tr.uuid, 0, 36);
                    if (uuidStr && uuidStr[0] != '\0') {
                        std::memcpy(tr.uuid, uuidStr, std::min<size_t>(std::strlen(uuidStr), 36));
                    }

                    texRecs.push_back(tr);
                }
            }
        }

        // Fill header counts
        h.propCount = (uint16_t)propRecs.size();
        h.texCount = (uint16_t)texRecs.size();
        h.shaderNameLen = shaderNameLen;

        size_t offset = sizeof(NE::Resource::NanoMatHeader);

        // shader name
        h.shaderNameOffset = (uint32_t)offset;
        offset += shaderNameLen;

        // prop table
        const uint32_t propTableBytes = (uint32_t)(propRecs.size() * sizeof(NE::Resource::MatPropRecord));
        h.propsOffset = (uint32_t)offset;
        offset += propTableBytes;

        // shared string blob (for prop names + tex names)
        const uint32_t stringsBase = (uint32_t)offset;
        offset += (uint32_t)strings.size();

        // payload (for prop data)
        const uint32_t payloadBase = (uint32_t)offset;
        offset += (uint32_t)payload.size();

        // texture table (if any)
        if (!texRecs.empty()) {
            h.texTableOffset = (uint32_t)offset;
            offset += (uint32_t)(texRecs.size() * sizeof(NE::Resource::MatTexRecord));
        } else {
            h.texTableOffset = 0;
        }

        // Fix up per-record absolute offsets
        for (auto& r : propRecs) {
            r.nameOffset += stringsBase;
            r.dataOffset += payloadBase;
        }
        for (auto& tr : texRecs) {
            tr.nameOffset += stringsBase;
            // tr.uuid is already inline
        }

        // 7) Write file
        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs) return false;

        ofs.write((char*)&h, sizeof(h));
        ofs.write(shaderName, shaderNameLen);
        if (!propRecs.empty()) ofs.write((char*)propRecs.data(), propTableBytes);
        if (!strings.empty())  ofs.write(strings.data(), (std::streamsize)strings.size());
        if (!payload.empty())  ofs.write(payload.data(), (std::streamsize)payload.size());
        if (!texRecs.empty())  ofs.write((char*)texRecs.data(), (std::streamsize)(texRecs.size() * sizeof(NE::Resource::MatTexRecord)));

        return ofs.good();
    }

    bool AssetManager::CookMesh(const std::string& sourcePath, const std::string& outPath) {
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

}
