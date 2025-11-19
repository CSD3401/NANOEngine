#include "ResourceManager.hpp"

#include <filesystem>
#include <fstream>

#include "Core/SpdLogger.hpp"

namespace NE::Resource {

	ResourceManager& ResourceManager::GetInstance() {
		static ResourceManager instance;
		return instance;
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