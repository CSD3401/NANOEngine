#pragma once

#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {
	using NE::Math::Vec3;
	using NE::Math::Mat4;

	using RenderViewHandle = std::uint32_t;

	struct Camera {
		enum class ProjectionType : uint8_t {
			Perspective,
			Orthographic
		};

		enum class FieldOfViewAxis : uint8_t {
			Vertical,
			Horizontal
		};

		Mat4 viewMtx;
		Mat4 projectionMtx;
		uint64_t luid = 0;

		float fovY{ 60.0f };
		float aspectRatio{ 16.0f / 9.0f };
		float nearPlane{ 0.3f };
		float farPlane{ 1000.0f };

		ProjectionType projectionType{ ProjectionType::Perspective };
		FieldOfViewAxis fovAxis{ FieldOfViewAxis::Vertical };

		bool isMain{ true }; // This camera is the main camera used for rendering to screen
		bool isActive{ true }; // This camera is active and will be rendered, but not necessarily sent to screen

		// Flag tracks the projection matrix needs to be rebuilt, view matrix is tracked via transform component
		bool isDirty{ true }; 

		// The render view handle associated with this camera
		std::vector<RenderViewHandle> renderViewHandles;

		NE_REFLECT_BEGIN(Camera)
			NE_REFLECT_FIELD_HIDDEN(projectionType),
			NE_REFLECT_FIELD_HIDDEN(fovAxis),
			NE_REFLECT_FIELD_NAMED(fovY, "Field of View"),
			NE_REFLECT_FIELD_NAMED(nearPlane, "Near Plane"),
			NE_REFLECT_FIELD_NAMED(farPlane, "Far Plane"),
			NE_REFLECT_FIELD_NAMED(isMain, "Is Main Camera"),
			NE_REFLECT_FIELD_NAMED(isActive, "Is Active Camera"),
			NE_REFLECT_FIELD_HIDDEN(aspectRatio)
		NE_REFLECT_END()

	};
}