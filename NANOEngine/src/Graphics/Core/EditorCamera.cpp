#include "pch.h"
#include "EditorCamera.hpp"
#include <cmath>
#include <iostream>

namespace NE::Graphics {

    EditorCamera::EditorCamera()
        : m_position(0.0f, 0.0f, 5.0f),
        m_target(0.0f, 0.0f, 0.0f),
        m_up(0.0f, 1.0f, 0.0f)
    {
        RecalculateView();
    }

    void EditorCamera::SetPerspective(float fov, float aspectRatio, float nearPlane, float farPlane,
        bool, bool)
    {
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;

        // Convert FOV from degrees to radians
        float fovRadians = fov * NE::Math::DEG_TO_RAD;
        float f = 1.0f / std::tan(fovRadians * 0.5f);

        m_projectionMatrix.SetToZero();

        m_projectionMatrix.GetElement(0, 0) = f / aspectRatio;
        m_projectionMatrix.GetElement(1, 1) = f;
        m_projectionMatrix.GetElement(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
        m_projectionMatrix.GetElement(2, 3) = (2 * farPlane * nearPlane) / (nearPlane - farPlane);
        m_projectionMatrix.GetElement(3, 2) = -1.0f;
        m_projectionMatrix.GetElement(3, 3) = 0.0f;
    }

    void EditorCamera::SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) 
    {
		m_nearPlane = nearPlane;
		m_farPlane = farPlane;
        m_projectionMatrix = Mat4::BuildOrtho(left, right, bottom, top, nearPlane, farPlane);
    }

    void EditorCamera::SetPosition(const Vec3& position) 
    {
        m_position = position;
        m_viewDirty = true;
    }

    void EditorCamera::LookAt(const Vec3& target, const Vec3& up) 
    {
        m_target = target;
        m_up = up;
        m_viewDirty = true;
    }

    void EditorCamera::RecalculateView() const 
    {
        m_viewMatrix.SetToIdentity();
        m_viewMatrix = Mat4::BuildViewMtx(m_position, m_target, m_up);
        m_viewDirty = false;
    }

    const Mat4& EditorCamera::GetViewMatrix() const 
    {
        if (m_viewDirty)
            RecalculateView();
        return m_viewMatrix;
    }

    const Mat4& EditorCamera::GetProjectionMatrix() const 
    {
        return m_projectionMatrix;
    }

    const Vec3& EditorCamera::GetPosition() const 
    {
        return m_position;
    }

    const float& EditorCamera::GetNearPlane() const 
    {
		return m_nearPlane;
	}

    const float& EditorCamera::GetFarPlane() const 
    {
		return m_farPlane;
	}

    Vec3 EditorCamera::GetForward() const 
    {
        return (m_target - m_position).Normalized();
    }
}
