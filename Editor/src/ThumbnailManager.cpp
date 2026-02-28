#include "pch.h"
#include "ThumbnailManager.hpp"

#include <compressonator/cmp_compressonatorlib/compressonator.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include <stb_image/stb_image_resize2.h>
#include <glad/glad.h>

#include <Engine.hpp>
#include <ResourceManagement/ResourcePaths.hpp>
#include <ResourceManagement/BinaryHeaders/NanoThumbHeader.hpp>

#include "AssetManagement/AssetManager.hpp"

namespace Editor::Assets {
    namespace {
        bool CMP_API Progress(float fProgress, CMP_DWORD_PTR, CMP_DWORD_PTR) {
            std::printf("\r[BC7] %3.0f%%", fProgress);
            return false;
        }

        CMP_ERROR CompressRGBA8ToBC3(const uint8_t* rgba8, uint32_t w, uint32_t h,
            float quality, uint32_t threads,
            std::vector<uint8_t>& outBC3)
        {
            outBC3.clear();
            if (!rgba8 || w == 0 || h == 0)
				return CMP_ERR_INVALID_SOURCE_TEXTURE;

			quality = std::clamp(quality, 0.0f, 1.0f);

            //if (threads == 0) {
            //    // Auto-detect threads
            //    uint32_t hwThreads = std::thread::hardware_concurrency();
            //    threads = hwThreads > 1 ? hwThreads - 1 : 1;
            //}

            CMP_Texture src{};
            src.dwSize = sizeof(src);
            src.dwWidth = w;
            src.dwHeight = h;
			src.dwPitch = w * 4;
            src.format = CMP_FORMAT_RGBA_8888;
            src.dwDataSize = w * h * 4;
            src.pData = (CMP_BYTE*)rgba8;

            CMP_Texture dst{};
            dst.dwSize = sizeof(dst);
            dst.dwWidth = w;
            dst.dwHeight = h;
            dst.dwPitch = 0;
            dst.format = CMP_FORMAT_BC3;

            dst.dwDataSize = CMP_CalculateBufferSize(&dst);
            dst.pData = (CMP_BYTE*)std::malloc(dst.dwDataSize);
            if (!dst.pData) 
                return CMP_ERR_MEM_ALLOC_FOR_MIPSET;

            CMP_CompressOptions opts{};
            opts.dwSize = sizeof(opts);
            opts.fquality = quality;
            opts.dwnumThreads = threads;

            CMP_ERROR err = CMP_ConvertTexture(&src, &dst, &opts, &Progress);
            if (err == CMP_OK) {
                outBC3.assign(static_cast<uint8_t*>(dst.pData), static_cast<uint8_t*>(dst.pData) + dst.dwDataSize);
            }

            std::free(dst.pData);
            return err;
        }

		constexpr int thumbnailSize = 256;

        bool WriteBytes(FILE* f, const void* data, size_t size) {
            return std::fwrite(data, 1, size, f) == size;
        }

        void AspectFitIntoSquareRGBA8(const stbi_uc* srcRGBA,
            int srcW, int srcH,
            stbi_uc* dstRGBA,
            int dstSize)
        {
            std::fill(dstRGBA, dstRGBA + (dstSize * dstSize * 4), (stbi_uc)0);

            if (srcW <= 0 || srcH <= 0) return;

            const float scale = std::min((float)dstSize / (float)srcW, (float)dstSize / (float)srcH);
            const int newW = std::max(1, (int)std::round(srcW * scale));
            const int newH = std::max(1, (int)std::round(srcH * scale));

            std::vector<stbi_uc> resized(newW * newH * 4);

            const int ok = *stbir_resize_uint8_linear(
                srcRGBA, srcW, srcH, 0,
                resized.data(), newW, newH, 0,
                STBIR_RGBA
            );
            if (!ok) return;

            const int offX = (dstSize - newW) / 2;
            const int offY = (dstSize - newH) / 2;

            for (int y = 0; y < newH; ++y) {
                stbi_uc* dstRow = dstRGBA + ((offY + y) * dstSize + offX) * 4;
                const stbi_uc* srcRow = resized.data() + (y * newW) * 4;
                std::memcpy(dstRow, srcRow, (size_t)newW * 4);
            }
        }
    }

    ThumbnailManager& ThumbnailManager::GetInstance() {
        static ThumbnailManager instance;
        return instance;
	}

    unsigned int ThumbnailManager::GetThumbnail(const std::filesystem::path& filePath) {
        if (std::filesystem::is_directory(filePath))
            return m_folderIcon;

        std::string extension = filePath.extension().string();
        for (auto& c : extension) c = static_cast<char>(tolower(c));

        if (extension == ".scene") {
            return m_sceneIcon;
        } else if (extension == ".nfab") {
            return m_prefabIcon;
        } else if (extension == ".nanomat") {
            return m_materialIcon;
		} else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") {
			std::string uuid = Assets::AssetManager::GetInstance().RetrieveUUID(filePath.string());
            auto thumbnail = LoadCookedThumbnail(uuid);
            if (thumbnail != 0) {
                return thumbnail;
            }
        } else if (extension == ".obj" || extension == ".fbx") {
            return m_meshIcon;
		} else if (filePath == "submesh") {
            return m_subMeshIcon;
        }

        return m_fileIcon;
    }

    unsigned int ThumbnailManager::GetThumbnailByUUID(const std::string& uuid) {
        auto thumbnail = LoadCookedThumbnail(uuid);
        if (thumbnail != 0) {
            return thumbnail;
        }
        return 0;
    }

    void ThumbnailManager::GenerateThumbnail(const std::filesystem::path& sourceImagePath, const std::string& uuid) {
        std::filesystem::path destPath = NE::Resource::ComputeThumbnailPathFromUUID(uuid);

        if (std::filesystem::exists(destPath)) {
            return;
        }

        // Load RGBA
        int srcW = 0, srcH = 0, srcChannels = 0;
        stbi_uc* srcRGBA = stbi_load(sourceImagePath.string().c_str(), &srcW, &srcH, &srcChannels, 4);
        if (!srcRGBA) {
            // TODO: log stbi_failure_reason()
            return;
        }

        std::vector<stbi_uc> thumbRGBA(thumbnailSize * thumbnailSize * 4);
        AspectFitIntoSquareRGBA8(srcRGBA, srcW, srcH, thumbRGBA.data(), thumbnailSize);

        stbi_image_free(srcRGBA);

        std::vector<uint8_t> compressedData;
        const float quality = 0.1f;
        const uint32_t threads = 0;

        CMP_ERROR err = CompressRGBA8ToBC3(
            thumbRGBA.data(),
            (uint32_t)thumbnailSize, (uint32_t)thumbnailSize,
            quality, threads,
            compressedData
        );

        if (err != CMP_OK || compressedData.empty()) {
            // TODO: log err
            return;
        }

        std::filesystem::create_directories(destPath.parent_path());

        FILE* file = std::fopen(destPath.string().c_str(), "wb");
        if (!file) {
            // TODO: log error
            return;
        }

        NE::Resource::NanoThumbHeader header{};
        header.width = (uint16_t)thumbnailSize;
        header.height = (uint16_t)thumbnailSize;
        header.mipCount = 1;
        header.format = NE::Resource::ThumbFormat::BC3;
        header.dataSizeBytes = (uint32_t)compressedData.size();

        const bool ok =
            WriteBytes(file, &header, sizeof(header)) &&
            WriteBytes(file, compressedData.data(), compressedData.size());

        std::fclose(file);

        if (!ok) {
            // Optional: delete partial file
            std::error_code ec;
            std::filesystem::remove(destPath, ec);
            // TODO: log write failure
        }
    }

    ThumbnailManager::ThumbnailManager() {
        m_folderIcon = LoadRawIcon("Library/Icons/icon_folder.png");
        m_fileIcon = LoadRawIcon("Library/Icons/icon_file.png");
        m_sceneIcon = LoadRawIcon("Library/Icons/icon_scene.png");
        m_prefabIcon = LoadRawIcon("Library/Icons/icon_prefab.png");
        m_materialIcon = LoadRawIcon("Library/Icons/icon_material.png");
        m_meshIcon = LoadRawIcon("Library/Icons/icon_mesh.png");
        m_subMeshIcon = LoadRawIcon("Library/Icons/icon_submesh.png");
    }

    ThumbnailManager::~ThumbnailManager() {
        glDeleteTextures(1, &m_folderIcon);
        glDeleteTextures(1, &m_fileIcon);
        glDeleteTextures(1, &m_sceneIcon);
    }

    unsigned int ThumbnailManager::LoadRawIcon(const std::string& path) {
        stbi_set_flip_vertically_on_load(true);

        int srcWidth, srcHeight, srcChannels;
        stbi_uc* data = stbi_load(path.c_str(), &srcWidth, &srcHeight, &srcChannels, 4); // Force RGBA
        if (!data) {
            // TODO log error or smth
            return 0;
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        if (textureID == 0) {
            stbi_image_free(data);
            return 0; // Failed to generate texture
        }

        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, srcWidth, srcHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(data);
        stbi_set_flip_vertically_on_load(false);

        return textureID;
    }

    unsigned int ThumbnailManager::LoadCookedThumbnail(const std::string& uuid) {
        if (uuid.empty())
			return 0;

        if (auto it = m_cache.find(uuid); it != m_cache.end()) {
            m_lru.erase(it->second.lruIt);
            m_lru.push_front(uuid);
            it->second.lruIt = m_lru.begin();
            return it->second.tex;
        }

        GLuint tex = NE::LoadCookedThumbnailGL(uuid);
        if (!tex) return 0;

        m_lru.push_front(uuid);
        m_cache.emplace(uuid, ThumbCacheEntry{ tex, m_lru.begin() });

        while (m_cache.size() > m_maxThumbs) {
            const std::string victim = m_lru.back();
            m_lru.pop_back();

            auto vit = m_cache.find(victim);
            if (vit != m_cache.end()) {
                NE::DestroyGLTexture(vit->second.tex);
                m_cache.erase(vit);
            }
        }

		return tex;
    }
}