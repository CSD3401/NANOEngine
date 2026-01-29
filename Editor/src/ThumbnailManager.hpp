#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>

namespace Editor::Assets {
    struct ThumbCacheEntry {
        unsigned int tex = 0;
        std::list<std::string>::iterator lruIt;
    };

    class ThumbnailManager {
    public:
        static ThumbnailManager& GetInstance();

        unsigned int GetThumbnail(const std::filesystem::path& filePath);
        unsigned int GetThumbnailByUUID(const std::string& uuid);

		void GenerateThumbnail(const std::filesystem::path& sourceImagePath, const std::string& uuid);
        unsigned int LoadRawIcon(const std::string& path);
    private:
        ThumbnailManager();
        ~ThumbnailManager();

		unsigned int LoadCookedThumbnail(const std::string& uuid);

        unsigned int m_folderIcon = 0;
        unsigned int m_sceneIcon = 0;
        unsigned int m_fileIcon = 0;
		unsigned int m_materialIcon = 0;
		unsigned int m_prefabIcon = 0;
		unsigned int m_meshIcon = 0;
		unsigned int m_subMeshIcon = 0;

        size_t m_maxThumbs = 512;
        std::unordered_map<std::string, ThumbCacheEntry> m_cache;
        std::list<std::string> m_lru; // front=MRU, back=LRU
    };
}
