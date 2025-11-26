#include "GLClusteredLighting.hpp"

#include "../Core/RenderViewManager.hpp"

namespace NE::Graphics::OpenGL {
	void GLClusteredLighting::Initialize() {
		// Initialize SSBOs, UBOs, and compile compute shader
	}
	void GLClusteredLighting::Shutdown() {
		// Clean up resources
	}
	void GLClusteredLighting::OnResize(int width, int height) {
		// Handle resizing if necessary
	}
	void GLClusteredLighting::BuildForView(const RenderView& view, const std::vector<Light*>& lights) {
		UploadLights(lights, view);
		DispatchCompute();
	}
	void GLClusteredLighting::BindForDraw() {
		// Bind SSBOs for use in the rendering pipeline
	}
	void GLClusteredLighting::UploadLights(const std::vector<Light*>& lights, const RenderView& view) {
		// Upload light data to GPU
	}
	void GLClusteredLighting::DispatchCompute() {
		// Dispatch the compute shader to build clusters
	}
}