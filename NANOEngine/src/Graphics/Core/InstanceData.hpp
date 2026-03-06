#pragma once
#include "../../Math/Mat4.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Vec4.hpp"

namespace NE::Graphics {
	// Note: Changes to this struct must be reflected in GLGeometryBuffer::EnableInstanceLayout
	struct InstanceData 
	{
		Math::Mat4 model;
		Math::Vec3 idRGB; // for object picking
		float padding;
	};

	struct ParticleInstanceData
	{
		NE::Math::Vec3 posLS;  // particle position in emitter-local space
		float size;            // particle size
		NE::Math::Vec4 color;  // particle color
	};
}
