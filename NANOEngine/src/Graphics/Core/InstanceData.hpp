#pragma once
#include "../../Math/Mat4.hpp"
#include "../../Math/Vec3.hpp"

namespace NE::Graphics {
	// Note: Changes to this struct must be reflected in GLGeometryBuffer::EnableInstanceLayout
	struct InstanceData {
		Math::Mat4 model;
		Math::Vec3 idRGB; // for object picking
		float padding;
	};
}
