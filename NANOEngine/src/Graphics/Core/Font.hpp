#pragma once

#include <vector>
#include <cstdint>
#include "NANOEngineAPI.hpp"
#include "ResourceManagement/IResource.hpp"
#include "ResourceManagement/BinaryView.hpp"
#include "ResourceManagement/BinaryHeaders/NanoFontHeader.hpp"

namespace NE::Graphics {

	class NANOENGINE_API Font final : public Resource::IResource {
	public:
		bool Preload(NE::Resource::BinaryView blob) override;
		void Finalize() override;

		static constexpr Resource::ResourceType GetStaticType() { return Resource::ResourceType::Font; }
		Resource::ResourceType GetType() const override { return GetStaticType(); }

		const std::vector<uint8_t>& GetFontData() const { return m_fontData; }
		float GetAscent100() const { return m_ascent100; }
		float GetDescent100() const { return m_descent100; }
		float GetLineHeight100() const { return m_lineHeight100; }

	private:
		std::vector<uint8_t> m_fontData;
		float m_ascent100 = 0.0f;
		float m_descent100 = 0.0f;
		float m_lineHeight100 = 0.0f;
	};

}
