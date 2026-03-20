#include "pch.h"
#include "ResourceManager.hpp"

#include <filesystem>
#include <fstream>

#include <glad/glad.h>

#include "Core/SpdLogger.hpp"
#include "ResourceManagement/BinaryHeaders/NanoThumbHeader.hpp"
#include "ResourceManagement/ResourcePaths.hpp"

namespace NE::Resource {

	ResourceManager& ResourceManager::GetInstance() {
		static ResourceManager instance;
		return instance;
	}

	unsigned int ResourceManager::LoadCookedThumbnailGL(const std::string& uuid) {
        const auto path = ComputeThumbnailPathFromUUID(uuid);

        std::vector<uint8_t> bytes;
        if (!ReadBinFile(path, bytes) || bytes.size() < sizeof(NanoThumbHeader)) {
            SPD_WARNING("Failed to read thumbnail: " << uuid);
            return 0;
        }

        NanoThumbHeader hdr{};
        std::memcpy(&hdr, bytes.data(), sizeof(NanoThumbHeader));

        if (hdr.magic != NTHM_MAGIC ||
            hdr.version != CURRENT_NANOTHUMB_FORMAT_VERSION ||
            hdr.format != ThumbFormat::BC3 ||
            hdr.width == 0 || hdr.height == 0) {
            SPD_WARNING("Invalid thumb header: " << uuid);
            return 0;
        }

        const size_t payloadSize = bytes.size() - sizeof(NanoThumbHeader);
        if (hdr.dataSizeBytes != payloadSize) {
            SPD_WARNING("Thumb payload size mismatch: " << uuid);
            return 0;
        }

        const void* payload = bytes.data() + sizeof(NanoThumbHeader);

        GLuint tex = 0;
        glGenTextures(1, &tex);
        if (!tex) return 0;

        glBindTexture(GL_TEXTURE_2D, tex);

        // BC3 / DXT5
        glCompressedTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
            (GLsizei)hdr.width,
            (GLsizei)hdr.height,
            0,
            (GLsizei)payloadSize,
            payload
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
	}

	void ResourceManager::DestroyGLTexture(unsigned int id) {
		if (id) glDeleteTextures(1, &id);
	}

	void ResourceManager::UnloadResource(const std::string& uuid) {
		if (uuid.empty()) {
			return;
		}

		std::scoped_lock lock(mtx);
		cache.erase(uuid);
	}

	bool ResourceManager::ReadBinFile(const std::string& path, std::vector<uint8_t>& out) const {
		std::ifstream ifs(path, std::ios::binary | std::ios::ate);
		if (!ifs) {
			SPD_WARNING("Unable to read binary file: " << path);
			return false;
		}

		const std::streamsize n = ifs.tellg();
		if (n <= 0) return false;

		out.resize(static_cast<size_t>(n));
		ifs.seekg(0, std::ios::beg);

		return static_cast<bool>(ifs.read(reinterpret_cast<char*>(out.data()), n));
	}

}
