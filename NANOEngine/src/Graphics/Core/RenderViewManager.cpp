#include "RenderViewManager.hpp"
#include "../Interfaces/IFrameBuffer.hpp"
#include "Graphics/OpenGL/GLFrameBuffer.hpp"

// Note: When remove component is added, make sure to call RenderViewManager::Destroy for the associated handle
// when deleting a camera component to avoid memory leaks.

namespace NE::Graphics {

	void RenderViewManager::Init()
	{
	}

	void RenderViewManager::Shutdown()
	{
		DestroyAll();
	}

	RenderViewHandle RenderViewManager::Create(uint32_t width, uint32_t height, bool enablePicking)
	{
		RenderViewHandle handle = m_NextHandle++;

		// Create OpenGL framebuffer
		auto framebuffer = std::make_shared<OpenGL::GLFrameBuffer>(width, height);
		framebuffer->SetPickingWrite(enablePicking);

		// Store the render view
		RenderView view;
		view.framebuffer = framebuffer;
		m_Views[handle] = view;

		return handle;
	}

	void RenderViewManager::Destroy(RenderViewHandle handle)
	{
		if (handle == InvalidRenderView)
			return;

		auto it = m_Views.find(handle);
		if (it == m_Views.end())
			return;

		m_Views.erase(it);
	}

	void RenderViewManager::DestroyAll()
	{
		// This function should ONLY be called during Shutdown
		m_Views.clear();
		m_NextHandle = 1;
	}

	void RenderViewManager::SetCameraData(RenderViewHandle handle, Math::Mat4 projection, Math::Mat4 view, Math::Vec3 position, bool isMain)
	{
		auto it = m_Views.find(handle);
		if (it != m_Views.end()) {
			it->second.projection = projection;
			it->second.view = view;
			it->second.position = position;
			it->second.isMain = isMain;
			it->second.isActive = true;
		}
	}

	void RenderViewManager::EnableCamera(RenderViewHandle handle)
	{
		auto it = m_Views.find(handle);
		if (it != m_Views.end()) {
			it->second.isActive = true;
		}
	}

	void RenderViewManager::DisableCamera(RenderViewHandle handle)
	{
		auto it = m_Views.find(handle);
		if (it != m_Views.end()) {
			it->second.isActive = false;
		}
	}

	std::shared_ptr<IFrameBuffer> RenderViewManager::GetFramebuffer(RenderViewHandle handle)
	{
		auto it = m_Views.find(handle);
		if (it != m_Views.end()) {
			return it->second.framebuffer;
		}
		return nullptr;
	}

	void RenderViewManager::Bind(RenderViewHandle handle)
	{
		auto framebuffer = GetFramebuffer(handle);
		if (framebuffer) {
			framebuffer->Bind();
		}
		else {
			Unbind();
		}
	}

	void RenderViewManager::Unbind() const
	{
		OpenGL::GLFrameBuffer::Unbind();
	}

	void RenderViewManager::Resize(RenderViewHandle handle, uint32_t width, uint32_t height)
	{
		auto framebuffer = GetFramebuffer(handle);
		if (framebuffer) {
			framebuffer->Resize(width, height);
		}
	}

	void RenderViewManager::ResizeAll(uint32_t width, uint32_t height)
	{
		for (auto& [handle, view] : m_Views) {
			view.framebuffer->Resize(width, height);
		}
	}
}