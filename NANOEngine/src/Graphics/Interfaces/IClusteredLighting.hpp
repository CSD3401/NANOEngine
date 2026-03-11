#pragma once
#include <vector>

#include "../Core/LightShadowRuntime.hpp"

// Forward declarations
namespace NE {
	namespace Graphics {
		struct RenderView;
		class ShadowRenderer;
	}
}

namespace NE::Graphics {
	class IClusteredLighting {
	public:
		virtual ~IClusteredLighting() = default;

		// Per view per frame
		virtual void BuildForView(const Graphics::RenderView& view, const ShadowRenderer& shadowRenderer, const std::vector<RenderLightRef>& lights) = 0;

		// Called before drawing geometry (so forward shaders can read SSBOs/UBOs)
		virtual void BindForDraw() = 0;
	};
}
