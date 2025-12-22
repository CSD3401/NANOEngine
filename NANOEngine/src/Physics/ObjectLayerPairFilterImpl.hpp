#pragma once
#include <array>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include "Core/Layers.hpp"

namespace NE::Physics {
	class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
	public:
		explicit ObjectLayerPairFilterImpl(const std::array<NE::Core::LayerMask, NE::Core::MAX_LAYERS>& matrix);

		bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override;

	private:
		const std::array<NE::Core::LayerMask, NE::Core::MAX_LAYERS>& m_matrix;
	};
}

