#ifndef RENDER_SETTINGS_HPP
#define RENDER_SETTINGS_HPP
#include <cstdint>
#include "Core/Reflection.hpp"
#include "Math/Vec3.hpp"

namespace NE::Graphics {
	struct RenderSettings {
		// Environment Lighting
		enum EnvSource : uint8_t {
			Skybox,
			Gradient,
			Color
		};

		EnvSource envSource = EnvSource::Color;
		Math::Vec3 ambientColour{ 0.5f, 0.5f, 0.5f };
		float ambientIntensity = 0.f;

		// Fog
		enum FogMode : uint8_t {
			Linear,
			Exponential,
			ExponentialSquared
		};

		bool fogEnabled = false;
		FogMode fogMode = FogMode::Linear;
		Math::Vec3 fogColour{ 0.5f, 0.5f, 0.5f };

		// Linear settings
		float fogStart = 0.f;
		float fogEnd = 100.f;

		// Exp settings
		float fogDensity = 0.1f;

		NE_REFLECT_BEGIN(RenderSettings)
			NE_REFLECT_FIELD(envSource),
			NE_REFLECT_FIELD(ambientColour),
			NE_REFLECT_FIELD(ambientIntensity),
			NE_REFLECT_FIELD(fogEnabled),
			NE_REFLECT_FIELD(fogMode),
			NE_REFLECT_FIELD(fogColour),
			NE_REFLECT_FIELD(fogStart),
			NE_REFLECT_FIELD(fogEnd),
			NE_REFLECT_FIELD(fogDensity)
		NE_REFLECT_END()
	};
}

#endif