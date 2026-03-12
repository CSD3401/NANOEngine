#include "pch.h"
#include "LightingAsset.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

#include <compressonator/cmp_compressonatorlib/compressonator.h>

#include <Core/SpdLogger.hpp>
#include <Lighting/LightmapResource.hpp>
#include <ResourceManagement/BinaryHeaders/NanoLightingHeader.hpp>
#include <ResourceManagement/BinaryHeaders/NanoTexHeader.hpp>
#include <ResourceManagement/ResourcePaths.hpp>
#include <Serialisation/BinaryReflection.hpp>

#include "../../Lighting/LightmapBakeOutput.hpp"
#include "../AssetManager.hpp"

namespace Editor::Assets {
	namespace {
		constexpr uint32_t NLAS_MAGIC = 0x4E4C4153; // NLAS
		constexpr uint16_t CURRENT_LIGHTMAP_ASSET_FILE_VERSION = 1;
		constexpr uint8_t kNanoTexFormatBC6HUF16 = 3u;

#pragma pack(push, 1)
		struct NanoLightmapAssetHeader {
			uint32_t magic = NLAS_MAGIC;
			uint16_t version = CURRENT_LIGHTMAP_ASSET_FILE_VERSION;
			uint64_t payloadBytes = 0u;
		};
#pragma pack(pop)

		bool EnsureParentDirectory(const std::filesystem::path& path) {
			const std::filesystem::path parent = path.parent_path();
			if (parent.empty()) {
				return true;
			}

			std::error_code ec;
			if (std::filesystem::exists(parent, ec)) {
				return true;
			}

			return std::filesystem::create_directories(parent, ec);
		}

		uint64_t HashBytes(const void* data, size_t size) {
			const auto* bytes = static_cast<const uint8_t*>(data);
			uint64_t hash = 1469598103934665603ull;
			for (size_t i = 0; i < size; ++i) {
				hash ^= bytes[i];
				hash *= 1099511628211ull;
			}
			return hash;
		}

		std::string ToHexString(uint64_t value) {
			std::ostringstream stream;
			stream << std::hex << std::setfill('0') << std::setw(16) << value;
			return stream.str();
		}

		std::string BuildStableUuid(std::string_view seed) {
			const uint64_t lo = HashBytes(seed.data(), seed.size());
			const std::string hiSeed = std::string(seed) + "#hi";
			const uint64_t hi = HashBytes(hiSeed.data(), hiSeed.size());
			return "lm" + ToHexString(lo) + ToHexString(hi);
		}

		bool IsFiniteFloat(float value) {
			return std::isfinite(value);
		}

		uint16_t FloatToHalf(float value) {
			uint32_t bits = 0u;
			std::memcpy(&bits, &value, sizeof(bits));

			const uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000u);
			const uint32_t exponent = (bits >> 23) & 0xffu;
			uint32_t mantissa = bits & 0x007fffffu;

			if (exponent == 0xffu) {
				if (mantissa != 0u) {
					const uint16_t payload = static_cast<uint16_t>(mantissa >> 13);
					return static_cast<uint16_t>(sign | 0x7c00u | (payload != 0u ? payload : 0x0200u));
				}

				return static_cast<uint16_t>(sign | 0x7c00u);
			}

			int32_t halfExponent = static_cast<int32_t>(exponent) - 127 + 15;
			if (halfExponent >= 31) {
				return static_cast<uint16_t>(sign | 0x7c00u);
			}

			if (halfExponent <= 0) {
				if (halfExponent < -10) {
					return sign;
				}

				mantissa |= 0x00800000u;
				const uint32_t shift = static_cast<uint32_t>(14 - halfExponent);
				uint32_t halfMantissa = mantissa >> shift;
				const uint32_t roundBit = 1u << (shift - 1u);
				const uint32_t roundMask = roundBit - 1u;
				if ((mantissa & roundBit) != 0u &&
					((mantissa & roundMask) != 0u || (halfMantissa & 1u) != 0u)) {
					++halfMantissa;
				}

				if (halfMantissa >= 0x0400u) {
					return static_cast<uint16_t>(sign | 0x0400u);
				}

				return static_cast<uint16_t>(sign | static_cast<uint16_t>(halfMantissa));
			}

			mantissa += 0x00000fffu + ((mantissa >> 13u) & 1u);
			if ((mantissa & 0x00800000u) != 0u) {
				mantissa = 0u;
				++halfExponent;
				if (halfExponent >= 31) {
					return static_cast<uint16_t>(sign | 0x7c00u);
				}
			}

			return static_cast<uint16_t>(
				sign |
				(static_cast<uint16_t>(halfExponent) << 10u) |
				static_cast<uint16_t>(mantissa >> 13u));
		}

		void SanitizeLighting(
			const std::vector<NE::Math::Vec3>& input,
			std::vector<NE::Math::Vec3>& outLighting) {
			outLighting.resize(input.size(), { 0.0f, 0.0f, 0.0f });
			for (size_t i = 0; i < input.size(); ++i) {
				const auto& texel = input[i];
				auto& dst = outLighting[i];
				dst.x = IsFiniteFloat(texel.x) ? std::max(texel.x, 0.0f) : 0.0f;
				dst.y = IsFiniteFloat(texel.y) ? std::max(texel.y, 0.0f) : 0.0f;
				dst.z = IsFiniteFloat(texel.z) ? std::max(texel.z, 0.0f) : 0.0f;
			}
		}

		void ApplyMaskDrivenDilation(
			uint32_t width,
			uint32_t height,
			uint32_t radius,
			const std::vector<uint8_t>& writeMask,
			std::vector<uint8_t>& ioValidMask,
			std::vector<NE::Math::Vec3>& ioLighting) {
			if (radius == 0u || width == 0u || height == 0u || writeMask.size() != ioValidMask.size()) {
				return;
			}

			static constexpr std::array<std::pair<int, int>, 8u> kNeighborOffsets{ {
				{ -1, -1 }, { 0, -1 }, { 1, -1 },
				{ -1,  0 },             { 1,  0 },
				{ -1,  1 }, { 0,  1 }, { 1,  1 }
			} };

			std::vector<NE::Math::Vec3> currentLighting = ioLighting;
			std::vector<NE::Math::Vec3> nextLighting = currentLighting;
			std::vector<uint8_t> currentMask = ioValidMask;
			std::vector<uint8_t> nextMask = currentMask;

			for (uint32_t passIndex = 0; passIndex < radius; ++passIndex) {
				size_t passFillCount = 0u;
				nextLighting = currentLighting;
				nextMask = currentMask;

				for (uint32_t y = 0u; y < height; ++y) {
					for (uint32_t x = 0u; x < width; ++x) {
						const size_t linearIndex = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
						if (writeMask[linearIndex] == 0u || currentMask[linearIndex] != 0u) {
							continue;
						}

						NE::Math::Vec3 accum{ 0.0f, 0.0f, 0.0f };
						uint32_t neighborCount = 0u;
						for (const auto& [offsetX, offsetY] : kNeighborOffsets) {
							const int neighborX = static_cast<int>(x) + offsetX;
							const int neighborY = static_cast<int>(y) + offsetY;
							if (neighborX < 0 ||
								neighborY < 0 ||
								neighborX >= static_cast<int>(width) ||
								neighborY >= static_cast<int>(height)) {
								continue;
							}

							const size_t neighborIndex =
								static_cast<size_t>(neighborY) * static_cast<size_t>(width) +
								static_cast<size_t>(neighborX);
							if (currentMask[neighborIndex] == 0u) {
								continue;
							}

							accum += currentLighting[neighborIndex];
							++neighborCount;
						}

						if (neighborCount == 0u) {
							continue;
						}

						nextLighting[linearIndex] = accum / static_cast<float>(neighborCount);
						nextMask[linearIndex] = 1u;
						++passFillCount;
					}
				}

				if (passFillCount == 0u) {
					break;
				}

				currentLighting.swap(nextLighting);
				currentMask.swap(nextMask);
			}

			ioLighting.swap(currentLighting);
			ioValidMask.swap(currentMask);
		}

		std::vector<NE::Math::Vec3> GenerateNextMip(
			const std::vector<NE::Math::Vec3>& src,
			uint32_t srcWidth,
			uint32_t srcHeight,
			uint32_t dstWidth,
			uint32_t dstHeight) {
			std::vector<NE::Math::Vec3> dst(static_cast<size_t>(dstWidth) * static_cast<size_t>(dstHeight), { 0.0f, 0.0f, 0.0f });
			for (uint32_t y = 0u; y < dstHeight; ++y) {
				for (uint32_t x = 0u; x < dstWidth; ++x) {
					NE::Math::Vec3 accum{ 0.0f, 0.0f, 0.0f };
					uint32_t sampleCount = 0u;
					for (uint32_t sampleY = 0u; sampleY < 2u; ++sampleY) {
						for (uint32_t sampleX = 0u; sampleX < 2u; ++sampleX) {
							const uint32_t srcX = std::min(srcWidth - 1u, x * 2u + sampleX);
							const uint32_t srcY = std::min(srcHeight - 1u, y * 2u + sampleY);
							const size_t srcIndex = static_cast<size_t>(srcY) * static_cast<size_t>(srcWidth) + static_cast<size_t>(srcX);
							if (srcIndex >= src.size()) {
								continue;
							}
							accum += src[srcIndex];
							++sampleCount;
						}
					}

					if (sampleCount > 0u) {
						dst[static_cast<size_t>(y) * static_cast<size_t>(dstWidth) + static_cast<size_t>(x)] =
							accum / static_cast<float>(sampleCount);
					}
				}
			}
			return dst;
		}

		bool CompressRgb32fToBC6H(
			const std::vector<NE::Math::Vec3>& lighting,
			uint32_t width,
			uint32_t height,
			std::vector<uint8_t>& outCompressed,
			std::string& outErrorMessage) {
			outCompressed.clear();
			outErrorMessage.clear();
			if (width == 0u || height == 0u) {
				outErrorMessage = "invalid BC6H compression dimensions";
				return false;
			}

			std::vector<uint16_t> sourceRgba16f;
			sourceRgba16f.reserve(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
			for (const auto& texel : lighting) {
				sourceRgba16f.push_back(FloatToHalf(texel.x));
				sourceRgba16f.push_back(FloatToHalf(texel.y));
				sourceRgba16f.push_back(FloatToHalf(texel.z));
				sourceRgba16f.push_back(FloatToHalf(0.0f));
			}

			CMP_Texture src{};
			src.dwSize = sizeof(src);
			src.dwWidth = width;
			src.dwHeight = height;
			src.dwPitch = width * sizeof(uint16_t) * 4u;
			src.format = CMP_FORMAT_RGBA_16F;
			src.dwDataSize = static_cast<CMP_DWORD>(sourceRgba16f.size() * sizeof(uint16_t));
			src.pData = reinterpret_cast<CMP_BYTE*>(sourceRgba16f.data());

			CMP_Texture dst{};
			dst.dwSize = sizeof(dst);
			dst.dwWidth = width;
			dst.dwHeight = height;
			dst.dwPitch = 0;
			dst.format = CMP_FORMAT_BC6H;
			dst.dwDataSize = CMP_CalculateBufferSize(&dst);
			dst.pData = static_cast<CMP_BYTE*>(std::malloc(dst.dwDataSize));
			if (!dst.pData) {
				outErrorMessage = "failed to allocate BC6H destination buffer";
				return false;
			}

			CMP_CompressOptions opts{};
			opts.dwSize = sizeof(opts);

			const CMP_ERROR error = CMP_ConvertTexture(&src, &dst, &opts, nullptr);
			if (error == CMP_OK) {
				outCompressed.assign(dst.pData, dst.pData + dst.dwDataSize);
			} else {
				outErrorMessage = "Compressonator BC6H compression failed with error " + std::to_string(static_cast<int>(error));
			}

			std::free(dst.pData);
			return error == CMP_OK;
		}

		bool WriteNanoTex(
			const std::string& outPath,
			uint32_t width,
			uint32_t height,
			uint16_t mipCount,
			const std::vector<uint8_t>& payload,
			std::string& outErrorMessage) {
			if (!EnsureParentDirectory(std::filesystem::path(outPath))) {
				outErrorMessage = "failed to create output directory for BC6H texture";
				return false;
			}

			NE::Resource::NanoTexHeader header{};
			header.magic = NE::Resource::NTEX_MAGIC;
			header.importerVersion = NE::Resource::CURRENT_NANOTEX_FORMAT_VERSION;
			header.width = width;
			header.height = height;
			header.mipCount = mipCount;
			header.layers = 1u;
			header.isSRGB = 0u;
			header.shape = 0u;
			header.format = kNanoTexFormatBC6HUF16;

			std::ofstream ofs(outPath, std::ios::binary);
			if (!ofs.is_open()) {
				outErrorMessage = "failed to open BC6H output texture for writing";
				return false;
			}

			ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
			if (!payload.empty()) {
				ofs.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
			}

			if (!ofs.good()) {
				outErrorMessage = "failed while writing BC6H output texture";
				return false;
			}

			return true;
		}

		bool WriteLightingManifest(
			const std::string& outPath,
			const NE::Lighting::LightmapResourceBlob& blob,
			std::string& outErrorMessage) {
			if (!EnsureParentDirectory(std::filesystem::path(outPath))) {
				outErrorMessage = "failed to create output directory for lightmap manifest";
				return false;
			}

			std::vector<uint8_t> payload;
			NE::Serialization::ToBinary(payload, blob);

			NE::Resource::NanoLightingHeader header{};
			header.magic = NE::Resource::NLGT_MAGIC;
			header.version = NE::Resource::CURRENT_NANOLIGHTING_FORMAT_VERSION;
			header.payloadBytes = static_cast<uint64_t>(payload.size());

			std::ofstream ofs(outPath, std::ios::binary);
			if (!ofs.is_open()) {
				outErrorMessage = "failed to open cooked lightmap manifest for writing";
				return false;
			}

			ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
			if (!payload.empty()) {
				ofs.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
			}

			if (!ofs.good()) {
				outErrorMessage = "failed while writing cooked lightmap manifest";
				return false;
			}

			return true;
		}
	}

	bool LightmapAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
		LightmapAssetBlob sourceBlob{};
		std::string errorMessage;
		if (!LoadBlob(sourcePath, sourceBlob, errorMessage)) {
			SPD_ERROR("LightmapAsset::Cook failed to load source '" << sourcePath << "': " << errorMessage);
			return false;
		}

		if (sourceBlob.lightmapAssetId.empty()) {
			sourceBlob.lightmapAssetId = AssetManager::GetInstance().RetrieveUUID(sourcePath);
		}
		if (sourceBlob.lightmapAssetId.empty()) {
			SPD_ERROR("LightmapAsset::Cook missing lightmap asset UUID for '" << sourcePath << "'");
			return false;
		}

		const uint32_t resolvedDilationRadius = sourceBlob.bakeSettings.dilationRadiusTexels != 0u
			? sourceBlob.bakeSettings.dilationRadiusTexels
			: sourceBlob.bakeSettings.padding;

		NE::Lighting::LightmapResourceBlob cookedBlob{};
		cookedBlob.formatVersionMajor = 2;
		cookedBlob.formatVersionMinor = 0;
		cookedBlob.lightmapAssetId = sourceBlob.lightmapAssetId;
		cookedBlob.lightingRevisionId = sourceBlob.lightingRevisionId;
		cookedBlob.dependencySignature = sourceBlob.dependencySignature;
		cookedBlob.bindings = sourceBlob.bindings;
		cookedBlob.pages.reserve(sourceBlob.pages.size());

		for (const auto& page : sourceBlob.pages) {
			const uint64_t texelCount = static_cast<uint64_t>(page.width) * static_cast<uint64_t>(page.height);
			if (page.pageId.empty() ||
				page.width == 0u ||
				page.height == 0u ||
				page.lighting.size() != static_cast<size_t>(texelCount) ||
				page.validMask.size() != static_cast<size_t>(texelCount)) {
				SPD_ERROR("LightmapAsset::Cook encountered invalid canonical page data for '" << sourcePath << "'");
				return false;
			}

			std::vector<NE::Math::Vec3> cookedLighting;
			SanitizeLighting(page.lighting, cookedLighting);

			std::vector<uint8_t> cookedMask = page.validMask;
			std::vector<uint8_t> writeMask = page.dilationWriteMask;
			if (writeMask.size() != static_cast<size_t>(texelCount)) {
				writeMask.assign(static_cast<size_t>(texelCount), static_cast<uint8_t>(1u));
			}
			ApplyMaskDrivenDilation(page.width, page.height, resolvedDilationRadius, writeMask, cookedMask, cookedLighting);

			std::vector<uint8_t> pagePayload;
			uint32_t mipWidth = page.width;
			uint32_t mipHeight = page.height;
			std::vector<NE::Math::Vec3> mipLighting = std::move(cookedLighting);
			const uint32_t mipCount = Lightmapping::CalculateLightmapBakeMipCount(page.width, page.height);
			for (uint32_t mipIndex = 0u; mipIndex < mipCount; ++mipIndex) {
				std::vector<uint8_t> compressedMip;
				if (!CompressRgb32fToBC6H(mipLighting, mipWidth, mipHeight, compressedMip, errorMessage)) {
					SPD_ERROR("LightmapAsset::Cook failed to compress page '" << page.pageId << "': " << errorMessage);
					return false;
				}

				pagePayload.insert(pagePayload.end(), compressedMip.begin(), compressedMip.end());
				if (mipIndex + 1u < mipCount) {
					const uint32_t nextWidth = std::max(1u, mipWidth >> 1u);
					const uint32_t nextHeight = std::max(1u, mipHeight >> 1u);
					mipLighting = GenerateNextMip(mipLighting, mipWidth, mipHeight, nextWidth, nextHeight);
					mipWidth = nextWidth;
					mipHeight = nextHeight;
				}
			}

			const std::string textureUuid = BuildStableUuid(
				"lightmap-page:" + sourceBlob.lightmapAssetId + ":" + sourceBlob.lightingRevisionId + ":" + page.pageId);
			const std::string texturePath =
				NE::Resource::ComputeArtifactPathFromUUID(textureUuid, NE::Resource::ResourceType::Texture);
			if (!WriteNanoTex(texturePath, page.width, page.height, static_cast<uint16_t>(mipCount), pagePayload, errorMessage)) {
				SPD_ERROR("LightmapAsset::Cook failed to write page texture '" << texturePath << "': " << errorMessage);
				return false;
			}

			NE::Lighting::LightmapPageRecord cookedPage{};
			cookedPage.pageId = page.pageId;
			cookedPage.pageType = NE::Lighting::LightmapPageType::NonDirectional;
			cookedPage.width = page.width;
			cookedPage.height = page.height;
			cookedPage.mipCount = mipCount;
			cookedPage.format = "BC6H_UF16";
			cookedPage.irradianceTextureUUID = textureUuid;
			cookedBlob.pages.push_back(std::move(cookedPage));
		}

		if (!WriteLightingManifest(outPath, cookedBlob, errorMessage)) {
			SPD_ERROR("LightmapAsset::Cook failed to write manifest '" << outPath << "': " << errorMessage);
			return false;
		}

		return true;
	}

	bool LightmapAsset::LoadImportSettings(const std::string& sourcePath) {
		return std::filesystem::exists(sourcePath);
	}

	bool LightmapAsset::SaveImportSettings(const std::string& sourcePath) {
		return std::filesystem::exists(sourcePath);
	}

	bool LightmapAsset::LoadBlob(const std::string& sourcePath, LightmapAssetBlob& outBlob, std::string& outErrorMessage) {
		outBlob = {};
		outErrorMessage.clear();

		std::ifstream ifs(sourcePath, std::ios::binary | std::ios::ate);
		if (!ifs.is_open()) {
			outErrorMessage = "failed to open canonical lightmap asset";
			return false;
		}

		const std::streamsize fileSize = ifs.tellg();
		if (fileSize < static_cast<std::streamsize>(sizeof(NanoLightmapAssetHeader))) {
			outErrorMessage = "canonical lightmap asset file is truncated";
			return false;
		}

		std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
		ifs.seekg(0, std::ios::beg);
		if (!ifs.read(reinterpret_cast<char*>(bytes.data()), fileSize)) {
			outErrorMessage = "failed to read canonical lightmap asset";
			return false;
		}

		const auto* header = reinterpret_cast<const NanoLightmapAssetHeader*>(bytes.data());
		if (header->magic != NLAS_MAGIC) {
			outErrorMessage = "canonical lightmap asset magic mismatch";
			return false;
		}
		if (header->version != CURRENT_LIGHTMAP_ASSET_FILE_VERSION) {
			outErrorMessage = "canonical lightmap asset version mismatch";
			return false;
		}

		const size_t payloadOffset = sizeof(NanoLightmapAssetHeader);
		if (bytes.size() < payloadOffset + static_cast<size_t>(header->payloadBytes)) {
			outErrorMessage = "canonical lightmap asset payload is truncated";
			return false;
		}

		const uint8_t* it = bytes.data() + payloadOffset;
		const uint8_t* end = it + header->payloadBytes;
		if (!NE::Deserialization::FromBinary(it, end, outBlob)) {
			outErrorMessage = "failed to deserialize canonical lightmap asset payload";
			return false;
		}

		return true;
	}

	bool LightmapAsset::SaveBlob(const std::string& sourcePath, const LightmapAssetBlob& blob, std::string& outErrorMessage) {
		outErrorMessage.clear();
		if (!EnsureParentDirectory(std::filesystem::path(sourcePath))) {
			outErrorMessage = "failed to create canonical lightmap asset directory";
			return false;
		}

		std::vector<uint8_t> payload;
		NE::Serialization::ToBinary(payload, blob);

		NanoLightmapAssetHeader header{};
		header.magic = NLAS_MAGIC;
		header.version = CURRENT_LIGHTMAP_ASSET_FILE_VERSION;
		header.payloadBytes = static_cast<uint64_t>(payload.size());

		std::ofstream ofs(sourcePath, std::ios::binary);
		if (!ofs.is_open()) {
			outErrorMessage = "failed to open canonical lightmap asset for writing";
			return false;
		}

		ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
		if (!payload.empty()) {
			ofs.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
		}

		if (!ofs.good()) {
			outErrorMessage = "failed while writing canonical lightmap asset";
			return false;
		}

		return true;
	}
}
