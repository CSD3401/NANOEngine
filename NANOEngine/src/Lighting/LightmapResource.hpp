#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Reflection.hpp"
#include "Graphics/OpenGL/GLTexture.hpp"
#include "Math/Vec2.hpp"
#include "ResourceManagement/BinaryView.hpp"
#include "ResourceManagement/IResource.hpp"

namespace NE::Lighting {

	enum class LightmapPageType : uint8_t {
		NonDirectional = 0,
		Directional = 1
	};

	struct LightmapBindingRecord {
		uint64_t entityLuid = 0;
		uint32_t subMeshIndex = 0;
		std::string pageId;
		Math::Vec2 uvScale{ 1.0f, 1.0f };
		Math::Vec2 uvOffset{ 0.0f, 0.0f };

		NE_REFLECT_BEGIN(LightmapBindingRecord)
			NE_REFLECT_FIELD(entityLuid),
			NE_REFLECT_FIELD(subMeshIndex),
			NE_REFLECT_FIELD(pageId),
			NE_REFLECT_FIELD(uvScale),
			NE_REFLECT_FIELD(uvOffset)
		NE_REFLECT_END()
	};

	struct LightmapPageRecord {
		std::string pageId;
		LightmapPageType pageType = LightmapPageType::NonDirectional;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t mipCount = 0;
		std::string format;
		std::string irradianceTextureUUID;
		std::string directionTextureUUID;

		NE_REFLECT_BEGIN(LightmapPageRecord)
			NE_REFLECT_FIELD(pageId),
			NE_REFLECT_FIELD(pageType),
			NE_REFLECT_FIELD(width),
			NE_REFLECT_FIELD(height),
			NE_REFLECT_FIELD(mipCount),
			NE_REFLECT_FIELD(format),
			NE_REFLECT_FIELD(irradianceTextureUUID),
			NE_REFLECT_FIELD(directionTextureUUID)
		NE_REFLECT_END()
	};

	struct LightmapResourceBlob {
		uint16_t formatVersionMajor = 2;
		uint16_t formatVersionMinor = 0;
		std::string lightmapAssetId;
		std::string lightingRevisionId;
		std::string dependencySignature;
		std::vector<LightmapPageRecord> pages;
		std::vector<LightmapBindingRecord> bindings;

		NE_REFLECT_BEGIN(LightmapResourceBlob)
			NE_REFLECT_FIELD(formatVersionMajor),
			NE_REFLECT_FIELD(formatVersionMinor),
			NE_REFLECT_FIELD(lightmapAssetId),
			NE_REFLECT_FIELD(lightingRevisionId),
			NE_REFLECT_FIELD(dependencySignature),
			NE_REFLECT_FIELD(pages),
			NE_REFLECT_FIELD(bindings)
		NE_REFLECT_END()
	};

	struct LightmapResourcePage {
		LightmapPageRecord manifest{};
		std::shared_ptr<Graphics::OpenGL::GLTexture> irradianceTexture;
		std::uint64_t irradianceHandle = 0;
		bool resolved = false;
		std::string failureReason;
	};

	class LightmapResource final : public Resource::IResource {
	public:
		bool Preload(Resource::BinaryView blob) override;
		void Finalize() override;

		static constexpr Resource::ResourceType GetStaticType() { return Resource::ResourceType::Lighting; }
		Resource::ResourceType GetType() const override { return GetStaticType(); }

		uint16_t GetFormatVersionMajor() const { return m_data.formatVersionMajor; }
		uint16_t GetFormatVersionMinor() const { return m_data.formatVersionMinor; }
		const std::string& GetLightmapAssetId() const { return m_data.lightmapAssetId; }
		const std::string& GetLightingRevisionId() const { return m_data.lightingRevisionId; }
		const std::string& GetDependencySignature() const { return m_data.dependencySignature; }
		const std::vector<LightmapBindingRecord>& GetBindings() const { return m_data.bindings; }
		const std::vector<LightmapResourcePage>& GetPages() const { return m_pages; }
		const std::unordered_map<std::string, std::uint32_t>& GetPageSlots() const { return m_pageSlots; }
		const std::vector<std::uint64_t>& GetIrradianceHandles() const { return m_irradianceHandles; }
		bool IsManifestResolved() const { return m_manifestResolved; }
		bool IsUsable() const { return m_usable; }
		const std::string& GetFailureReason() const { return m_failureReason; }

		std::string uuid;

	private:
		static constexpr std::uint32_t kMaxRuntimePages = 128u;

		LightmapResourceBlob m_data;
		std::vector<LightmapResourcePage> m_pages;
		std::unordered_map<std::string, std::uint32_t> m_pageSlots;
		std::vector<std::uint64_t> m_irradianceHandles;
		bool m_manifestResolved = false;
		bool m_usable = false;
		std::string m_failureReason;
	};

}
