#ifndef TEXTURE_IMPORT_SETTINGS_HPP
#define TEXTURE_IMPORT_SETTINGS_HPP

#include <cstdint>

namespace Editor {

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
	};

	struct MipPolicy {
		bool generateMipmap = true;
		bool preserveCoverage = false;
	};

	struct TextureImportSettings {
		TexType type = TexType::Sprite;
		TexShape shape = TexShape::TwoD;
		bool sRGB = true;
		TexAlphaSource alpha = TexAlphaSource::InputTextureAlpha;
		bool alphaIsTransparency = false;

		TexWrapMode wrap = TexWrapMode::Clamp;
		TexFilterMode filer = TexFilterMode::Bilinear;

		MipPolicy mips{};
		NormalMapOptions normal{};
	};

}


#endif // !TEXTURE_IMPORT_SETTINGS_HPP
