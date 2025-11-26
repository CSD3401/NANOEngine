#pragma once
#include <vector>

namespace NE::Graphics {

	struct RenderView;
	struct Light;

	class IClusteredLighting {
	public:
		virtual ~IClusteredLighting() = default;
		
		virtual void Initialize() = 0;
		virtual void Shutdown() = 0;

		// Called when window/FBO size or cluster resolution changes
		virtual void OnResize(int width, int height) = 0;

		// Per view per frame
		virtual void BuildForView(const RenderView& view, const std::vector<Light*>& lights) = 0;

		// Called before drawing geometry (so forward shaders can read SSBOs/UBOs)
		virtual void BindForDraw() = 0;
	};
}