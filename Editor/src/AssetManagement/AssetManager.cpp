#include "AssetManager.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>

//#define STB_IMAGE_IMPLEMENTATION
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


namespace {
	std::string ToLower(std::string s) { 
		for (auto& c : s) 
			c = (char)std::tolower((unsigned char)c); 
		return s; 
	}

    // Texture Helpers
    // Optional: progress callback for Compressonator (return true to abort)
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

    // Load image as RGBA8 with stb_image
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

    // Tiny enums so your header fields are readable
    enum class TexShape : uint8_t { D2 = 0, Cube = 1, D3 = 2, D2Array = 3 };
    enum class TexFormat : uint8_t { BC7_UNORM = 0, BC7_UNORM_SRGB = 1 };

    // Compress RGBA8 -> BC7 (CPU path)
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
        dst.dwDataSize = CMP_CalculateBufferSize(&dst); // ensures correct BCn size
        dst.pData = (CMP_BYTE*)std::malloc(dst.dwDataSize);
        if (!dst.pData) return CMP_ERR_MEM_ALLOC_FOR_MIPSET;

        CMP_CompressOptions opts{};
        opts.dwSize = sizeof(opts);
        opts.fquality = quality;   // 0..1 (higher = slower/better)
        opts.dwnumThreads = threads;   // 0 = auto

        CMP_ERROR err = CMP_ConvertTexture(&src, &dst, &opts, &Progress);
        if (err == CMP_OK) {
            outBC7.assign(dst.pData, dst.pData + dst.dwDataSize);
        }

        std::free(dst.pData);
        return err;
    }

    // Write your packed NanoTex header + payload
    static bool WriteNanoTex(const std::string& outPath,
        uint32_t w, uint32_t h,
        bool isSRGB, TexShape shape, TexFormat fmt,
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
}

namespace Editor {

	AssetManager& AssetManager::GetInstance() {
		static AssetManager am;
		return am;
	}

	void AssetManager::GenerateMetadata(const std::string& sourcePath) {
		std::filesystem::path fsSourcePath = sourcePath;
		std::filesystem::path metaPath = sourcePath + ".meta";

        AssetMetadata metadata{};
        metadata.sourcePath = sourcePath;

        if (std::filesystem::exists(metaPath)) {
            std::ifstream ifs(metaPath);
            if (!ifs) {
                SPD_WARNING("Failed to read meta file: " << metaPath.string());
                return;
            }

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
                }
            }

            if (metadata.uuid.empty())
                metadata.uuid = GenerateUUID(); // fallback, should rarely happen
            if (metadata.type == AssetType::Unknown)
                metadata.type = GetAssetTypeFromExtension(fsSourcePath.extension().string());

            switch (metadata.type) {
            case AssetType::Texture: {
                GetAssetsOfType<AssetType::Texture>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
                break;
            }
            case AssetType::Mesh: {
                GetAssetsOfType<AssetType::Mesh>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
                break;
            }
            case AssetType::Shader: {
                GetAssetsOfType<AssetType::Shader>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
                break;
            }
            case AssetType::Material: {
                GetAssetsOfType<AssetType::Material>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
                break;
            }
            case AssetType::Audio: {

                break;
            }
            default:
                break;
            }

            m_assets[metadata.uuid] = std::move(metadata);
            return;
        }

		std::string uuid = GenerateUUID();
        std::string outPath = NE::Resource::ComputeArtifactPathFromUUID(uuid);

		std::ofstream ofs(metaPath);
		ofs << "importerVersion: " << CURRENT_META_SCHEMA_VERSION << '\n'
			<< "uuid: " << uuid << '\n';

		AssetType assetType = GetAssetTypeFromExtension(fsSourcePath.extension().string());

		switch (assetType) {
		case AssetType::Texture: {
			ofs << "assetType: Texture\n"
				<< "sourcePath: " << sourcePath << '\n';
            
            CookTexture(sourcePath, outPath);
            GetAssetsOfType<AssetType::Texture>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
			break;
		}
		case AssetType::Mesh: {
            ofs << "assetType: Mesh\n"
                << "sourcePath: " << sourcePath << '\n';

            CookMesh(sourcePath, outPath);
            GetAssetsOfType<AssetType::Mesh>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
			break;
		}
        case AssetType::Shader: {
            ofs << "assetType: Shader\n"
                << "sourcePath: " << sourcePath << '\n';

            CookShader(sourcePath, outPath);
            GetAssetsOfType<AssetType::Shader>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
            break;
        }
        case AssetType::Material: {
            ofs << "assetType: Material\n"
                << "sourcePath: " << sourcePath << '\n';

            CookMaterial(sourcePath, outPath);
            GetAssetsOfType<AssetType::Material>().push_back({ fsSourcePath.filename().string(), metadata.uuid });
            break;
        }
        case AssetType::Audio: {

            break;
        }
		default:
			break;
		}

		ofs.close();


		metadata.uuid = uuid;
		metadata.type = assetType;


		m_assets[uuid] = std::move(metadata);
	}

    void AssetManager::ReimportAsset(const std::string& sourcePath) {
        std::filesystem::path fsSourcePath = sourcePath;
        std::filesystem::path metaPath = sourcePath + ".meta";

        AssetMetadata metadata{};
        metadata.sourcePath = sourcePath;

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
                }
            }
        }

        std::string outPath = NE::Resource::ComputeArtifactPathFromUUID(metadata.uuid);

        AssetType assetType = GetAssetTypeFromExtension(fsSourcePath.extension().string());
        switch (assetType) {
        case AssetType::Texture: {
            CookTexture(sourcePath, outPath);
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
                }
            }

        }
        return uuid;
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

	bool AssetManager::ImportTexture() {




		return false;
	}

    bool AssetManager::CookTexture(const std::string& sourcePath, const std::string& outPath)
    {
        std::filesystem::path src = sourcePath;
        std::filesystem::path out = outPath;
        std::filesystem::create_directories(out.parent_path());

        const bool srgb = GuessSRGBFromExt(src);

        // 1) Load
        std::vector<uint8_t> rgba8;
        uint32_t w = 0, h = 0;
        if (!LoadRGBA8(src.string(), rgba8, w, h)) {
            std::fprintf(stderr, "[CookTexture] Failed to load: %s\n", src.string().c_str());
            return false;
        }

        // 2) Compress to BC7
        std::vector<uint8_t> bc7;
        const float    quality = 0.6f;  // tune per build config
        const uint32_t threads = 0;     // 0 = auto
        if (CMP_ERROR err = CompressRGBA8ToBC7(rgba8.data(), w, h, quality, threads, bc7); err != CMP_OK) {
            std::fprintf(stderr, "[CookTexture] Compressonator error %d on: %s\n", (int)err, src.string().c_str());
            return false;
        }

        // 3) Write .nanotex
        const uint16_t mipCount = 1;          // (future: generate mip chain)
        const uint16_t layers = 1;          // (future: array/cubemap)
        const TexShape shape = TexShape::D2;
        const TexFormat fmt = srgb ? TexFormat::BC7_UNORM_SRGB : TexFormat::BC7_UNORM;

        if (!WriteNanoTex(out.string(), w, h, srgb, shape, fmt, mipCount, layers, bc7)) {
            std::fprintf(stderr, "[CookTexture] Failed to write: %s\n", out.string().c_str());
            return false;
        }

        std::printf("\n[CookTexture] OK: %s -> %s (%ux%u, %zu bytes)\n",
            src.string().c_str(), out.string().c_str(), w, h, bc7.size());
        return true;
    }

    bool AssetManager::CookShader(const std::string& sourcePath, const std::string& outPath) {
        std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());

        const std::string source = LoadShaderSource(sourcePath);
        auto shaderStages = Preprocess(source);

        return NE::CookShader(sourcePath, outPath, shaderStages);
    }

    bool AssetManager::CookMaterial(const std::string& sourcePath, const std::string& outPath) {
        //std::filesystem::path src = sourcePath;
        std::filesystem::path out = outPath;
        std::filesystem::create_directories(out.parent_path());

        std::ifstream in(sourcePath);
        if (!in) return false;
        std::string j((std::istreambuf_iterator<char>(in)), {});
        rapidjson::Document doc; doc.Parse(j.c_str());
        if (!doc.IsObject()) return false;

        // 2) Fill header (pipeline state)
        NE::Resource::NanoMatHeader h{};
        h.depthTest = doc.HasMember("DepthTest") ? (doc["DepthTest"].GetBool() ? 1 : 0) : 1;
        h.blendMode = doc.HasMember("BlendMode") ? (doc["BlendMode"].GetBool() ? 1 : 0) : 0;
        h.cullMode = doc.HasMember("CullMode") ? doc["CullMode"].GetUint() : 0;
        h.polygonMode = doc.HasMember("PolygonMode") ? doc["PolygonMode"].GetUint() : 0;

        const char* shaderName = doc.HasMember("Shader") ? doc["Shader"].GetString() : "Basic";
        const uint32_t shaderNameLen = (uint32_t)std::strlen(shaderName);

        // 3) Collect properties
        std::vector<NE::Resource::MatPropRecord> recs;
        std::string strings; // names will be appended here
        std::string payload; // binary values appended here

        if (doc.HasMember("Properties") && doc["Properties"].IsObject()) {
            for (auto it = doc["Properties"].MemberBegin(); it != doc["Properties"].MemberEnd(); ++it) {
                const std::string name = it->name.GetString();
                const auto& v = it->value;

                NE::Resource::MatPropRecord r{};
                r.nameLen = (uint32_t)name.size();
                r.nameOffset = 0; // fill later after we know base offsets
                r.count = 1;

                // Serialize the value
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
                    float f[3] = { (float)v[0].GetDouble(), (float)v[1].GetDouble(), (float)v[2].GetDouble() };
                    r.type = (uint8_t)NE::Resource::MatPropType::VEC3;
                    payload.append(reinterpret_cast<const char*>(f), sizeof(f));
                } else if (v.IsArray() && v.Size() == 16) {
                    float m[16];
                    for (rapidjson::SizeType i = 0; i < 16; ++i) m[i] = (float)v[i].GetDouble();
                    r.type = (uint8_t)NE::Resource::MatPropType::MAT4;
                    payload.append(reinterpret_cast<const char*>(m), sizeof(m));
                } else {
                    // unknown type – skip or handle texture uuid strings later
                    continue;
                }
                r.dataOffset = 0; // fill later
                r.dataSize = (uint32_t)(payload.size() - dataStart);

                // Add name to string table
                uint32_t nameOff = (uint32_t)strings.size();
                strings.append(name.data(), name.size());

                r.nameOffset = nameOff; // relative to strings base (filled after we know base)
                recs.push_back(r);
            }
        }

        // 4) Finalize offsets relative to file start
        h.propCount = (uint16_t)recs.size();
        h.shaderNameLen = shaderNameLen;

        size_t offset = sizeof(NE::Resource::NanoMatHeader);
        h.shaderNameOffset = (uint32_t)offset;
        offset += shaderNameLen;

        const uint32_t propsTableBytes = (uint32_t)(recs.size() * sizeof(NE::Resource::MatPropRecord));
        h.propsOffset = (uint32_t)offset;
        offset += propsTableBytes;

        const uint32_t stringsBase = (uint32_t)offset;
        offset += (uint32_t)strings.size();

        const uint32_t payloadBase = (uint32_t)offset;
        // payload bytes will follow

        // Fix up per-record absolute offsets
        for (auto& r : recs) {
            r.nameOffset += stringsBase;
            r.dataOffset += payloadBase;
        }

        // 5) Write file
        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs) return false;

        ofs.write((char*)&h, sizeof(h));
        ofs.write(shaderName, shaderNameLen);
        if (!recs.empty()) ofs.write((char*)recs.data(), propsTableBytes);
        if (!strings.empty()) ofs.write(strings.data(), (std::streamsize)strings.size());
        if (!payload.empty()) ofs.write(payload.data(), (std::streamsize)payload.size());

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
            // offsets filled after we write blobs
        }

        // compute sphere from AABB (optional – depends on your NanoMeshHeader)
        //NE::Math::Vec3 center(0, 0, 0);
        //float radius = 0.0f;
        //if (hasBounds) {
        //    center = (minP + maxP) * 0.5f;
        //    NE::Math::Vec3 ext = maxP - center;
        //    radius = std::max(std::max(ext.x, ext.y), ext.z);
        //}

        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs) return false;

        NE::Resource::NanoMeshHeader header{};
        header.submeshCount = static_cast<uint16_t>(scene->mNumMeshes);

        // only do this if your NanoMeshHeader actually has these fields
        //header.sphereCenter[0] = center.x;
        //header.sphereCenter[1] = center.y;
        //header.sphereCenter[2] = center.z;
        //header.sphereRadius = radius;

        // 1) header
        ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // 2) placeholder submesh table
        const std::streamoff subTablePos = ofs.tellp();
        ofs.write(reinterpret_cast<const char*>(subdescs.data()),
            subdescs.size() * sizeof(NE::Resource::NanoSubmeshDesc));

        // 3) actual data + capture offsets
        for (size_t m = 0; m < blobs.size(); ++m) {
            auto& rb = blobs[m];

            uint32_t vertexOffset = static_cast<uint32_t>(ofs.tellp());
            ofs.write(reinterpret_cast<const char*>(rb.vertices.data()), rb.vertices.size());

            uint32_t indexOffset = static_cast<uint32_t>(ofs.tellp());
            ofs.write(reinterpret_cast<const char*>(rb.indices.data()), rb.indices.size());

            subdescs[m].vertexDataOffset = vertexOffset;
            subdescs[m].indexDataOffset = indexOffset;
        }

        // 4) go back and patch submesh table
        ofs.seekp(subTablePos, std::ios::beg);
        ofs.write(reinterpret_cast<const char*>(subdescs.data()),
            subdescs.size() * sizeof(NE::Resource::NanoSubmeshDesc));

        std::printf("[CookMesh] wrote %s\n", outPath.c_str());
        return true;
    }

}
