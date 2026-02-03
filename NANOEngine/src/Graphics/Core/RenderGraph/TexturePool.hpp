#pragma once

#include <cstdint>
#include <vector>
#include <memory>

#include "NANOEngineAPI.hpp"
#include "TextureFormats.hpp"

namespace NE::Graphics {

    struct PooledTexture {
        uint32_t textureId = 0;
        uint32_t fboId = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        TextureFormat format = TextureFormat::RGBA8;
        bool inUse = false;
    };

    struct TexturePoolStats {
        size_t poolSize = 0;        // Total textures in pool
        size_t inUseCount = 0;      // Textures currently allocated
        size_t hits = 0;            // Times a pooled texture was reused
        size_t misses = 0;          // Times a new texture had to be created
        size_t totalAllocated = 0;  // Lifetime allocation count
    };

    class NANOENGINE_API TexturePool {
    public:
        TexturePool() = default;
        ~TexturePool();

        // Non-copyable
        TexturePool(const TexturePool&) = delete;
        TexturePool& operator=(const TexturePool&) = delete;

        // Acquire a texture matching the description
        // Returns existing pooled texture if available, creates new otherwise
        PooledTexture* Acquire(uint32_t width, uint32_t height, TextureFormat format);

        // Release a texture back to the pool for reuse
        void Release(PooledTexture* texture);

        // Release all textures (marks all as available)
        void ReleaseAll();

        // Clear the pool (deletes all GPU resources)
        void Clear();

        // Get statistics
        const TexturePoolStats& GetStats() const { return m_Stats; }

        // Get all pooled textures (for debugging)
        const std::vector<std::unique_ptr<PooledTexture>>& GetTextures() const { return m_Textures; }

    private:
        std::vector<std::unique_ptr<PooledTexture>> m_Textures;
        TexturePoolStats m_Stats;
    };

}