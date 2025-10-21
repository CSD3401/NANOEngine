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
#include <ResourceManagement/NanoTexHeader.hpp>


namespace {
    //GHEOP
	std::string ToLower(std::string s) { 
		for (auto& c : s) 
			c = (char)std::tolower((unsigned char)c); 
		return s; 
	}

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

        // Strongly recommended in your header:
        //  #pragma pack(push, 1)
        //  struct NanoTexHeader { ... };
        //  #pragma pack(pop)
        //  static_assert(sizeof(NanoTexHeader) == 21, "layout");

        NanoTexHeader hdr{};
        hdr.magic = 0x4E544558; // 'NTEX', already defaulted in your header
        hdr.importerVersion = CURRENT_IMPORTER_VERSION;
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

}

namespace Editor {

	AssetManager& AssetManager::GetInstance() {
		static AssetManager am;
		return am;
	}

	void AssetManager::GenerateMetadata(const std::string& sourcePath) {
		std::filesystem::path fsSourcePath = sourcePath;
		std::filesystem::path metaPath = sourcePath + ".nanometa";

		std::string uuid = GenerateUUID();

		std::ofstream ofs(metaPath);
		ofs << "importerVersion: " << CURRENT_FORMAT_VERSION << '\n'
			<< "uuid: " << uuid << '\n';

		AssetType assetType = GetAssetTypeFromExtension(fsSourcePath.extension().string());

		switch (assetType) {
		case AssetType::Texture: {
			ofs << "assetType: Texture\n"
				<< "sourcePath: " << sourcePath << '\n';
			// cook To be done

            CookTexture(sourcePath);

			break;
		}
		case AssetType::Mesh: {

			break;
		}
		default:
			break;
		}


		ofs.close();

		AssetMetadata metadata;
		metadata.uuid = uuid;
		metadata.type = assetType;
		metadata.sourcePath = sourcePath;

		m_assets[uuid] = std::move(metadata);
	}

	AssetType AssetManager::GetAssetTypeFromExtension(std::string_view extension) {
		std::string e = ToLower(std::string(extension));
		if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga") return AssetType::Texture;
		else if (e == ".fbx" || e == ".obj") return AssetType::Mesh;
		return AssetType::Unknown;
	}

	bool AssetManager::ImportTexture() {




		return false;
	}

    bool AssetManager::CookTexture(const std::string& sourcePath)
    {
        std::filesystem::path src = sourcePath;
        std::filesystem::path out = src;
        out.replace_extension(".nanotex");

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

}
