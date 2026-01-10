#include "TextureAsset.hpp"

#include <filesystem>
#include <fstream>

#include <compressonator/cmp_compressonatorlib/compressonator.h>
#include <stb_image/stb_image.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/ostreamwrapper.h>

#include <Core/SpdLogger.hpp>
#include <ResourceManagement/BinaryHeaders/NanoTexHeader.hpp>

#include "../../Serialization/JSONReflection.hpp"

namespace Editor::Assets {
	namespace {
		bool CMP_API Progress(float fProgress, CMP_DWORD_PTR, CMP_DWORD_PTR) {
			std::printf("\r[BC7] %3.0f%%", fProgress);
			return false;
		}

		bool GuessSRGBFromExt(const std::filesystem::path& p) {
			auto ext = p.extension().string();
			for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
			return (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga");
		}

		bool LoadRGBA8(const std::string& path, std::vector<uint8_t>& rgba, uint32_t& w, uint32_t& h) {
			int x = 0, y = 0, n = 0;
			stbi_uc* data = stbi_load(path.c_str(), &x, &y, &n, 4);
			if (!data) return false;
			w = static_cast<uint32_t>(x);
			h = static_cast<uint32_t>(y);
			rgba.assign(data, data + (size_t)w * h * 4);
			stbi_image_free(data);
			return true;
		}

		//enum class TexShape : uint8_t { D2 = 0, Cube = 1, D3 = 2, D2Array = 3 };
		enum class TexFormat : uint8_t { BC7_UNORM = 0, BC7_UNORM_SRGB = 1, BC5_UNORM = 2 };

		// rgba8 to bc7
		CMP_ERROR CompressRGBA8ToBC7(const uint8_t* rgba8, uint32_t w, uint32_t h,
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
			dst.dwDataSize = CMP_CalculateBufferSize(&dst);
			dst.pData = (CMP_BYTE*)std::malloc(dst.dwDataSize);
			if (!dst.pData) return CMP_ERR_MEM_ALLOC_FOR_MIPSET;

			CMP_CompressOptions opts{};
			opts.dwSize = sizeof(opts);
			opts.fquality = quality;   // 0 to 1
			opts.dwnumThreads = threads;   // 0 = auto

			CMP_ERROR err = CMP_ConvertTexture(&src, &dst, &opts, &Progress);
			if (err == CMP_OK) {
				outBC7.assign(dst.pData, dst.pData + dst.dwDataSize);
			}

			std::free(dst.pData);
			return err;
		}

		CMP_ERROR CompressRGBA8ToBC5(const uint8_t* rgba8, uint32_t w, uint32_t h,
			float quality, uint32_t threads,
			std::vector<uint8_t>& outBC5)
		{
			CMP_Texture src{};
			src.dwSize = sizeof(src);
			src.dwWidth = w;
			src.dwHeight = h;
			src.dwPitch = 0;
			src.format = CMP_FORMAT_RGBA_8888;
			src.dwDataSize = w * h * 4;
			src.pData = (CMP_BYTE*)rgba8;

			CMP_Texture dst{};
			dst.dwSize = sizeof(dst);
			dst.dwWidth = w;
			dst.dwHeight = h;
			dst.dwPitch = 0;
			dst.format = CMP_FORMAT_BC5;
			dst.dwDataSize = CMP_CalculateBufferSize(&dst);
			dst.pData = (CMP_BYTE*)std::malloc(dst.dwDataSize);
			if (!dst.pData) return CMP_ERR_MEM_ALLOC_FOR_MIPSET;

			CMP_CompressOptions opts{};
			opts.dwSize = sizeof(opts);
			opts.fquality = quality;
			opts.dwnumThreads = threads;

			CMP_ERROR err = CMP_ConvertTexture(&src, &dst, &opts, &Progress);
			if (err == CMP_OK) {
				outBC5.assign(dst.pData, dst.pData + dst.dwDataSize);
			}

			std::free(dst.pData);
			return err;
		}

		bool WriteNanoTex(const std::string& outPath,
			uint32_t w, uint32_t h,
			bool isSRGB, Editor::Assets::TexShape shape, TexFormat fmt,
			uint16_t mipCount, uint16_t layers,
			const std::vector<uint8_t>& payload)
		{
			using namespace NE::Resource;

			NanoTexHeader hdr{};
			hdr.magic = 0x4E544558;
			hdr.importerVersion = CURRENT_NANOTEX_FORMAT_VERSION;
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

	bool TextureAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
		namespace fs = std::filesystem;

		fs::path src = sourcePath;
		fs::path out = outPath;
		fs::create_directories(out.parent_path());

		// Get settings (lazy-loaded from meta or defaults)
		const TextureImportSettings& settings = GetImportSettings(sourcePath);

		// Decide on sRGB / normal map based on settings
		const bool isNormalMap = (settings.type == TexType::NormalMap);
		bool srgb = settings.sRGB && !isNormalMap;

		// Load source image into RGBA8
		std::vector<uint8_t> rgba8;
		uint32_t w = 0, h = 0;
		if (!LoadRGBA8(src.string(), rgba8, w, h)) {
			std::fprintf(stderr, "[TextureAsset::Cook] Failed to load: %s\n",
				src.string().c_str());
			return false;
		}

		std::vector<uint8_t> compressed;
		TexFormat fmt = TexFormat::BC7_UNORM;

		const float    quality = 0.5f;  // maybe expose via settings later
		const uint32_t threads = 0;

		if (isNormalMap) {
			if (CMP_ERROR err =
				CompressRGBA8ToBC5(rgba8.data(), w, h, quality, threads, compressed);
				err != CMP_OK) {
				std::fprintf(stderr, "[TextureAsset::Cook] BC5 compress error %d on: %s\n",
					(int)err, src.string().c_str());
				return false;
			}
			fmt = TexFormat::BC5_UNORM;
		} else {
			if (CMP_ERROR err =
				CompressRGBA8ToBC7(rgba8.data(), w, h, quality, threads, compressed);
				err != CMP_OK) {
				std::fprintf(stderr, "[TextureAsset::Cook] BC7 compress error %d on: %s\n",
					(int)err, src.string().c_str());
				return false;
			}
			fmt = srgb ? TexFormat::BC7_UNORM_SRGB : TexFormat::BC7_UNORM;
		}

		const uint16_t mipCount = 1;            // TODO: plug in from settings
		const uint16_t layers = 1;            // TODO: array / cube later
		const TexShape shape = settings.shape;

		if (!WriteNanoTex(out.string(), w, h, srgb, shape, fmt, mipCount, layers, compressed)) {
			std::fprintf(stderr, "[TextureAsset::Cook] Failed to write: %s\n",
				out.string().c_str());
			return false;
		}

		return true;
	}

	bool TextureAsset::LoadImportSettings(const std::string& sourcePath) {
		namespace fs = std::filesystem;
		using rapidjson::IStreamWrapper;
		using rapidjson::Document;

		fs::path metaPath = fs::path(sourcePath).concat(".meta");
		if (!fs::exists(metaPath)) {
			return false;
		}

		std::ifstream ifs(metaPath);
		if (!ifs) {
			SPD_WARNING("TextureAsset::LoadImportSettings - failed to open meta: "
				<< metaPath.string());
			return false;
		}

		IStreamWrapper isw(ifs);
		Document doc;
		doc.ParseStream(isw);

		if (doc.HasParseError() || !doc.IsObject()) {
			SPD_WARNING("TextureAsset::LoadImportSettings - invalid JSON in: "
				<< metaPath.string());
			return false;
		}

		if (!doc.HasMember("textureImport") || !doc["textureImport"].IsObject()) {
			return true;
		}

		if (!importSettings)
			importSettings.emplace();

		const auto& jSettings = doc["textureImport"];
		Deserialization::FromJSON(jSettings, *importSettings);

		return true;
	}

	bool TextureAsset::SaveImportSettings(const std::string& sourcePath) {
		namespace fs = std::filesystem;
		using rapidjson::IStreamWrapper;
		using rapidjson::Document;
		using rapidjson::OStreamWrapper;
		using rapidjson::PrettyWriter;

		//fs::path metaPath = fs::path(sourcePath).concat(".meta");
		//if (!fs::exists(metaPath)) {
		//	SPD_WARNING("TextureAsset::SaveImportSettings - meta does not exist: "
		//		<< metaPath.string());
		//	return false;
		//}

		//std::ifstream ifs(metaPath);
		//if (!ifs)
		//	return false;

		//IStreamWrapper isw(ifs);
		//Document doc;
		//doc.ParseStream(isw);
		//if (doc.HasParseError() || !doc.IsObject())
		//	return false;

		//auto& alloc = doc.GetAllocator();

		//if (!importSettings) importSettings.emplace();
		//auto texImportJson = Serialization::ToJSON(*importSettings, alloc);

		//if (doc.HasMember("textureImport") && doc["textureImport"].IsObject()) {
		//	doc["textureImport"].CopyFrom(texImportJson, alloc);
		//} else {
		//	doc.AddMember("textureImport", texImportJson, alloc);
		//}

		//std::ofstream ofs(metaPath, std::ios::trunc);
		//if (!ofs)
		//	return false;

		//OStreamWrapper osw(ofs);
		//PrettyWriter<OStreamWrapper> writer(osw);
		//writer.SetIndent(' ', 4);
		//doc.Accept(writer);

		//return true;
		std::string metaPath = sourcePath + ".meta";

		rapidjson::Document doc;
		doc.SetObject();

		if (std::filesystem::exists(metaPath)) {
			std::ifstream ifs(metaPath);
			if (ifs) {
				rapidjson::IStreamWrapper isw(ifs);
				doc.ParseStream(isw);
				if (doc.HasParseError() || !doc.IsObject()) {
					doc.SetObject();
				}
			}
		}

		auto& alloc = doc.GetAllocator();

		if (!importSettings) importSettings.emplace();
		auto jSettings = Serialization::ToJSON(*importSettings, alloc);

		if (doc.HasMember("textureImport"))
			doc["textureImport"].CopyFrom(jSettings, alloc);
		else
			doc.AddMember("textureImport", jSettings, alloc);

		std::ofstream ofs(metaPath);
		if (!ofs) {
			SPD_WARNING("Failed to write meta file: " << metaPath);
			return false;
		}

		rapidjson::OStreamWrapper osw(ofs);
		rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
		writer.SetIndent(' ', 4);
		doc.Accept(writer);

		return true;
	}

	TextureImportSettings& TextureAsset::GetImportSettings(const std::string& sourcePath) const {
		if (!importSettings) {
			importSettings.emplace();

			auto* self = const_cast<TextureAsset*>(this);
			self->LoadImportSettings(sourcePath);
		}
		return *importSettings;
	}
}