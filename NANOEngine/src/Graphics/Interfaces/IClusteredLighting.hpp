#pragma once
#include <vector>

// Forward declarations
namespace NE {
	namespace ECS {
		namespace Component {
			struct Light;
		}
	}
	namespace Graphics {
		struct RenderView;
	}
}

namespace NE::Graphics {
	class IClusteredLighting {
	public:
		virtual ~IClusteredLighting() = default;

		// Per view per frame
		virtual void BuildForView(const Graphics::RenderView& view, const std::vector<ECS::Component::Light*>& lights) = 0;

		// Called before drawing geometry (so forward shaders can read SSBOs/UBOs)
		virtual void BindForDraw() = 0;
	};
}