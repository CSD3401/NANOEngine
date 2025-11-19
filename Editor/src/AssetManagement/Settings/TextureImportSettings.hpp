#ifndef TEXTURE_IMPORT_SETTINGS_HPP
#define TEXTURE_IMPORT_SETTINGS_HPP

#include <cstdint>
#include <Core/Reflection.hpp>

namespace Editor {

	inline constexpr int TEXTURE_IMPORTER_VERSION = 1;

	enum class TexType : uint8_t {
		Default,
		NormalMap,
		Sprite
	};

	enum class TexShape : uint8_t {
		TwoD,
		Cube,
		TwoDArray
	};

	enum class TexWrapMode : uint8_t {
		Repeat,
		Clamp,
		Mirror,
		MirrorOnce,
		PerAxis,
	};

	enum class TexFilterMode : uint8_t {
		Point,
		Bilinear,
		Trilinear
	};

	enum class TexAlphaSource : uint8_t {
		InputTextureAlpha,
		GrayscaleSource,
		None
	};

	struct NormalMapOptions {
		bool flipGreenChannel = false;
		bool createFromGrayscale = false;

		NE_REFLECT_BEGIN(NormalMapOptions)
			NE_REFLECT_FIELD(flipGreenChannel),
			NE_REFLECT_FIELD(createFromGrayscale)
		NE_REFLECT_END()
	};

	struct MipPolicy {
		bool generateMipmap = true;
		bool preserveCoverage = false;

		NE_REFLECT_BEGIN(MipPolicy)
			NE_REFLECT_FIELD(generateMipmap),
			NE_REFLECT_FIELD(preserveCoverage)
		NE_REFLECT_END()
	};

	struct TextureImportSettings {
		TexType type = TexType::Default;
		TexShape shape = TexShape::TwoD;
		bool sRGB = true;
		TexAlphaSource alphaSource = TexAlphaSource::InputTextureAlpha;
		bool alphaIsTransparency = false;

		TexWrapMode wrapMode = TexWrapMode::Repeat;
		TexFilterMode filterMode = TexFilterMode::Bilinear;

		MipPolicy mips{};
		NormalMapOptions normal{};


		NE_REFLECT_BEGIN(TextureImportSettings)
			NE_REFLECT_FIELD(type),
			NE_REFLECT_FIELD(shape),
			NE_REFLECT_FIELD(sRGB),
			NE_REFLECT_FIELD(alphaSource),
			NE_REFLECT_FIELD(alphaIsTransparency),
			NE_REFLECT_FIELD(wrapMode),
			NE_REFLECT_FIELD(filterMode),
			NE_REFLECT_FIELD(mips),
			NE_REFLECT_FIELD(normal)
		NE_REFLECT_END()
	};

}


#endif // !TEXTURE_IMPORT_SETTINGS_HPP
