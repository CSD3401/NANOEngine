#pragma once

#include <string>
#include <vector>

#include "Core/Reflection.hpp"
#include "ResourceManagement/BinaryView.hpp"
#include "ResourceManagement/IResource.hpp"

namespace NE::Lighting {

	enum class LightmapPageType : uint8_t {
		NonDirectional = 0,
		Directional = 1
	};

	struct LightmapPageRecord {
		std::string pageId;
		LightmapPageType pageType = LightmapPageType::NonDirectional;
		uint32_t width = 0;
		uint32_t height = 0;
		std::string format;
		std::string irradianceTextureUUID;
		std::string directionTextureUUID;

		NE_REFLECT_BEGIN(LightmapPageRecord)
			NE_REFLECT_FIELD(pageId),
			NE_REFLECT_FIELD(pageType),
			NE_REFLECT_FIELD(width),
			NE_REFLECT_FIELD(height),
			NE_REFLECT_FIELD(format),
			NE_REFLECT_FIELD(irradianceTextureUUID),
			NE_REFLECT_FIELD(directionTextureUUID)
		NE_REFLECT_END()
	};

	struct LightingAssetBlob {
		uint16_t formatVersionMajor = 1;
		uint16_t formatVersionMinor = 0;
		std::string lightingAssetId;
		std::string lightingRevisionId;
		std::string dependencySignature;
		std::vector<LightmapPageRecord> pages;

		NE_REFLECT_BEGIN(LightingAssetBlob)
			NE_REFLECT_FIELD(formatVersionMajor),
			NE_REFLECT_FIELD(formatVersionMinor),
			NE_REFLECT_FIELD(lightingAssetId),
			NE_REFLECT_FIELD(lightingRevisionId),
			NE_REFLECT_FIELD(dependencySignature),
			NE_REFLECT_FIELD(pages)
		NE_REFLECT_END()
	};

	class LightingAsset final : public Resource::IResource {
	public:
		bool Preload(Resource::BinaryView blob) override;
		void Finalize() override;

		static constexpr Resource::ResourceType GetStaticType() { return Resource::ResourceType::Lighting; }
		Resource::ResourceType GetType() const override { return GetStaticType(); }

		uint16_t GetFormatVersionMajor() const { return m_data.formatVersionMajor; }
		uint16_t GetFormatVersionMinor() const { return m_data.formatVersionMinor; }
		const std::string& GetLightingAssetId() const { return m_data.lightingAssetId; }
		const std::string& GetLightingRevisionId() const { return m_data.lightingRevisionId; }
		const std::string& GetDependencySignature() const { return m_data.dependencySignature; }
		const std::vector<LightmapPageRecord>& GetPages() const { return m_data.pages; }

		std::string uuid;

	private:
		LightingAssetBlob m_data;
	};

}
