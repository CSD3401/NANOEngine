#include "CameraSystem.hpp"
#include "../Components/Camera.hpp"
#include "../Components/Transform.hpp"
#include "../../../src/Math/Vec3.hpp"
#include "../../../src/Math/Mat4.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"

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
		bool allInactive = true;
		const auto& entities = GetEntities();
		for (Entity entity : entities) {
			auto& camera = m_componentManager->GetComponent<Component::Camera>(entity);
			if (camera.isMain) m_mainCameraEntity = entity;
			if (camera.isActive == false) continue;
			else allInactive = true;
			auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

			BuildProjection(camera);
			BuildView(camera, transform);
		}

		// Must have at least one active camera, default to main camera
		if (allInactive) {
			if (m_mainCameraEntity.has_value()) {
				auto& mainCam = m_componentManager->GetComponent<Component::Camera>(*m_mainCameraEntity);
				mainCam.isActive = true;
				auto& transform = m_componentManager->GetComponent<Component::Transform>(*m_mainCameraEntity);
				BuildProjection(mainCam);
				BuildView(mainCam, transform);
				Graphics::GraphicsManager::SetActiveCamera(mainCam.projectionMtx, mainCam.viewMtx, transform.localPosition, mainCam.isMain);
			}
			else {
				if (!entities.empty()) {
					auto& firstCam = m_componentManager->GetComponent<Component::Camera>(entities[0]);
					firstCam.isActive = true;
					auto& transform = m_componentManager->GetComponent<Component::Transform>(entities[0]);
					BuildProjection(firstCam);
					BuildView(firstCam, transform);
					Graphics::GraphicsManager::SetActiveCamera(firstCam.projectionMtx, firstCam.viewMtx, transform.localPosition, firstCam.isMain);
				}
			}
		}
	}

	void CameraSystem::Update(double)
	{
		bool allInactive = true;
		const auto& entities = GetEntities();
		for (Entity entity : entities) {
			auto& camera = m_componentManager->GetComponent<Component::Camera>(entity);
			if (camera.isMain) m_mainCameraEntity = entity;
			if (camera.isActive == false) continue;
			else allInactive = true;
			auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

			if (camera.isDirty) {
				BuildProjection(camera);
			}
			if (transform.isDirty) {
				BuildView(camera, transform);
			}

			// For now, support only one active camera at a time
			Graphics::GraphicsManager::SetActiveCamera(camera.projectionMtx, camera.viewMtx, transform.localPosition, camera.isMain);
			break;
		}

		// Must have at least one active camera, default to main camera
		if (allInactive) {
			if (m_mainCameraEntity.has_value()) {
				auto& mainCam = m_componentManager->GetComponent<Component::Camera>(*m_mainCameraEntity);
				mainCam.isActive = true;
				auto& transform = m_componentManager->GetComponent<Component::Transform>(*m_mainCameraEntity);
				BuildProjection(mainCam);
				BuildView(mainCam, transform);
				Graphics::GraphicsManager::SetActiveCamera(mainCam.projectionMtx, mainCam.viewMtx, transform.localPosition, mainCam.isMain);
			}
			else {
				if (!entities.empty()) {
					auto& firstCam = m_componentManager->GetComponent<Component::Camera>(entities[0]);
					firstCam.isActive = true;
					auto& transform = m_componentManager->GetComponent<Component::Transform>(entities[0]);
					BuildProjection(firstCam);
					BuildView(firstCam, transform);
					Graphics::GraphicsManager::SetActiveCamera(firstCam.projectionMtx, firstCam.viewMtx, transform.localPosition, firstCam.isMain);
				}
			}
		}
	}

	void CameraSystem::Exit()
	{
	}

	void CameraSystem::BuildProjection(Camera& cam)
	{
		// Build perspective projection matrix
		float f = 1.0f / std::tan(cam.fovY * 0.5f);
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

	void CameraSystem::BuildView(Camera& cam, Transform& transform)
	{
		const Vec3 eye = transform.worldMatrix.GetTranslation();
		const Vec3 fwd = ForwardFromEuler(transform.localRotationEuler);
		const Vec3 target = eye + fwd;
		const Vec3 up = Vec3{ 0.0f, 1.0f, 0.0f };

		cam.viewMtx = Mat4::BuildViewMtx(eye, target, up);
	}

	inline Vec3 CameraSystem::ForwardFromEuler(const Vec3& euler)
	{
		float pitch = euler.x * (3.14159265f / 180.0f);
		float yaw = euler.y * (3.14159265f / 180.0f);

		Vec3 forward;
		forward.x = std::cos(yaw) * std::cos(pitch);
		forward.y = std::sin(pitch);
		forward.z = std::sin(yaw) * std::cos(pitch);
		return forward.Normalize();
	}
}
