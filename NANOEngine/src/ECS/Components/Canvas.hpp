#ifndef CANVAS_HPP
#define CANVAS_HPP

#include <cstdint>
#include <variant>

#include "Math/Vec2.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {
	struct Canvas {
		struct ScreenSpaceOverlayData {
			int sortOrder = 0;
			bool pixelPerfect = false;
			NE_REFLECT_BEGIN(ScreenSpaceOverlayData)
				NE_REFLECT_FIELD(sortOrder),
				NE_REFLECT_FIELD(pixelPerfect)
				NE_REFLECT_END()
		};

		struct ScreenSpaceCameraData {
			uint64_t cameraLUID = 0;
			float planeDistaance = 100.f;
			bool pixelPerfect = false;
			bool resizeCanvas = true;

			// if needed next time
			// sortinglayer
			// order in layer
			NE_REFLECT_BEGIN(ScreenSpaceCameraData)
				NE_REFLECT_FIELD(cameraLUID),
				NE_REFLECT_FIELD(planeDistaance),
				NE_REFLECT_FIELD(pixelPerfect),
				NE_REFLECT_FIELD(resizeCanvas)
				NE_REFLECT_END()
		};

		struct WorldSpaceData {
			uint64_t eventCameraLUID = 0;
			// if needed next time
			// sortinglayer
			// order in layer
			NE_REFLECT_BEGIN(WorldSpaceData)
				NE_REFLECT_FIELD(eventCameraLUID)
				NE_REFLECT_END()
		};

		struct ConstantPixelScaleData {
			float scaleFactor = 1.f;
			float referencePixelsPerUnit = 100.f;

			NE_REFLECT_BEGIN(ConstantPixelScaleData)
				NE_REFLECT_FIELD(scaleFactor),
				NE_REFLECT_FIELD(referencePixelsPerUnit)
				NE_REFLECT_END()
		};

		struct ScreenSizeScaleData {
			Math::Vec2 referenceResolution{ 800.f, 600.f };
			float referencePixelsPerUnit = 100.f;

			NE_REFLECT_BEGIN(ScreenSizeScaleData)
				NE_REFLECT_FIELD(referenceResolution),
				NE_REFLECT_FIELD(referencePixelsPerUnit)
				NE_REFLECT_END()
		};

		struct ConstantPhysicalScaleData {
			enum class PhysicalUnit : uint8_t {
				CENTIMETERS,
				MILLIMETERS,
				INCHES,
				POINTS,
				PICAS
			};

			float fallbackScreenDPI = 96.f;
			float defaultSpriteDPI = 96.f;
			float referencePixelsPerUnit = 100.f;
			PhysicalUnit physicalUnit = PhysicalUnit::MILLIMETERS;

			NE_REFLECT_BEGIN(ConstantPhysicalScaleData)
				NE_REFLECT_FIELD(physicalUnit),
				NE_REFLECT_FIELD(fallbackScreenDPI),
				NE_REFLECT_FIELD(defaultSpriteDPI),
				NE_REFLECT_FIELD(referencePixelsPerUnit)
				NE_REFLECT_END()
		};

		using RenderModeData = std::variant<
			ScreenSpaceOverlayData,
			ScreenSpaceCameraData,
			WorldSpaceData
		>;

		using ScaleModeData = std::variant <
			ConstantPixelScaleData,
			ScreenSizeScaleData,
			ConstantPhysicalScaleData
		>;

		enum class RenderMode : uint8_t {
			SCREEN_SPACE_OVERLAY, // Always on top, no camera needed
			SCREEN_SPACE_CAMERA,  // Rendered by specific camera
			WORLD_SPACE           // Exists in 3D world
		};

		enum class ScaleMode : uint8_t {
			CONSTANT_PIXEL_SIZE,
			SCALE_WITH_SCREEN_SIZE,
			CONSTANT_PHYSICAL_SIZE
		};

		RenderModeData renderModeData;
		ScaleModeData scaleModeData;
		uint64_t luid{};
		RenderMode renderMode = RenderMode::SCREEN_SPACE_OVERLAY;
		ScaleMode scaleMode = ScaleMode::CONSTANT_PIXEL_SIZE;

		NE_REFLECT_BEGIN(Canvas)
			NE_REFLECT_FIELD_HIDDEN(luid),
			NE_REFLECT_FIELD_HIDDEN(renderMode),
			NE_REFLECT_FIELD_HIDDEN(renderModeData),
			NE_REFLECT_FIELD_HIDDEN(scaleMode),
			NE_REFLECT_FIELD_HIDDEN(renderModeData)
			NE_REFLECT_END()
	};
}

#endif