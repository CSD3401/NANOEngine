#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include "Core/Layers.hpp"

namespace NE::Physics {
	class ObjectLayerFilterImpl final : public JPH::ObjectLayerFilter {
	public:
		explicit ObjectLayerFilterImpl(NE::Core::LayerMask mask);

		bool ShouldCollide(JPH::ObjectLayer inLayer) const override;

	private:
		Core::LayerMask mLayerMask;
	};
}