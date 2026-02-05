#include "FontAsset.hpp"

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>

#include <ResourceManagement/ResourcePaths.hpp>
#include <ResourceManagement/BinaryHeaders/NanoFontHeader.hpp>

// Include stb_truetype for parsing font metrics during asset cooking
// Note: This is a separate compilation unit from FontAtlas.cpp, so we need the implementation here too
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../../extern/jolt/TestFramework/External/stb_truetype.h"

namespace Editor::Assets {

	bool FontAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
		namespace fs = std::filesystem;

		// Read source TTF/OTF file
		std::ifstream ifs(sourcePath, std::ios::binary | std::ios::ate);
		if (!ifs.is_open()) {
			return false;
		}

		std::streamsize ttfSize = ifs.tellg();
		ifs.seekg(0, std::ios::beg);

		std::vector<uint8_t> ttfData(static_cast<size_t>(ttfSize));
		if (!ifs.read(reinterpret_cast<char*>(ttfData.data()), ttfSize)) {
			return false;
		}

		// Parse font with stb_truetype to get metrics at 100pt reference
		stbtt_fontinfo fontInfo;
		if (!stbtt_InitFont(&fontInfo, ttfData.data(), 0)) {
			return false;
		}

		// Get metrics at 100pt reference size
		constexpr float referenceFontSize = 100.0f;
		float scale = stbtt_ScaleForPixelHeight(&fontInfo, referenceFontSize);

		int ascent, descent, lineGap;
		stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

		float ascent100 = ascent * scale;
		float descent100 = descent * scale;
		float lineHeight100 = (ascent - descent + lineGap) * scale;

		// Create header
		NE::Resource::NanoFontHeader hdr{};
		hdr.ttfDataSize = static_cast<uint64_t>(ttfSize);
		hdr.ascent100 = ascent100;
		hdr.descent100 = descent100;
		hdr.lineHeight100 = lineHeight100;

		// Ensure output directory exists
		std::filesystem::path filePath(outPath);
		std::filesystem::path directory = filePath.parent_path();
		if (!directory.empty() && !std::filesystem::exists(directory)) {
			std::filesystem::create_directories(directory);
		}

		// Write binary: header + raw TTF data
		std::ofstream ofs(outPath, std::ios::binary);
		if (!ofs.is_open()) {
			return false;
		}

		ofs.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
		ofs.write(reinterpret_cast<const char*>(ttfData.data()), ttfSize);

		return true;
	}

	bool FontAsset::LoadImportSettings(const std::string& sourcePath) {
		// Font assets don't have additional import settings yet
		// Could add settings for: default size, character ranges, etc.
		return true;
	}

	bool FontAsset::SaveImportSettings(const std::string& sourcePath) {
		// Font assets don't have additional import settings yet
		return true;
	}

}
