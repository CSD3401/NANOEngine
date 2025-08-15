#pragma once

#include "../Interfaces/IRenderContext.hpp"

namespace NE::Graphics::Vulkan {
	class VkContext final : public IRenderContext {
	public:
		bool Init(void* windowHandle) override;
		void Shutdown() override;
		void SwapBuffers() override;
		void ResizeViewport(int width, int height) override;
		void SetWindowTitle(const char* title) override;
	private:
		//VkInstance m_instance;
		//VkSurfaceKHR m_surface;
		//VkSwapchainKHR m_swapchain;
	};
}
