#pragma once
#include "../../Math/Mat4.hpp"
#include "../../Math/Vec2.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Vec4.hpp"
#include <cstdint>
#include <limits>

namespace NE::Graphics {
	// Note: Changes to this struct must be reflected in GLGeometryBuffer::EnableInstanceLayout
	struct InstanceData 
	{
		Math::Mat4 model;
		Math::Vec3 idRGB; // for object picking
		float lightmapEnabled = 0.0f;
		Math::Vec2 lightmapUvScale = Math::Vec2{ 1.0f, 1.0f };
		Math::Vec2 lightmapUvOffset = Math::Vec2{ 0.0f, 0.0f };
		std::uint32_t lightmapPageSlot = std::numeric_limits<std::uint32_t>::max();
		std::uint32_t padding = 0;
	};

	struct ParticleInstanceData
	{
		NE::Math::Vec3 posLS;  // particle position in emitter-local space
		float size;            // particle size
		NE::Math::Vec4 color;  // particle color
	};
}
