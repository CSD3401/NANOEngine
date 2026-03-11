#include "pch.h"
#include "CameraSystem.hpp"
#include "../Components/Camera.hpp"
#include "../Components/Transform.hpp"
#include "../../../src/Math/Vec3.hpp"
#include "../../../src/Math/Mat4.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include <Core/Profiler.hpp>
#include <algorithm>
#include <cmath>

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

	void CameraSystem::OnEntityActive(Entity /*entity*/) {}
	void CameraSystem::OnEntityInactive(Entity /*entity*/) {}

	void CameraSystem::Init() {
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
				camera.renderViewHandles.push_back(Graphics::GraphicsManager::CreateRenderView(
					Graphics::GraphicsManager::GetGameViewWidth(),
					Graphics::GraphicsManager::GetGameViewHeight(),
					false
				));
			}

			if (camera.isActive) {
				// Note: Setting the data automatically enables the camera, so no need to call EnableCamera separately
				for (uint16_t i = 0; i < camera.renderViewHandles.size(); ++i) {
					Graphics::GraphicsManager::SetCameraData(
						camera.renderViewHandles[i],
						camera.projectionMtx,
						camera.viewMtx,
						transform.worldMatrix.GetTranslation(),
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

		// If the Game View resolution/aspect changes, rebuild the main camera projection.
		static float s_lastGameViewAspect = -1.0f;
		const float desiredAspect = Graphics::GraphicsManager::GetGameViewAspect();
		const bool aspectChanged = (std::abs(desiredAspect - s_lastGameViewAspect) > 1e-4f);
		if (aspectChanged) {
			s_lastGameViewAspect = desiredAspect;
		}

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
			else if (aspectChanged && camera.isMain) {
				camera.isDirty = true;
				BuildProjection(camera);
			}

			// transform isDirty seems to be broken.
			/*if (transform.isDirty) {
				BuildView(camera, transform);
			}*/

			BuildView(camera, transform);

			if (camera.renderViewHandles.empty()) {
				// Create a render view for this camera if it doesn't have one
				camera.renderViewHandles.push_back(Graphics::GraphicsManager::CreateRenderView(
					Graphics::GraphicsManager::GetGameViewWidth(),
					Graphics::GraphicsManager::GetGameViewHeight(),
					false
				));
			}

			if (camera.isActive) {
				// Note: Setting the data automatically enables the camera, so no need to call EnableCamera separately
				for (uint16_t i = 0; i < camera.renderViewHandles.size(); ++i) {
					Graphics::GraphicsManager::SetCameraData(
						camera.renderViewHandles[i],
						camera.projectionMtx,
						camera.viewMtx,
						transform.worldMatrix.GetTranslation(),
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
		m_mainCameraEntity.reset();
		const auto& entities = GetEntities();
		for (Entity entity : entities) {
			auto& camera = m_componentManager->GetComponent<Component::Camera>(entity);
			if (!camera.renderViewHandles.empty()) {
				for (auto& handle : camera.renderViewHandles) {
					Graphics::GraphicsManager::DestroyRenderView(handle);
				}
				camera.renderViewHandles.clear();
			}
		}
	}

	void CameraSystem::BuildProjection(Camera& cam) {
		const float aspectRaw = cam.isMain ? Graphics::GraphicsManager::GetGameViewAspect() : cam.aspectRatio;
		const float aspect = std::max(1e-6f, aspectRaw);
		float& nearPlane = cam.nearPlane;
		float& farPlane = cam.farPlane;

		if (cam.projectionType == Camera::ProjectionType::Orthographic) {
			const float size = std::max(0.001f, cam.fovY);

			float halfH = size;
			float halfW = size;

			if (cam.fovAxis == Camera::FieldOfViewAxis::Vertical) {
				halfH = size;
				halfW = size * aspect;
			} else {
				halfW = size;
				halfH = (aspect > 1e-6f) ? (size / aspect) : size;
			}

			const float l = -halfW;
			const float r = +halfW;
			const float b = -halfH;
			const float t = +halfH;

			cam.projectionMtx = Mat4::BuildOrtho(l, r, b, t, nearPlane, farPlane);
		} else {
			const float fovDeg = std::clamp(cam.fovY, 1.0f, 179.0f);
			const float fovRad = fovDeg * Math::DEG_TO_RAD;
			const float invTan = 1.0f / std::tan(fovRad * 0.5f);

			float xScale = 0.0f;
			float yScale = 0.0f;

			if (cam.fovAxis == Camera::FieldOfViewAxis::Vertical) {
				yScale = invTan;
				xScale = invTan / aspect;
			} else {
				xScale = invTan;
				yScale = invTan * aspect;
			}

			cam.projectionMtx.SetToZero();
			cam.projectionMtx.GetElement(0, 0) = xScale;
			cam.projectionMtx.GetElement(1, 1) = yScale;
			cam.projectionMtx.GetElement(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
			cam.projectionMtx.GetElement(2, 3) = (2 * farPlane * nearPlane) / (nearPlane - farPlane);
			cam.projectionMtx.GetElement(3, 2) = -1.0f;
			cam.projectionMtx.GetElement(3, 3) = 0.0f;
		}

		cam.isDirty = false;
	}

	void CameraSystem::BuildView(Camera& cam, Transform& transform) {
		cam.viewMtx = transform.worldMatrix.Inverse();
	}
}
