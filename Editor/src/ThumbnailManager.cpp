#include "ThumbnailManager.hpp"
#include <Graphics/OpenGL/GLUtilities.hpp>

namespace Editor {

    ThumbnailManager::ThumbnailManager()
    {
    }

    ThumbnailManager::~ThumbnailManager()
    {
    }

    void ThumbnailManager::Init()
    {
        m_DirectoryIcon = LoadTextureFromFile("Library/Icons/icon_folder.png");
        m_FileIcon = LoadTextureFromFile("Library/Icons/icon_file.png");
    }

    void ThumbnailManager::Shutdown()
    {
        //glDeleteTextures(1, &m_DirectoryIcon);
        //glDeleteTextures(1, &m_FileIcon);

        //for (auto& [path, textureID] : m_LoadedThumbnails) {
        //    glDeleteTextures(1, &textureID);
        //}
        m_LoadedThumbnails.clear();
    }

    GLuint ThumbnailManager::GetThumbnail(const std::filesystem::path& filePath)
    {
        if (std::filesystem::is_directory(filePath))
            return GetDefaultDirectoryIcon();

        std::string extension = filePath.extension().string();
        for (auto& c : extension) c = (char)tolower(c); // lowercase it

        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") {
            // Check if we already loaded this file
            if (m_LoadedThumbnails.find(filePath.string()) != m_LoadedThumbnails.end())
                return m_LoadedThumbnails[filePath.string()];

            // Load it fresh
            GLuint thumbnail = LoadTextureFromFile(filePath.string());
            if (thumbnail != 0) {
                m_LoadedThumbnails[filePath.string()] = thumbnail;
                return thumbnail;
            }
        }

        return GetDefaultFileIcon(); // fallback if it's not an image
    }

    GLuint ThumbnailManager::GetDefaultDirectoryIcon()
    {
        return m_DirectoryIcon;
    }

    GLuint ThumbnailManager::GetDefaultFileIcon()
    {
        return m_FileIcon;
    }

    GLuint ThumbnailManager::LoadTextureFromFile(const std::string& path)
    {
        return Engine::CreateGLTexture(path);
    }
}