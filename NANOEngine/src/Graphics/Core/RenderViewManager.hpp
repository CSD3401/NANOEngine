#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include "../../Math/Mat4.hpp"
#include "../../Math/Vec3.hpp"

namespace NE::Graphics {

	class IFrameBuffer;
	using RenderViewHandle = std::uint32_t;
	static constexpr RenderViewHandle InvalidRenderView = 0;

	enum class RenderViewFormat {
		Standard,
		HDR,
		LDR
	};

	struct RenderViewCreateDesc {
		uint32_t width = 0;
		uint32_t height = 0;
		bool enablePicking = true;
		RenderViewFormat format = RenderViewFormat::Standard;
	};

	struct RenderView
	{
		std::shared_ptr<IFrameBuffer> framebuffer;

		// Camera data
		Math::Mat4 projection;
		Math::Mat4 view;
		Math::Vec3 position;
		float nearPlane;
		float farPlane;

		bool isMain = false;
		bool isActive = false; // GraphicsManager only renders active views

		// In the event of multiple framebuffers using the same camera, 
		// this determines the order they are added to the graphics manager's render loop
		uint16_t order = 0;
	};

	class RenderViewManager {
	public:
		void Init();
		void Shutdown();
		
		// Creates a framebuffer and returns its handle
		RenderViewHandle Create(const RenderViewCreateDesc& desc);
		RenderViewHandle Create(uint32_t width, uint32_t height, bool enablePicking = true);
		RenderViewHandle CreateHDR(uint32_t width, uint32_t height, bool enablePicking = true);
		RenderViewHandle CreateLDR(uint32_t width, uint32_t height, bool enablePicking = true);

		// Destroys the framebuffer associated with the given handle
		void Destroy(RenderViewHandle handle);
		
		// Destroys all managed framebuffers
		void DestroyAll();

		// Blits to the default framebuffer
		void BlitToScreen(RenderViewHandle handle, int windowWidth, int windowHeight);

		// Sets the camera data for the given render view handle
		void SetCameraData(RenderViewHandle handle, const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, float nearPlane, float farPlane, bool isMain, uint16_t order);

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
		std::vector<RenderViewHandle> GetOrderedActiveViews() const;

		// Returns all render views, for graphics manager access
		const std::unordered_map<RenderViewHandle, RenderView>& GetAllRenderViews() const { return m_Views; }
		std::unordered_map<RenderViewHandle, RenderView>& GetAllRenderViews() { return m_Views; }

	private:
		RenderViewHandle m_NextHandle = 1;
		std::unordered_map<RenderViewHandle, RenderView> m_Views;
	};

}
