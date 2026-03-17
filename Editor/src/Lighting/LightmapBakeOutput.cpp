#include "pch.h"
#include "LightmapBakeOutput.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_set>

#include <glad/glad.h>

namespace Editor::Lightmapping {
	namespace {
		constexpr float kTonemapGamma = 1.0f / 2.2f;
		constexpr float kPreviewExposureEpsilon = 1e-4f;
		constexpr uint8_t kMaskOn = 255u;

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

		float HalfToFloat(uint16_t value) {
			const uint32_t sign = static_cast<uint32_t>(value & 0x8000u) << 16;
			const uint32_t exponent = (value >> 10) & 0x1fu;
			uint32_t mantissa = value & 0x03ffu;
			uint32_t bits = 0u;

			if (exponent == 0u) {
				if (mantissa == 0u) {
					bits = sign;
				} else {
					int32_t normalizedExponent = -14;
					while ((mantissa & 0x0400u) == 0u) {
						mantissa <<= 1u;
						--normalizedExponent;
					}
					mantissa &= 0x03ffu;
					bits = sign |
						(static_cast<uint32_t>(normalizedExponent + 127) << 23u) |
						(mantissa << 13u);
				}
			} else if (exponent == 31u) {
				bits = sign | 0x7f800000u | (mantissa << 13u);
			} else {
				bits = sign | ((exponent + (127u - 15u)) << 23u) | (mantissa << 13u);
			}

			float result = 0.0f;
			std::memcpy(&result, &bits, sizeof(result));
			return result;
		}

		bool IsFiniteFloat(float value) {
			return std::isfinite(value);
		}

		size_t CountNonZeroMaskTexels(const std::vector<uint8_t>& mask) {
			return static_cast<size_t>(std::count_if(mask.begin(), mask.end(),
				[](uint8_t value) { return value != 0u; }));
		}

		struct PreparedDilationPage {
			std::vector<NE::Math::Vec3> sanitizedLighting;
			std::vector<uint8_t> originalValidMask;
			std::vector<uint8_t> dilatedValidMask;
			std::vector<uint8_t> filledValidMask;
			size_t sanitizedNonFiniteTexelCount = 0;
			size_t clampedNegativeChannelCount = 0;
			LightmapBakeDilationPageDiagnostics diagnostics{};
		};

		uint8_t ToPreviewByte(float value) {
			const float clamped = std::clamp(value, 0.0f, 1.0f);
			return static_cast<uint8_t>(std::clamp(std::pow(clamped, kTonemapGamma) * 255.0f, 0.0f, 255.0f));
		}

		void ReleaseTexture(unsigned int& textureId) {
			if (textureId != 0u) {
				glDeleteTextures(1, &textureId);
				textureId = 0u;
			}
		}

		unsigned int UploadRgbaTexture(
			GLenum internalFormat,
			GLenum uploadFormat,
			GLenum uploadType,
			uint32_t width,
			uint32_t height,
			const void* pixels,
			bool generateMipmaps,
			double* ioMipGenerationMs = nullptr) {
			if (width == 0u || height == 0u || pixels == nullptr) {
				return 0u;
			}

			const uint32_t mipCount = generateMipmaps ? CalculateLightmapBakeMipCount(width, height) : 1u;
			unsigned int texture = 0u;
			glGenTextures(1, &texture);
			if (texture == 0u) {
				return 0u;
			}

			glBindTexture(GL_TEXTURE_2D, texture);
			glTexStorage2D(GL_TEXTURE_2D, static_cast<GLsizei>(mipCount), internalFormat, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
			glTexSubImage2D(
				GL_TEXTURE_2D,
				0,
				0,
				0,
				static_cast<GLsizei>(width),
				static_cast<GLsizei>(height),
				uploadFormat,
				uploadType,
				pixels);

			if (generateMipmaps) {
				const auto mipStart = std::chrono::high_resolution_clock::now();
				glGenerateMipmap(GL_TEXTURE_2D);
				const auto mipEnd = std::chrono::high_resolution_clock::now();
				if (ioMipGenerationMs != nullptr) {
					*ioMipGenerationMs += std::chrono::duration<double, std::milli>(mipEnd - mipStart).count();
				}
			}

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
			return texture;
		}

		bool BuildMaskPreviewPixels(
			const std::vector<uint8_t>& mask,
			uint8_t onR,
			uint8_t onG,
			uint8_t onB,
			std::vector<uint8_t>& outPixels) {
			outPixels.assign(mask.size() * 4u, 0u);
			for (size_t texelIndex = 0; texelIndex < mask.size(); ++texelIndex) {
				const size_t rgbaIndex = texelIndex * 4u;
				if (mask[texelIndex] != 0u) {
					outPixels[rgbaIndex + 0u] = onR;
					outPixels[rgbaIndex + 1u] = onG;
					outPixels[rgbaIndex + 2u] = onB;
				}
				outPixels[rgbaIndex + 3u] = 255u;
			}
			return true;
		}

		bool BuildDisplayPreviewPixels(
			const LightmapBakeOutputPage& page,
			float previewExposure,
			std::vector<uint8_t>& outPixels) {
			const uint64_t texelCount = static_cast<uint64_t>(page.descriptor.width) * static_cast<uint64_t>(page.descriptor.height);
			const uint64_t expectedValues = texelCount * 4u;
			if (page.canonicalRgba16f.size() != expectedValues) {
				return false;
			}

			// Display preview refresh is intentionally derived from the packed
			// output-canonical RGBA16F page data. The original float bake buffers
			// remain the higher-precision authoring source of truth elsewhere.
			outPixels.assign(static_cast<size_t>(texelCount) * 4u, 0u);
			for (uint64_t texelIndex = 0; texelIndex < texelCount; ++texelIndex) {
				const size_t canonicalIndex = static_cast<size_t>(texelIndex) * 4u;
				const float hdrR = HalfToFloat(page.canonicalRgba16f[canonicalIndex + 0u]) * previewExposure;
				const float hdrG = HalfToFloat(page.canonicalRgba16f[canonicalIndex + 1u]) * previewExposure;
				const float hdrB = HalfToFloat(page.canonicalRgba16f[canonicalIndex + 2u]) * previewExposure;

				const float mappedR = hdrR / (1.0f + std::max(hdrR, 0.0f));
				const float mappedG = hdrG / (1.0f + std::max(hdrG, 0.0f));
				const float mappedB = hdrB / (1.0f + std::max(hdrB, 0.0f));

				const size_t previewIndex = static_cast<size_t>(texelIndex) * 4u;
				outPixels[previewIndex + 0u] = ToPreviewByte(mappedR);
				outPixels[previewIndex + 1u] = ToPreviewByte(mappedG);
				outPixels[previewIndex + 2u] = ToPreviewByte(mappedB);
				outPixels[previewIndex + 3u] = 255u;
			}

			return true;
		}

		void AppendSanitizationWarnings(
			const LightmapBakeOutputPageDescriptor& descriptor,
			size_t nonFiniteCount,
			size_t negativeCount,
			std::vector<std::string>& ioWarnings) {
			if (nonFiniteCount > 0u) {
				ioWarnings.push_back(
					descriptor.pageId + ": replaced " + std::to_string(nonFiniteCount) +
					" non-finite baked texels with black before texture upload.");
			}

			if (negativeCount > 0u) {
				ioWarnings.push_back(
					descriptor.pageId + ": clamped " + std::to_string(negativeCount) +
					" negative HDR channels to zero before texture upload.");
			}
		}

		void AppendDilationWarnings(
			const LightmapBakeOutputPageDescriptor& descriptor,
			const LightmapBakeDilationPageDiagnostics& diagnostics,
			uint32_t resolvedRadiusTexels,
			std::vector<std::string>& ioWarnings) {
			if (diagnostics.hadNoValidTexels) {
				ioWarnings.push_back(
					descriptor.pageId + ": dilation skipped because the page had no original valid texels.");
			} else if (resolvedRadiusTexels > 0u && diagnostics.filledTexelCount == 0u) {
				ioWarnings.push_back(
					descriptor.pageId + ": dilation found no writable invalid texels adjacent to valid texels.");
			}
		}

		bool SanitizePageLighting(
			const LightmapBakeOutputInputPage& inputPage,
			PreparedDilationPage& outPreparedPage) {
			const uint64_t texelCount = static_cast<uint64_t>(inputPage.width) * static_cast<uint64_t>(inputPage.height);
			if (inputPage.lighting == nullptr ||
				inputPage.validMask == nullptr ||
				inputPage.lighting->size() != static_cast<size_t>(texelCount) ||
				inputPage.validMask->size() != static_cast<size_t>(texelCount)) {
				return false;
			}

			outPreparedPage = {};
			outPreparedPage.sanitizedLighting.resize(static_cast<size_t>(texelCount), { 0.0f, 0.0f, 0.0f });
			outPreparedPage.originalValidMask = *inputPage.validMask;
			outPreparedPage.diagnostics.originalValidTexelCount = CountNonZeroMaskTexels(outPreparedPage.originalValidMask);

			for (uint64_t texelIndex = 0; texelIndex < texelCount; ++texelIndex) {
				const auto& sample = (*inputPage.lighting)[static_cast<size_t>(texelIndex)];
				float channels[3] = { sample.x, sample.y, sample.z };
				bool texelHadNonFinite = false;
				for (int channelIndex = 0; channelIndex < 3; ++channelIndex) {
					if (!IsFiniteFloat(channels[channelIndex])) {
						channels[channelIndex] = 0.0f;
						texelHadNonFinite = true;
					} else if (channels[channelIndex] < 0.0f) {
						channels[channelIndex] = 0.0f;
						++outPreparedPage.clampedNegativeChannelCount;
					}
				}

				if (texelHadNonFinite) {
					++outPreparedPage.sanitizedNonFiniteTexelCount;
				}

				auto& sanitized = outPreparedPage.sanitizedLighting[static_cast<size_t>(texelIndex)];
				sanitized.x = channels[0];
				sanitized.y = channels[1];
				sanitized.z = channels[2];
			}

			return true;
		}

		bool RunMaskDrivenDilation(
			const LightmapBakeOutputInputPage& inputPage,
			uint32_t resolvedDilationRadiusTexels,
			PreparedDilationPage& ioPreparedPage) {
			const uint64_t texelCount = static_cast<uint64_t>(inputPage.width) * static_cast<uint64_t>(inputPage.height);
			if (inputPage.dilationWriteMask == nullptr ||
				inputPage.dilationWriteMask->size() != static_cast<size_t>(texelCount)) {
				return false;
			}

			ioPreparedPage.dilatedValidMask = ioPreparedPage.originalValidMask;
			ioPreparedPage.filledValidMask.assign(static_cast<size_t>(texelCount), 0u);
			ioPreparedPage.diagnostics.finalValidTexelCount = ioPreparedPage.diagnostics.originalValidTexelCount;
			if (ioPreparedPage.diagnostics.originalValidTexelCount == 0u) {
				ioPreparedPage.diagnostics.hadNoValidTexels = true;
				return true;
			}

			if (resolvedDilationRadiusTexels == 0u) {
				return true;
			}

			static constexpr std::array<std::pair<int, int>, 8u> kNeighborOffsets{ {
				{ -1, -1 }, { 0, -1 }, { 1, -1 },
				{ -1,  0 },             { 1,  0 },
				{ -1,  1 }, { 0,  1 }, { 1,  1 }
			} };

			std::vector<NE::Math::Vec3> currentLighting = ioPreparedPage.sanitizedLighting;
			std::vector<NE::Math::Vec3> nextLighting = currentLighting;
			std::vector<uint8_t> currentMask = ioPreparedPage.originalValidMask;
			std::vector<uint8_t> nextMask = currentMask;

			for (uint32_t passIndex = 0; passIndex < resolvedDilationRadiusTexels; ++passIndex) {
				++ioPreparedPage.diagnostics.passesExecuted;
				nextLighting = currentLighting;
				nextMask = currentMask;
				size_t passFillCount = 0u;

				for (uint32_t y = 0; y < inputPage.height; ++y) {
					for (uint32_t x = 0; x < inputPage.width; ++x) {
						const size_t linearIndex = static_cast<size_t>(y) * static_cast<size_t>(inputPage.width) + static_cast<size_t>(x);
						if ((*inputPage.dilationWriteMask)[linearIndex] == 0u || currentMask[linearIndex] != 0u) {
							continue;
						}

						float accumR = 0.0f;
						float accumG = 0.0f;
						float accumB = 0.0f;
						uint32_t validNeighborCount = 0u;
						for (const auto& [offsetX, offsetY] : kNeighborOffsets) {
							const int neighborX = static_cast<int>(x) + offsetX;
							const int neighborY = static_cast<int>(y) + offsetY;
							if (neighborX < 0 ||
								neighborY < 0 ||
								neighborX >= static_cast<int>(inputPage.width) ||
								neighborY >= static_cast<int>(inputPage.height)) {
								continue;
							}

							const size_t neighborIndex =
								static_cast<size_t>(neighborY) * static_cast<size_t>(inputPage.width) +
								static_cast<size_t>(neighborX);
							if (currentMask[neighborIndex] == 0u) {
								continue;
							}

							const auto& neighbor = currentLighting[neighborIndex];
							accumR += neighbor.x;
							accumG += neighbor.y;
							accumB += neighbor.z;
							++validNeighborCount;
						}

						if (validNeighborCount == 0u) {
							continue;
						}

						const float invNeighborCount = 1.0f / static_cast<float>(validNeighborCount);
						auto& filled = nextLighting[linearIndex];
						filled.x = accumR * invNeighborCount;
						filled.y = accumG * invNeighborCount;
						filled.z = accumB * invNeighborCount;
						nextMask[linearIndex] = 1u;
						ioPreparedPage.filledValidMask[linearIndex] = 1u;
						++passFillCount;
					}
				}

				if (passFillCount == 0u) {
					ioPreparedPage.diagnostics.convergedEarly = true;
					break;
				}

				currentLighting.swap(nextLighting);
				currentMask.swap(nextMask);
			}

			ioPreparedPage.sanitizedLighting.swap(currentLighting);
			ioPreparedPage.dilatedValidMask.swap(currentMask);
			ioPreparedPage.diagnostics.filledTexelCount = CountNonZeroMaskTexels(ioPreparedPage.filledValidMask);
			ioPreparedPage.diagnostics.finalValidTexelCount = CountNonZeroMaskTexels(ioPreparedPage.dilatedValidMask);
			return true;
		}

		void PackCanonicalPage(const std::vector<NE::Math::Vec3>& lighting, std::vector<uint16_t>& outPackedRgba16f) {
			outPackedRgba16f.resize(lighting.size() * 4u, 0u);
			for (size_t texelIndex = 0; texelIndex < lighting.size(); ++texelIndex) {
				const size_t baseIndex = texelIndex * 4u;
				outPackedRgba16f[baseIndex + 0u] = FloatToHalf(lighting[texelIndex].x);
				outPackedRgba16f[baseIndex + 1u] = FloatToHalf(lighting[texelIndex].y);
				outPackedRgba16f[baseIndex + 2u] = FloatToHalf(lighting[texelIndex].z);
				outPackedRgba16f[baseIndex + 3u] = FloatToHalf(1.0f);
			}
		}
	}

	LightmapBakePreviewTextureSet::LightmapBakePreviewTextureSet(LightmapBakePreviewTextureSet&& other) noexcept {
		*this = std::move(other);
	}

	LightmapBakePreviewTextureSet& LightmapBakePreviewTextureSet::operator=(LightmapBakePreviewTextureSet&& other) noexcept {
		if (this == &other) {
			return *this;
		}

		Reset();
		hdrTexture = other.hdrTexture;
		displayTexture = other.displayTexture;
		originalValidityTexture = other.originalValidityTexture;
		dilatedValidityTexture = other.dilatedValidityTexture;
		filledValidityTexture = other.filledValidityTexture;
		ownerTexture = other.ownerTexture;
		other.hdrTexture = 0u;
		other.displayTexture = 0u;
		other.originalValidityTexture = 0u;
		other.dilatedValidityTexture = 0u;
		other.filledValidityTexture = 0u;
		other.ownerTexture = 0u;
		return *this;
	}

	LightmapBakePreviewTextureSet::~LightmapBakePreviewTextureSet() {
		Release();
	}

	void LightmapBakePreviewTextureSet::Release() {
		ReleaseTexture(hdrTexture);
		ReleaseTexture(displayTexture);
		ReleaseTexture(originalValidityTexture);
		ReleaseTexture(dilatedValidityTexture);
		ReleaseTexture(filledValidityTexture);
		ReleaseTexture(ownerTexture);
	}

	void LightmapBakePreviewTextureSet::Reset() {
		Release();
	}

	uint32_t CalculateLightmapBakeMipCount(uint32_t width, uint32_t height) {
		if (width == 0u || height == 0u) {
			return 0u;
		}

		uint32_t mipCount = 1u;
		uint32_t dimension = std::max(width, height);
		while (dimension > 1u) {
			dimension >>= 1u;
			++mipCount;
		}
		return mipCount;
	}

	void LightmapBakeTextureOutput::ReleasePreviewTextures() {
		for (auto& page : pages) {
			page.preview.Release();
		}
	}

	bool BuildLightmapBakeTextureOutput(
		const LightmapBakeOutputBuildRequest& request,
		LightmapBakeTextureOutput& outOutput,
		std::vector<std::string>& ioWarnings,
		std::string& outErrorMessage) {
		LightmapBakeTextureOutput builtOutput{};
		builtOutput.previewExposure = request.previewExposure;
		outErrorMessage.clear();
		ioWarnings.insert(ioWarnings.end(), request.prebuildWarnings.begin(), request.prebuildWarnings.end());

		std::unordered_set<int> pageIndices;
		std::unordered_set<std::string> pageIds;
		pageIndices.reserve(request.pages.size());
		pageIds.reserve(request.pages.size());

		// Preserve allocator/bake-result order exactly as supplied. Page ordering
		// is part of the published identity contract for preview, scene binding,
		// and future persistence and must not be re-sorted here.
		for (const auto& inputPage : request.pages) {
			if (inputPage.pageIndex < 0 || inputPage.pageId.empty()) {
				outErrorMessage = "Bake output stage received a page with invalid identity metadata.";
				return false;
			}

			if (!pageIndices.insert(inputPage.pageIndex).second || !pageIds.insert(inputPage.pageId).second) {
				outErrorMessage = "Bake output stage received duplicate page identity metadata.";
				return false;
			}
		}

		for (const auto& inputPage : request.pages) {
			if (inputPage.width == 0u || inputPage.height == 0u) {
				++builtOutput.diagnostics.invalidDimensionPageCount;
				outErrorMessage = "Bake output stage rejected a page with invalid dimensions.";
				return false;
			}

			const uint64_t texelCount = static_cast<uint64_t>(inputPage.width) * static_cast<uint64_t>(inputPage.height);
			if (inputPage.lighting == nullptr ||
				inputPage.validMask == nullptr ||
				inputPage.lighting->size() != static_cast<size_t>(texelCount) ||
				inputPage.validMask->size() != static_cast<size_t>(texelCount)) {
				++builtOutput.diagnostics.invalidBufferPageCount;
				outErrorMessage = "Bake output stage rejected a page with mismatched bake buffer sizes.";
				return false;
			}

			LightmapBakeOutputPage page{};
			page.descriptor.pageIndex = inputPage.pageIndex;
			page.descriptor.pageId = inputPage.pageId;
			page.descriptor.width = inputPage.width;
			page.descriptor.height = inputPage.height;
			page.descriptor.mipCount = CalculateLightmapBakeMipCount(inputPage.width, inputPage.height);
			page.descriptor.validTexelCount = inputPage.validTexelCount;
			page.descriptor.allocatedInnerTexelCount = inputPage.allocatedInnerTexelCount;
			page.descriptor.coverage01 = inputPage.coverage01;

			PreparedDilationPage preparedPage{};
			if (inputPage.dilationWriteMask == nullptr ||
				inputPage.dilationWriteMask->size() != static_cast<size_t>(texelCount) ||
				!SanitizePageLighting(inputPage, preparedPage)) {
				++builtOutput.diagnostics.invalidBufferPageCount;
				outErrorMessage = "Bake output stage rejected a page with mismatched dilation or bake buffer sizes.";
				return false;
			}

			const auto dilationStart = std::chrono::high_resolution_clock::now();
			if (!RunMaskDrivenDilation(inputPage, request.resolvedDilationRadiusTexels, preparedPage)) {
				++builtOutput.diagnostics.invalidBufferPageCount;
				outErrorMessage = "Bake output stage failed while running lightmap dilation.";
				return false;
			}
			const auto dilationEnd = std::chrono::high_resolution_clock::now();
			builtOutput.diagnostics.dilationMs +=
				std::chrono::duration<double, std::milli>(dilationEnd - dilationStart).count();

			page.originalValidMask = std::move(preparedPage.originalValidMask);
			page.dilatedValidMask = std::move(preparedPage.dilatedValidMask);
			page.filledValidMask = std::move(preparedPage.filledValidMask);
			page.sanitizedNonFiniteTexelCount = preparedPage.sanitizedNonFiniteTexelCount;
			page.clampedNegativeChannelCount = preparedPage.clampedNegativeChannelCount;
			page.dilation = preparedPage.diagnostics;
			PackCanonicalPage(preparedPage.sanitizedLighting, page.canonicalRgba16f);

			builtOutput.diagnostics.sanitizedNonFiniteTexelCount += page.sanitizedNonFiniteTexelCount;
			builtOutput.diagnostics.clampedNegativeChannelCount += page.clampedNegativeChannelCount;
			builtOutput.diagnostics.totalOriginalValidTexelCount += page.dilation.originalValidTexelCount;
			builtOutput.diagnostics.totalFilledTexelCount += page.dilation.filledTexelCount;
			builtOutput.diagnostics.totalFinalValidTexelCount += page.dilation.finalValidTexelCount;
			builtOutput.diagnostics.totalDilationPassesExecuted += page.dilation.passesExecuted;
			if (page.dilation.convergedEarly) {
				++builtOutput.diagnostics.pagesConvergedEarly;
			}
			if (page.dilation.hadNoValidTexels) {
				++builtOutput.diagnostics.pagesWithNoValidTexels;
			}

			AppendSanitizationWarnings(
				page.descriptor,
				page.sanitizedNonFiniteTexelCount,
				page.clampedNegativeChannelCount,
				ioWarnings);
			AppendDilationWarnings(page.descriptor, page.dilation, request.resolvedDilationRadiusTexels, ioWarnings);

			std::vector<uint8_t> displayPreviewPixels;
			std::vector<uint8_t> originalValidityPreviewPixels;
			std::vector<uint8_t> dilatedValidityPreviewPixels;
			std::vector<uint8_t> filledValidityPreviewPixels;
			if (!BuildDisplayPreviewPixels(page, request.previewExposure, displayPreviewPixels)) {
				++builtOutput.diagnostics.invalidBufferPageCount;
				outErrorMessage = "Bake output stage failed to build a display preview from canonical HDR data.";
				return false;
			}

			if (!BuildMaskPreviewPixels(page.originalValidMask, kMaskOn, kMaskOn, kMaskOn, originalValidityPreviewPixels) ||
				!BuildMaskPreviewPixels(page.dilatedValidMask, kMaskOn, kMaskOn, kMaskOn, dilatedValidityPreviewPixels) ||
				!BuildMaskPreviewPixels(page.filledValidMask, 72u, 200u, 96u, filledValidityPreviewPixels)) {
				++builtOutput.diagnostics.invalidBufferPageCount;
				outErrorMessage = "Bake output stage failed to build a validity preview texture.";
				return false;
			}

			const auto uploadStart = std::chrono::high_resolution_clock::now();
			page.preview.hdrTexture = UploadRgbaTexture(
				GL_RGBA16F,
				GL_RGBA,
				GL_HALF_FLOAT,
				inputPage.width,
				inputPage.height,
				page.canonicalRgba16f.data(),
				true,
				&builtOutput.diagnostics.mipGenerationMs);
			page.preview.displayTexture = UploadRgbaTexture(
				GL_RGBA8,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				inputPage.width,
				inputPage.height,
				displayPreviewPixels.data(),
				false);
			page.preview.originalValidityTexture = UploadRgbaTexture(
				GL_RGBA8,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				inputPage.width,
				inputPage.height,
				originalValidityPreviewPixels.data(),
				false);
			page.preview.dilatedValidityTexture = UploadRgbaTexture(
				GL_RGBA8,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				inputPage.width,
				inputPage.height,
				dilatedValidityPreviewPixels.data(),
				false);
			page.preview.filledValidityTexture = UploadRgbaTexture(
				GL_RGBA8,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				inputPage.width,
				inputPage.height,
				filledValidityPreviewPixels.data(),
				false);
			if (inputPage.ownerPreviewRgba8 != nullptr &&
				inputPage.ownerPreviewRgba8->size() == static_cast<size_t>(texelCount) * 4u) {
				page.preview.ownerTexture = UploadRgbaTexture(
					GL_RGBA8,
					GL_RGBA,
					GL_UNSIGNED_BYTE,
					inputPage.width,
					inputPage.height,
					inputPage.ownerPreviewRgba8->data(),
					false);
			}
			const auto uploadEnd = std::chrono::high_resolution_clock::now();
			builtOutput.diagnostics.textureCreationMs +=
				std::chrono::duration<double, std::milli>(uploadEnd - uploadStart).count();

			if (page.preview.hdrTexture == 0u ||
				page.preview.displayTexture == 0u ||
				page.preview.originalValidityTexture == 0u ||
				page.preview.dilatedValidityTexture == 0u ||
				page.preview.filledValidityTexture == 0u) {
				++builtOutput.diagnostics.textureCreationFailureCount;
				outErrorMessage = "Bake output stage failed to create one or more preview textures.";
				return false;
			}

			builtOutput.diagnostics.pageCountCreated++;
			builtOutput.diagnostics.totalPixelCountUploaded += static_cast<size_t>(texelCount);
			builtOutput.diagnostics.totalMipCount += page.descriptor.mipCount;
			builtOutput.pages.push_back(std::move(page));
		}

		outOutput = std::move(builtOutput);
		return true;
	}

	bool RefreshLightmapBakeDisplayPreviews(
		LightmapBakeTextureOutput& output,
		float previewExposure,
		std::string& outErrorMessage) {
		outErrorMessage.clear();
		if (std::fabs(output.previewExposure - previewExposure) <= kPreviewExposureEpsilon) {
			output.diagnostics.displayPreviewRefreshMs = 0.0;
			return true;
		}

		if (output.pages.empty()) {
			output.previewExposure = previewExposure;
			output.diagnostics.displayPreviewRefreshMs = 0.0;
			return true;
		}

		// This refresh path is intentionally display-only, main-thread only, and
		// non-destructive to the published HDR textures. Replacement textures are
		// built first and only swapped in after the full pass succeeds.
		std::vector<unsigned int> refreshedTextures(output.pages.size(), 0u);
		double refreshMs = 0.0;

		for (size_t pageIndex = 0; pageIndex < output.pages.size(); ++pageIndex) {
			std::vector<uint8_t> displayPreviewPixels;
			if (!BuildDisplayPreviewPixels(output.pages[pageIndex], previewExposure, displayPreviewPixels)) {
				outErrorMessage = "Bake output preview refresh failed because canonical HDR data was invalid.";
				for (auto& textureId : refreshedTextures) {
					ReleaseTexture(textureId);
				}
				return false;
			}

			const auto refreshStart = std::chrono::high_resolution_clock::now();
			refreshedTextures[pageIndex] = UploadRgbaTexture(
				GL_RGBA8,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				output.pages[pageIndex].descriptor.width,
				output.pages[pageIndex].descriptor.height,
				displayPreviewPixels.data(),
				false);
			const auto refreshEnd = std::chrono::high_resolution_clock::now();
			refreshMs += std::chrono::duration<double, std::milli>(refreshEnd - refreshStart).count();

			if (refreshedTextures[pageIndex] == 0u) {
				outErrorMessage = "Bake output preview refresh failed while creating display textures.";
				for (auto& textureId : refreshedTextures) {
					ReleaseTexture(textureId);
				}
				return false;
			}
		}

		for (size_t pageIndex = 0; pageIndex < output.pages.size(); ++pageIndex) {
			ReleaseTexture(output.pages[pageIndex].preview.displayTexture);
			output.pages[pageIndex].preview.displayTexture = refreshedTextures[pageIndex];
		}

		output.previewExposure = previewExposure;
		output.diagnostics.displayPreviewRefreshMs = refreshMs;
		return true;
	}

	bool RunLightmapBakeOutputSelfCheck(std::string& outMessage) {
		if (CalculateLightmapBakeMipCount(0u, 8u) != 0u) {
			outMessage = "Bake output self-check failed: zero-sized pages must produce zero mip levels.";
			return false;
		}

		if (CalculateLightmapBakeMipCount(1u, 1u) != 1u ||
			CalculateLightmapBakeMipCount(8u, 1u) != 4u ||
			CalculateLightmapBakeMipCount(512u, 512u) != 10u) {
			outMessage = "Bake output self-check failed: mip count calculation is inconsistent.";
			return false;
		}

		const float roundTripInputs[] = {
			0.0f,
			-0.0f,
			1.0f,
			-2.0f,
			12.5f,
			65504.0f,
			6.103515625e-5f,
			5.960464478e-8f
		};
		for (float input : roundTripInputs) {
			const float decoded = HalfToFloat(FloatToHalf(input));
			if (!std::isfinite(decoded) || std::fabs(decoded - input) > std::max(1e-3f, std::fabs(input) * 0.01f)) {
				outMessage = "Bake output self-check failed: half-float round trip drift exceeded tolerance.";
				return false;
			}
		}

		const uint16_t exactHalfPatterns[] = {
			0x0000u,
			0x8000u,
			0x0001u,
			0x03ffu,
			0x0400u,
			0x3555u,
			0x3c00u,
			0xbc00u,
			0x7bffu
		};
		for (uint16_t pattern : exactHalfPatterns) {
			const uint16_t repacked = FloatToHalf(HalfToFloat(pattern));
			if (repacked != pattern) {
				outMessage = "Bake output self-check failed: half-float exact pattern round trip is unstable.";
				return false;
			}
		}

		const float decodedInfinity = HalfToFloat(FloatToHalf(std::numeric_limits<float>::infinity()));
		if (!std::isinf(decodedInfinity)) {
			outMessage = "Bake output self-check failed: half-float infinity handling is invalid.";
			return false;
		}

		const float decodedNaN = HalfToFloat(FloatToHalf(std::numeric_limits<float>::quiet_NaN()));
		if (!std::isnan(decodedNaN)) {
			outMessage = "Bake output self-check failed: half-float NaN handling is invalid.";
			return false;
		}

		{
			std::vector<NE::Math::Vec3> lighting{
				{ std::numeric_limits<float>::quiet_NaN(), -1.0f, 2.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f }
			};
			std::vector<uint8_t> validMask{ 255u, 0u, 0u, 0u };
			std::vector<uint8_t> writeMask{ 1u, 1u, 0u, 0u };
			LightmapBakeOutputInputPage inputPage{};
			inputPage.pageIndex = 0;
			inputPage.pageId = "selfcheck_dilate";
			inputPage.width = 4u;
			inputPage.height = 1u;
			inputPage.validTexelCount = 1u;
			inputPage.lighting = &lighting;
			inputPage.validMask = &validMask;
			inputPage.dilationWriteMask = &writeMask;

			PreparedDilationPage preparedPage{};
			if (!SanitizePageLighting(inputPage, preparedPage)) {
				outMessage = "Bake output self-check failed: sanitize pass rejected a valid dilation fixture.";
				return false;
			}
			if (preparedPage.sanitizedNonFiniteTexelCount != 1u ||
				preparedPage.clampedNegativeChannelCount != 1u ||
				preparedPage.sanitizedLighting[0].x != 0.0f ||
				preparedPage.sanitizedLighting[0].y != 0.0f ||
				preparedPage.sanitizedLighting[0].z != 2.0f) {
				outMessage = "Bake output self-check failed: sanitize-before-dilate behavior is incorrect.";
				return false;
			}
			if (!RunMaskDrivenDilation(inputPage, 2u, preparedPage)) {
				outMessage = "Bake output self-check failed: dilation rejected a valid fixture.";
				return false;
			}
			if (preparedPage.diagnostics.originalValidTexelCount != 1u ||
				preparedPage.diagnostics.filledTexelCount != 1u ||
				preparedPage.diagnostics.finalValidTexelCount != 2u ||
				!preparedPage.diagnostics.convergedEarly ||
				preparedPage.filledValidMask[1] != 1u ||
				preparedPage.dilatedValidMask[2] != 0u ||
				preparedPage.sanitizedLighting[0].z != 2.0f ||
				preparedPage.sanitizedLighting[1].z != 2.0f) {
				outMessage = "Bake output self-check failed: dilation fill, bounds, or anchor preservation is invalid.";
				return false;
			}
		}

		{
			std::vector<NE::Math::Vec3> lighting{
				{ 3.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f }
			};
			std::vector<uint8_t> validMask{ 255u, 0u, 0u, 0u };
			std::vector<uint8_t> writeMask{ 1u, 1u, 1u, 1u };
			LightmapBakeOutputInputPage inputPage{};
			inputPage.pageIndex = 1;
			inputPage.pageId = "selfcheck_pingpong";
			inputPage.width = 4u;
			inputPage.height = 1u;
			inputPage.validTexelCount = 1u;
			inputPage.lighting = &lighting;
			inputPage.validMask = &validMask;
			inputPage.dilationWriteMask = &writeMask;

			PreparedDilationPage preparedPage{};
			if (!SanitizePageLighting(inputPage, preparedPage) ||
				!RunMaskDrivenDilation(inputPage, 1u, preparedPage)) {
				outMessage = "Bake output self-check failed: ping-pong dilation fixture did not run.";
				return false;
			}
			if (preparedPage.dilatedValidMask[1] != 1u || preparedPage.dilatedValidMask[2] != 0u) {
				outMessage = "Bake output self-check failed: ping-pong pass semantics allowed same-pass chaining.";
				return false;
			}
		}

		{
			std::vector<NE::Math::Vec3> lighting{
				{ 0.0f, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 0.0f }
			};
			std::vector<uint8_t> validMask{ 0u, 0u };
			std::vector<uint8_t> writeMask{ 1u, 1u };
			LightmapBakeOutputInputPage inputPage{};
			inputPage.pageIndex = 2;
			inputPage.pageId = "selfcheck_novalid";
			inputPage.width = 2u;
			inputPage.height = 1u;
			inputPage.lighting = &lighting;
			inputPage.validMask = &validMask;
			inputPage.dilationWriteMask = &writeMask;

			PreparedDilationPage preparedPage{};
			if (!SanitizePageLighting(inputPage, preparedPage) ||
				!RunMaskDrivenDilation(inputPage, 3u, preparedPage) ||
				!preparedPage.diagnostics.hadNoValidTexels ||
				preparedPage.diagnostics.finalValidTexelCount != 0u) {
				outMessage = "Bake output self-check failed: no-valid-page dilation handling is invalid.";
				return false;
			}
		}

		LightmapBakeOutputPage page{};
		page.descriptor.width = 1u;
		page.descriptor.height = 1u;
		page.canonicalRgba16f = {
			FloatToHalf(4.0f),
			FloatToHalf(1.0f),
			FloatToHalf(0.25f),
			FloatToHalf(1.0f)
		};

		std::vector<uint8_t> previewPixels;
		if (!BuildDisplayPreviewPixels(page, 1.0f, previewPixels) || previewPixels.size() != 4u) {
			outMessage = "Bake output self-check failed: display preview generation from canonical HDR data is invalid.";
			return false;
		}

		if (previewPixels[0] <= previewPixels[2]) {
			outMessage = "Bake output self-check failed: tone-mapped preview ordering is unexpected.";
			return false;
		}

		outMessage = "Bake output self-check passed: mip counts, sanitize+dilate behavior, half-float packing edge cases, and preview derivation are valid.";
		return true;
	}
}
