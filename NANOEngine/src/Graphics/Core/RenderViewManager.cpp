#include "RenderViewManager.hpp"
#include "../Interfaces/IFrameBuffer.hpp"
#include "Graphics/OpenGL/GLFrameBuffer.hpp"
#include <algorithm>

// Note: When remove component is added, make sure to call RenderViewManager::Destroy for the associated handle
// when deleting a camera component to avoid memory leaks.

namespace NE::Graphics {

	void RenderViewManager::Init()
	{
	}

	void RenderViewManager::Shutdown() {
		DestroyAll();
	}

	RenderViewHandle RenderViewManager::Create(const RenderViewCreateDesc& desc) {
		RenderViewHandle handle = m_NextHandle++;
		std::shared_ptr<OpenGL::GLFrameBuffer> framebuffer;

		switch (desc.format) {
		case RenderViewFormat::HDR:
			framebuffer = std::make_shared<OpenGL::GLFrameBuffer>();
			framebuffer->CreateAsHDR(desc.width, desc.height, desc.enablePicking, desc.enableMiniGBuffer);
			break;
		case RenderViewFormat::LDR:
			framebuffer = std::make_shared<OpenGL::GLFrameBuffer>(desc.width, desc.height);
			framebuffer->CreateAsLDR(desc.width, desc.height, desc.enablePicking, desc.enableMiniGBuffer);
			break;
		case RenderViewFormat::Standard:
		default:
			framebuffer = std::make_shared<OpenGL::GLFrameBuffer>(desc.width, desc.height);
			break;
		}

		framebuffer->SetPickingWrite(desc.enablePicking);

		RenderView view;
		view.framebuffer = framebuffer;
		m_Views[handle] = std::move(view);

		return handle;
	}

	RenderViewHandle RenderViewManager::Create(uint32_t width, uint32_t height, bool enablePicking) {
		RenderViewCreateDesc desc;
		desc.width = width;
		desc.height = height;
		desc.enablePicking = enablePicking;
		desc.format = RenderViewFormat::Standard;
		return Create(desc);
	}

	RenderViewHandle RenderViewManager::CreateHDR(uint32_t width, uint32_t height, bool enablePicking) {
		RenderViewCreateDesc desc;
		desc.width = width;
		desc.height = height;
		desc.enablePicking = enablePicking;
		desc.format = RenderViewFormat::HDR;
		return Create(desc);
	}

	RenderViewHandle RenderViewManager::CreateLDR(uint32_t width, uint32_t height, bool enablePicking) {
		RenderViewCreateDesc desc;
		desc.width = width;
		desc.height = height;
		desc.enablePicking = enablePicking;
		desc.format = RenderViewFormat::LDR;
		return Create(desc);
	}

	void RenderViewManager::Destroy(RenderViewHandle handle) {
		if (handle == InvalidRenderView)
			return;

		auto it = m_Views.find(handle);
		if (it == m_Views.end())
			return;

		m_Views.erase(it);
	}

	void RenderViewManager::DestroyAll() {
		// This function should ONLY be called during Shutdown
		m_Views.clear();
		m_NextHandle = 1;
	}

	void RenderViewManager::BlitToScreen(RenderViewHandle handle, int windowWidth, int windowHeight) {
		auto framebuffer = GetFramebuffer(handle);
		if (framebuffer) {
			framebuffer->BlitToScreen(windowWidth, windowHeight);
		}
	}

	void RenderViewManager::SetCameraData(RenderViewHandle handle, const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, float nearPlane, float farPlane, bool isMain, uint16_t order) {
		auto it = m_Views.find(handle);
		if (it != m_Views.end()) {
			it->second.projection = projection;
			it->second.view = view;
			it->second.position = position;
			it->second.nearPlane = nearPlane;
			it->second.farPlane = farPlane;
			it->second.isMain = isMain;
			it->second.isActive = true;
			it->second.order = order;
		}
	}

	void RenderViewManager::EnableCamera(RenderViewHandle handle) {
		auto it = m_Views.find(handle);
		if (it != m_Views.end()) {
			it->second.isActive = true;
		}
	}

	void RenderViewManager::DisableCamera(RenderViewHandle handle) {
		auto it = m_Views.find(handle);
		if (it != m_Views.end()) {
			it->second.isActive = false;
		}
	}

	std::shared_ptr<IFrameBuffer> RenderViewManager::GetFramebuffer(RenderViewHandle handle) {
		auto it = m_Views.find(handle);
		if (it != m_Views.end()) {
			return it->second.framebuffer;
		}
		return nullptr;
	}

	void RenderViewManager::Bind(RenderViewHandle handle) {
		auto framebuffer = GetFramebuffer(handle);
		if (framebuffer) {
			framebuffer->Bind();
		}
		else {
			Unbind();
		}
	}

	void RenderViewManager::Unbind() const {
		OpenGL::GLFrameBuffer::Unbind();
	}

	void RenderViewManager::Resize(RenderViewHandle handle, uint32_t width, uint32_t height) {
		auto framebuffer = GetFramebuffer(handle);
		if (framebuffer) {
			framebuffer->Resize(width, height);
		}
	}

	void RenderViewManager::ResizeAll(uint32_t width, uint32_t height) {
		for (auto& [handle, view] : m_Views) {
			view.framebuffer->Resize(width, height);
		}
	}

	std::vector<RenderViewHandle> RenderViewManager::GetOrderedActiveViews() const {
		std::vector<RenderViewHandle> handles;
		handles.reserve(m_Views.size());

		for (const auto& [handle, view] : m_Views) {
			if (view.isActive) {
				handles.push_back(handle);
			}
		}

		std::sort(handles.begin(), handles.end(), [this](RenderViewHandle a, RenderViewHandle b) {
			const auto ita = m_Views.find(a);
			const auto itb = m_Views.find(b);
			if (ita == m_Views.end() || itb == m_Views.end()) {
				return a < b;
			}

			const auto& viewA = ita->second;
			const auto& viewB = itb->second;
			if (viewA.order != viewB.order) {
				return viewA.order < viewB.order;
			}
			return a < b;
		});

		return handles;
	}
}
