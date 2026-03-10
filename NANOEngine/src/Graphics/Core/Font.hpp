#pragma once

#include <vector>
#include <cstdint>
#include "NANOEngineAPI.hpp"
#include "ResourceManagement/IResource.hpp"
#include "ResourceManagement/BinaryView.hpp"
#include "ResourceManagement/BinaryHeaders/NanoFontHeader.hpp"

namespace NE::Graphics {

	class  Font final : public Resource::IResource {
	public:
		NANOENGINE_API bool Preload(NE::Resource::BinaryView blob) override;
		NANOENGINE_API void Finalize() override;

		NANOENGINE_API static constexpr Resource::ResourceType GetStaticType() { return Resource::ResourceType::Font; }
		NANOENGINE_API Resource::ResourceType GetType() const override { return GetStaticType(); }

		NANOENGINE_API const std::vector<uint8_t>& GetFontData() const { return m_fontData; }
		NANOENGINE_API float GetAscent100() const { return m_ascent100; }
		NANOENGINE_API float GetDescent100() const { return m_descent100; }
		NANOENGINE_API float GetLineHeight100() const { return m_lineHeight100; }

	private:
		std::vector<uint8_t> m_fontData;
		float m_ascent100 = 0.0f;
		float m_descent100 = 0.0f;
		float m_lineHeight100 = 0.0f;
	};

}
