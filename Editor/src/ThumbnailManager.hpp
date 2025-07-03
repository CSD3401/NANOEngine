#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>

typedef unsigned int GLuint;
// testing stuff not final
namespace Editor {
    class ThumbnailManager {
    public:
        ThumbnailManager();
        ~ThumbnailManager();

        // Call this ONCE during Editor startup
        void Init();

        // Call this ONCE during Editor shutdown
        void Shutdown();

        // Get the OpenGL texture ID for a given file
        GLuint GetThumbnail(const std::filesystem::path& filePath);

        GLuint m_DirectoryIcon = 0;
        GLuint m_FileIcon = 0;
    private:
        GLuint LoadTextureFromFile(const std::string& path);
        GLuint GetDefaultDirectoryIcon();
        GLuint GetDefaultFileIcon();

    private:

        // Cache specific file thumbnails (like actual .png images as thumbnails)
        std::unordered_map<std::string, GLuint> m_LoadedThumbnails;
    };
}
