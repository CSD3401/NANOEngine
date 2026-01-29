#include "CameraSystem.hpp"
#include "../Components/Camera.hpp"
#include "../Components/Transform.hpp"
#include "../../../src/Math/Vec3.hpp"
#include "../../../src/Math/Mat4.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include <Core/Profiler.hpp>

namespace NE::ECS::Systems {

	CameraSystem::CameraSystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void CameraSystem::OnEntityAdded(Entity)
	{
	}

	void CameraSystem::OnEntityRemoved(Entity)
	{
	}

	void CameraSystem::Init()
	{
		const auto& entities = GetEntities();
		for (Entity entity : entities) {
			auto& camera = m_componentManager->GetComponent<Component::Camera>(entity);
			auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
			if (camera.isMain) {
				if (!m_mainCameraEntity.has_value()) {
					m_mainCameraEntity = entity;
				}
				else {
					// Only one main camera allowed, set this to false
					// Should never happen at init unless user misconfigured multiple cameras as main
					camera.isMain = false;
				}
			}

			BuildProjection(camera);
			BuildView(camera, transform);

			if (camera.renderViewHandles.empty()) {
				// Create a render view for this camera if it doesn't have one
				camera.renderViewHandles.push_back(Graphics::GraphicsManager::CreateRenderView(1920, 1080, false));
			}

			if (camera.isActive) {
				// Note: Setting the data automatically enables the camera, so no need to call EnableCamera separately
				for (uint16_t i = 0; i < camera.renderViewHandles.size(); ++i) {
					Graphics::GraphicsManager::SetCameraData(
						camera.renderViewHandles[i],
						camera.projectionMtx,
						camera.viewMtx,
						transform.localPosition,
						camera.nearPlane,
						camera.farPlane,
						camera.isMain,
						i
					);
				}
			}
			else {
				for (auto& handle : camera.renderViewHandles)
					Graphics::GraphicsManager::DisableCamera(handle);
			}
		}
	}

	void CameraSystem::Update(double)
	{
#ifndef PRODUCTION_BUILD
		NE_PROFILE_FUNCTION();
#endif

		const auto& entities = GetEntities();
		for (Entity entity : entities) {
			auto& camera = m_componentManager->GetComponent<Component::Camera>(entity);
			auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
			if (camera.isMain) {
				if (!m_mainCameraEntity.has_value()) {
					m_mainCameraEntity = entity;
				}
				else {
					// This happens if another camera was set to main during runtime
					// The previous main camera will be demoted but still active
					if (*m_mainCameraEntity != entity) {
						// Check if component exists before modifying (in case the previous main camera was deleted)
						if (m_componentManager->HasComponent<Component::Camera>(*m_mainCameraEntity)) {
							auto& prevMainCam = m_componentManager->GetComponent<Component::Camera>(*m_mainCameraEntity);
							prevMainCam.isMain = false;
						}
						m_mainCameraEntity = entity;
					}
				}
			}

			if (camera.isDirty) {				
				BuildProjection(camera);
			}

			// transform isDirty seems to be broken.
			/*if (transform.isDirty) {
				BuildView(camera, transform);
			}*/

			BuildView(camera, transform);

			if (camera.renderViewHandles.empty()) {
				// Create a render view for this camera if it doesn't have one
				camera.renderViewHandles.push_back(Graphics::GraphicsManager::CreateRenderView(1920, 1080, false));
			}

			if (camera.isActive) {
				// Note: Setting the data automatically enables the camera, so no need to call EnableCamera separately
				for (uint16_t i = 0; i < camera.renderViewHandles.size(); ++i) {
					Graphics::GraphicsManager::SetCameraData(
						camera.renderViewHandles[i],
						camera.projectionMtx,
						camera.viewMtx,
						transform.localPosition,
						camera.nearPlane,
						camera.farPlane,
						camera.isMain,
						i
					);
				}
			}
			else {
				for (auto& handle : camera.renderViewHandles)
					Graphics::GraphicsManager::DisableCamera(handle);
			}
		}
	}

	void CameraSystem::Exit() {
		const auto& entities = GetEntities();
		for (Entity entity : entities) {
			auto& camera = m_componentManager->GetComponent<Component::Camera>(entity);
			if (!camera.renderViewHandles.empty()) {
				for (auto& handle : camera.renderViewHandles) {
					Graphics::GraphicsManager::DestroyRenderView(handle);
				}
			}
		}
	}

	void CameraSystem::BuildProjection(Camera& cam)
	{
		// Build perspective projection matrix
		// Convert FOV from degrees to radians
		float fovYRadians = cam.fovY * Math::DEG_TO_RAD;
		float f = 1.0f / std::tan(fovYRadians * 0.5f);
		float& aspect = cam.aspectRatio;
		float& nearPlane = cam.nearPlane;
		float& farPlane = cam.farPlane;

		cam.projectionMtx.SetToZero();
		cam.projectionMtx.GetElement(0, 0) = f / aspect;
		cam.projectionMtx.GetElement(1, 1) = f;
		cam.projectionMtx.GetElement(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
		cam.projectionMtx.GetElement(2, 3) = (2 * farPlane * nearPlane) / (nearPlane - farPlane);
		cam.projectionMtx.GetElement(3, 2) = -1.0f;
		cam.projectionMtx.GetElement(3, 3) = 0.0f;

		cam.isDirty = false;
		//cam.projectionMtx = Mat4::BuildOrtho(left, right, bottom, top, nearPlane, farPlane);
	}

	void CameraSystem::BuildView(Camera& cam, Transform& transform) {
		cam.viewMtx = transform.worldMatrix.Inverse();
	}
}
