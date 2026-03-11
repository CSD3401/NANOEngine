#include "pch.h"
#include "LightingAsset.hpp"

#include <filesystem>
#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include <Lighting/LightingAsset.hpp>
#include <ResourceManagement/BinaryHeaders/NanoLightingHeader.hpp>
#include <Serialisation/BinaryReflection.hpp>
#include "../../Serialization/JSONReflection.hpp"

namespace Editor::Assets {

	bool LightingAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
		std::ifstream ifs(sourcePath);
		if (!ifs.is_open()) return false;

		rapidjson::IStreamWrapper isw(ifs);
		rapidjson::Document d;
		d.ParseStream(isw);
		if (d.HasParseError() || !d.IsObject()) return false;

		NE::Lighting::LightingAssetBlob blob;
		if (d.HasMember("lighting") && d["lighting"].IsObject()) {
			Editor::Deserialization::FromJSON(d["lighting"], blob);
		} else {
			Editor::Deserialization::FromJSON(d, blob);
		}

		std::vector<uint8_t> payload;
		NE::Serialization::ToBinary(payload, blob);

		NE::Resource::NanoLightingHeader hdr{};
		hdr.payloadBytes = static_cast<uint64_t>(payload.size());

		std::filesystem::path filePath(outPath);
		std::filesystem::path directory = filePath.parent_path();
		if (!directory.empty() && !std::filesystem::exists(directory)) {
			std::filesystem::create_directories(directory);
		}

		std::ofstream ofs(outPath, std::ios::binary);
		if (!ofs.is_open()) return false;

		ofs.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
		if (!payload.empty()) {
			ofs.write(reinterpret_cast<const char*>(payload.data()),
				static_cast<std::streamsize>(payload.size()));
		}

		return true;
	}

	bool LightingAsset::LoadImportSettings(const std::string& sourcePath) {
		std::ifstream ifs(sourcePath);
		if (!ifs.is_open()) return false;

		rapidjson::IStreamWrapper isw(ifs);
		rapidjson::Document d;
		d.ParseStream(isw);

		return !d.HasParseError() && d.IsObject();
	}

	bool LightingAsset::SaveImportSettings(const std::string& sourcePath) {
		std::ifstream ifs(sourcePath);
		return ifs.good();
	}

}
