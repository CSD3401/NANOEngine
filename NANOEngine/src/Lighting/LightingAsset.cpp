#include "pch.h"
#include "LightingAsset.hpp"

#include "Core/SpdLogger.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "ResourceManagement/BinaryHeaders/NanoLightingHeader.hpp"
#include "Serialisation/BinaryReflection.hpp"

namespace NE::Lighting {
	namespace {
		bool IsPageManifestValid(const LightmapPageRecord& page, std::string& outFailureReason) {
			if (page.pageId.empty()) {
				outFailureReason = "missing page id";
				return false;
			}

			if (page.width == 0u || page.height == 0u) {
				outFailureReason = "invalid page dimensions";
				return false;
			}

			if (page.irradianceTextureUUID.empty()) {
				outFailureReason = "missing irradiance texture";
				return false;
			}

			if (page.pageType == LightmapPageType::Directional) {
				outFailureReason = "directional pages are not supported in runtime v1";
				return false;
			}

			return true;
		}

		void PopulateFallbackHandles(std::vector<std::uint64_t>& handles) {
			std::uint64_t fallbackHandle = 0u;
			for (const std::uint64_t handle : handles) {
				if (handle != 0u) {
					fallbackHandle = handle;
					break;
				}
			}

			if (fallbackHandle == 0u) {
				return;
			}

			for (auto& handle : handles) {
				if (handle == 0u) {
					handle = fallbackHandle;
				}
			}
		}
	}

	bool LightmapResource::Preload(Resource::BinaryView blob) {
		m_data = {};
		m_pages.clear();
		m_pageSlots.clear();
		m_irradianceHandles.clear();
		m_manifestResolved = false;
		m_usable = false;
		m_failureReason.clear();

		if (blob.size < sizeof(Resource::NanoLightingHeader)) {
			return false;
		}

		const auto* hdr = blob.as<Resource::NanoLightingHeader>(0);
		if (!hdr) return false;
		if (hdr->magic != Resource::NLGT_MAGIC) return false;

		if (hdr->version != Resource::CURRENT_NANOLIGHTING_FORMAT_VERSION) {
			SPD_ERROR("NanoLighting version mismatch (got " << hdr->version
				<< ", expected " << Resource::CURRENT_NANOLIGHTING_FORMAT_VERSION
				<< "). Re-bake or recook the lighting asset.");
			return false;
		}

		const size_t payloadOffset = sizeof(Resource::NanoLightingHeader);
		if (blob.size < payloadOffset + static_cast<size_t>(hdr->payloadBytes)) {
			return false;
		}

		const uint8_t* it = blob.data + payloadOffset;
		const uint8_t* end = it + hdr->payloadBytes;

		LightmapResourceBlob parsed{};
		if (!NE::Deserialization::FromBinary(it, end, parsed)) {
			return false;
		}

		m_data = std::move(parsed);
		return true;
	}

	void LightmapResource::Finalize() {
		m_pages.clear();
		m_pageSlots.clear();
		m_irradianceHandles.assign(
			std::min<std::size_t>(m_data.pages.size(), static_cast<std::size_t>(kMaxRuntimePages)),
			0ull);
		m_manifestResolved = false;
		m_usable = false;
		m_failureReason.clear();

		std::size_t resolvedPageCount = 0u;
		for (std::uint32_t pageSlot = 0u; pageSlot < m_data.pages.size(); ++pageSlot) {
			LightmapResourcePage page{};
			page.manifest = m_data.pages[pageSlot];

			std::string failureReason;
			if (!IsPageManifestValid(page.manifest, failureReason)) {
				page.failureReason = std::move(failureReason);
				m_pages.push_back(std::move(page));
				continue;
			}

			if (pageSlot >= kMaxRuntimePages) {
				page.failureReason = "page exceeds runtime page limit";
				m_pages.push_back(std::move(page));
				continue;
			}

			const auto [itPageSlot, inserted] = m_pageSlots.emplace(page.manifest.pageId, pageSlot);
			if (!inserted) {
				(void)itPageSlot;
				page.failureReason = "duplicate page id";
				m_pages.push_back(std::move(page));
				continue;
			}

			page.irradianceTexture =
				Resource::ResourceManager::GetInstance().LoadResource<Graphics::OpenGL::GLTexture>(
					page.manifest.irradianceTextureUUID);
			if (!page.irradianceTexture) {
				page.failureReason = "failed to load irradiance texture";
				m_pages.push_back(std::move(page));
				continue;
			}

			page.irradianceTexture->MakeResident();
			page.irradianceHandle = page.irradianceTexture->GetClampBindlessHandle();
			page.resolved = (page.irradianceHandle != 0u);
			if (!page.resolved) {
				page.failureReason = "invalid irradiance bindless handle";
				m_pages.push_back(std::move(page));
				continue;
			}

			m_irradianceHandles[pageSlot] = page.irradianceHandle;
			++resolvedPageCount;
			m_pages.push_back(std::move(page));
		}

		PopulateFallbackHandles(m_irradianceHandles);
		m_manifestResolved = !m_pageSlots.empty();
		m_usable = resolvedPageCount > 0u;

		if (!m_manifestResolved) {
			m_failureReason = "lightmap resource contains no resolvable pages";
		} else if (!m_usable) {
			m_failureReason = "lightmap resource resolved no usable runtime pages";
		}
	}

}
