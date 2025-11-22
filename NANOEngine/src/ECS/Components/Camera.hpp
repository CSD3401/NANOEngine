#pragma once

#include "../../../src/Math/Vec3.hpp"
#include "../../../src/Math/Mat4.hpp"
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {
	using NE::Math::Vec3;
	using NE::Math::Mat4;

	using RenderViewHandle = std::uint32_t;

	struct Camera {
		Mat4 viewMtx;
		Mat4 projectionMtx;

		float fovY{ 45.0f };
		float aspectRatio{ 16.0f / 9.0f };
		float nearPlane{ 0.1f };
		float farPlane{ 1000.0f };

		bool isMain{ true }; // This camera is the main camera used for rendering to screen
		bool isActive{ true }; // This camera is active and will be rendered, but not necessarily sent to screen

		// Flag tracks the projection matrix needs to be rebuilt, view matrix is tracked via transform component
		bool isDirty{ true }; 
		uint64_t luid;

		// The render view handle associated with this camera
		std::vector<RenderViewHandle> renderViewHandles;

		NE_REFLECT_BEGIN(Camera)
			NE_REFLECT_FIELD_NAMED(fovY, "FOV Y"),
			NE_REFLECT_FIELD_NAMED(aspectRatio, "Aspect Ratio"),
			NE_REFLECT_FIELD_NAMED(nearPlane, "Near Plane"),
			NE_REFLECT_FIELD_NAMED(farPlane, "Far Plane"),
			NE_REFLECT_FIELD_NAMED(isMain, "Is Main Camera"),
			NE_REFLECT_FIELD_NAMED(isActive, "Is Active Camera")
		NE_REFLECT_END()

	};
}