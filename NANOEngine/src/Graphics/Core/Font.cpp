#include "Font.hpp"
#include "Core/SpdLogger.hpp"
#include <cstring>

namespace NE::Graphics {

	bool Font::Preload(NE::Resource::BinaryView blobView) {
		using NE::Resource::NanoFontHeader;
		using NE::Resource::NFNT_MAGIC;
		using NE::Resource::CURRENT_NANOFONT_FORMAT_VERSION;

		// Validate minimum size
		if (blobView.size < sizeof(NanoFontHeader)) {
			SPD_ERROR("[Font] Binary too small for header");
			return false;
		}

		// Read header
		const auto* hdr = blobView.as<NanoFontHeader>(0);
		if (!hdr) {
			SPD_ERROR("[Font] Failed to read header");
			return false;
		}

		// Validate magic
		if (hdr->magic != NFNT_MAGIC) {
			SPD_ERROR("[Font] Invalid magic: expected 0x{:08X}, got 0x{:08X}", NFNT_MAGIC, hdr->magic);
			return false;
		}

		// Validate version
		if (hdr->version != CURRENT_NANOFONT_FORMAT_VERSION) {
			SPD_ERROR("[Font] Unsupported version: {}", hdr->version);
			return false;
		}

		// Validate TTF data size
		const size_t headerSize = sizeof(NanoFontHeader);
		const size_t availableDataSize = blobView.size > headerSize ? (blobView.size - headerSize) : 0;

		if (hdr->ttfDataSize != availableDataSize) {
			SPD_ERROR("[Font] TTF data size mismatch: header says {}, available {}",
				hdr->ttfDataSize, availableDataSize);
			return false;
		}

		if (hdr->ttfDataSize == 0) {
			SPD_ERROR("[Font] Empty TTF data");
			return false;
		}

		// Copy TTF data
		const uint8_t* ttfData = blobView.at(headerSize, hdr->ttfDataSize);
		if (!ttfData) {
			SPD_ERROR("[Font] Failed to read TTF data");
			return false;
		}

		m_fontData.resize(hdr->ttfDataSize);
		std::memcpy(m_fontData.data(), ttfData, hdr->ttfDataSize);

		// Copy pre-computed metrics
		m_ascent100 = hdr->ascent100;
		m_descent100 = hdr->descent100;
		m_lineHeight100 = hdr->lineHeight100;

		return true;
	}

	void Font::Finalize() {
		// No GPU resources to allocate for Font
		// FontAtlas will handle runtime atlas generation
	}

}
