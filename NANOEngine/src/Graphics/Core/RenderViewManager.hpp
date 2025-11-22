#pragma once
#include <unordered_map>
#include <memory>
#include "../../Math/Mat4.hpp"
#include "../../Math/Vec3.hpp"

namespace NE::Graphics {

	class IFrameBuffer;
	using RenderViewHandle = std::uint32_t;
	static constexpr RenderViewHandle InvalidRenderView = 0;

	struct RenderView
	{
		std::shared_ptr<IFrameBuffer> framebuffer;

		// Camera data
		Math::Mat4 projection;
		Math::Mat4 view;
		Math::Vec3 position;
		bool isMain = false;
		bool isActive = false; // GraphicsManager only renders active views
	};

	class RenderViewManager {
	public:
		void Init();
		void Shutdown();
		
		// Creates a framebuffer and returns its handle
		RenderViewHandle Create(uint32_t width, uint32_t height, bool enablePicking = true);

		// Destroys the framebuffer associated with the given handle
		void Destroy(RenderViewHandle handle);
		
		// Destroys all managed framebuffers
		void DestroyAll();

		// Sets the camera data for the given render view handle
		void SetCameraData(RenderViewHandle handle, Math::Mat4 projection, Math::Mat4 view, Math::Vec3 position, bool isMain);

		// Enable/Disable camera for the given render view handle
		void EnableCamera(RenderViewHandle handle);
		void DisableCamera(RenderViewHandle handle);

		// Getters
		std::shared_ptr<IFrameBuffer> GetFramebuffer(RenderViewHandle handle);

		// Binds and unbinds the framebuffer associated with the given handle
		void Bind(RenderViewHandle handle);
		void Unbind() const;

		// Resizes the framebuffer associated with the given handle
		void Resize(RenderViewHandle  handle, uint32_t width, uint32_t height);
		void ResizeAll(uint32_t width, uint32_t height);

		// Returns all render views, for graphics manager access
		const std::unordered_map<RenderViewHandle, RenderView>& GetAllRenderViews() const { return m_Views; }

	private:
		RenderViewHandle m_NextHandle = 1;
		std::unordered_map<RenderViewHandle, RenderView> m_Views;
	};

}