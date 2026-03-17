#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Math/Vec3.hpp>

namespace Editor::Lightmapping {

	struct LightmapBakeOutputInputPage {
		int pageIndex = -1;
		std::string pageId;
		uint32_t width = 0;
		uint32_t height = 0;
		size_t validTexelCount = 0;
		size_t allocatedInnerTexelCount = 0;
		float coverage01 = 0.0f;
		const std::vector<NE::Math::Vec3>* lighting = nullptr;
		const std::vector<uint8_t>* validMask = nullptr;
		const std::vector<uint8_t>* dilationWriteMask = nullptr;
		const std::vector<uint8_t>* ownerPreviewRgba8 = nullptr;
	};

	struct LightmapBakeOutputBuildRequest {
		float previewExposure = 1.0f;
		uint32_t resolvedDilationRadiusTexels = 0;
		std::vector<std::vector<uint8_t>> pageDilationWriteMasks;
		std::vector<std::string> prebuildWarnings;
		std::vector<LightmapBakeOutputInputPage> pages;
	};

	struct LightmapBakeOutputPageDescriptor {
		int pageIndex = -1;
		std::string pageId;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t mipCount = 0;
		size_t validTexelCount = 0;
		size_t allocatedInnerTexelCount = 0;
		float coverage01 = 0.0f;
		std::string format = "RGBA16F_LINEAR";
	};

	struct LightmapBakePreviewTextureSet {
		unsigned int hdrTexture = 0;
		unsigned int displayTexture = 0;
		unsigned int originalValidityTexture = 0;
		unsigned int dilatedValidityTexture = 0;
		unsigned int filledValidityTexture = 0;
		unsigned int ownerTexture = 0;

		LightmapBakePreviewTextureSet() = default;
		LightmapBakePreviewTextureSet(const LightmapBakePreviewTextureSet&) = delete;
		LightmapBakePreviewTextureSet& operator=(const LightmapBakePreviewTextureSet&) = delete;
		LightmapBakePreviewTextureSet(LightmapBakePreviewTextureSet&& other) noexcept;
		LightmapBakePreviewTextureSet& operator=(LightmapBakePreviewTextureSet&& other) noexcept;
		~LightmapBakePreviewTextureSet();

		void Release();
		void Reset();
	};

	struct LightmapBakeDilationPageDiagnostics {
		size_t originalValidTexelCount = 0;
		size_t filledTexelCount = 0;
		size_t finalValidTexelCount = 0;
		uint32_t passesExecuted = 0;
		bool convergedEarly = false;
		bool hadNoValidTexels = false;
	};

	struct LightmapBakeOutputPage {
		LightmapBakeOutputPageDescriptor descriptor{};
		// Output canonical page storage: packed linear RGBA16F derived from the
		// authoring-quality float bake buffers. Display preview refreshes rebuild
		// from this packed output representation, not from the original float data.
		std::vector<uint16_t> canonicalRgba16f;
		std::vector<uint8_t> originalValidMask;
		std::vector<uint8_t> dilatedValidMask;
		std::vector<uint8_t> filledValidMask;
		size_t sanitizedNonFiniteTexelCount = 0;
		size_t clampedNegativeChannelCount = 0;
		LightmapBakeDilationPageDiagnostics dilation{};
		LightmapBakePreviewTextureSet preview{};

		LightmapBakeOutputPage() = default;
		LightmapBakeOutputPage(const LightmapBakeOutputPage&) = delete;
		LightmapBakeOutputPage& operator=(const LightmapBakeOutputPage&) = delete;
		LightmapBakeOutputPage(LightmapBakeOutputPage&&) noexcept = default;
		LightmapBakeOutputPage& operator=(LightmapBakeOutputPage&&) noexcept = default;
	};

	struct LightmapBakeTextureDiagnostics {
		size_t pageCountCreated = 0;
		size_t pageCountSkipped = 0;
		size_t totalPixelCountUploaded = 0;
		size_t totalMipCount = 0;
		size_t totalOriginalValidTexelCount = 0;
		size_t totalFilledTexelCount = 0;
		size_t totalFinalValidTexelCount = 0;
		size_t totalDilationPassesExecuted = 0;
		size_t pagesConvergedEarly = 0;
		size_t pagesWithNoValidTexels = 0;
		size_t sanitizedNonFiniteTexelCount = 0;
		size_t clampedNegativeChannelCount = 0;
		size_t invalidDimensionPageCount = 0;
		size_t invalidBufferPageCount = 0;
		size_t textureCreationFailureCount = 0;
		double textureCreationMs = 0.0;
		double dilationMs = 0.0;
		double mipGenerationMs = 0.0;
		double displayPreviewRefreshMs = 0.0;
	};

	struct LightmapBakeTextureOutput {
		uint64_t sourceBakeRevision = 0;
		float previewExposure = 1.0f;
		LightmapBakeTextureDiagnostics diagnostics{};
		std::vector<LightmapBakeOutputPage> pages;

		LightmapBakeTextureOutput() = default;
		LightmapBakeTextureOutput(const LightmapBakeTextureOutput&) = delete;
		LightmapBakeTextureOutput& operator=(const LightmapBakeTextureOutput&) = delete;
		LightmapBakeTextureOutput(LightmapBakeTextureOutput&&) noexcept = default;
		LightmapBakeTextureOutput& operator=(LightmapBakeTextureOutput&&) noexcept = default;

		void ReleasePreviewTextures();
	};

	uint32_t CalculateLightmapBakeMipCount(uint32_t width, uint32_t height);

	bool BuildLightmapBakeTextureOutput(
		const LightmapBakeOutputBuildRequest& request,
		LightmapBakeTextureOutput& outOutput,
		std::vector<std::string>& ioWarnings,
		std::string& outErrorMessage);

	bool RefreshLightmapBakeDisplayPreviews(
		LightmapBakeTextureOutput& output,
		float previewExposure,
		std::string& outErrorMessage);

	bool RunLightmapBakeOutputSelfCheck(std::string& outMessage);

}
