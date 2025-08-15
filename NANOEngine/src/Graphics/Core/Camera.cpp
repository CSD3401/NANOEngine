#include "Camera.hpp"
#include <cmath>
#include <iostream>

namespace NE::Graphics {

    Camera::Camera()
        : m_position(0.0f, 0.0f, 5.0f),
        m_target(0.0f, 0.0f, 0.0f),
        m_up(0.0f, 1.0f, 0.0f)
    {
        RecalculateView();
    }

    void Camera::SetPerspective(float fovYRadians, float aspectRatio, float nearPlane, float farPlane,
        bool, bool) 
    {
        float f = 1.0f / std::tan(fovYRadians * 0.5f);
        float zn = nearPlane;
        float zf = farPlane;

        m_projectionMatrix.SetToZero();

        m_projectionMatrix.GetElement(0, 0) = f / aspectRatio;
        m_projectionMatrix.GetElement(1, 1) = f;
        m_projectionMatrix.GetElement(2, 2) = (zf + zn) / (zn - zf);
        m_projectionMatrix.GetElement(2, 3) = (2 * zf * zn) / (zn - zf);
        m_projectionMatrix.GetElement(3, 2) = -1.0f;
        m_projectionMatrix.GetElement(3, 3) = 0.0f;
    }

    void Camera::SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
        m_projectionMatrix = Mat4::BuildOrtho(left, right, bottom, top, nearPlane, farPlane);
    }

    void Camera::SetPosition(const Vec3& position) {
        m_position = position;
        m_viewDirty = true;
    }

    void Camera::LookAt(const Vec3& target, const Vec3& up) {
        m_target = target;
        m_up = up;
        m_viewDirty = true;
    }

    void Camera::RecalculateView() const {
        m_viewMatrix.SetToIdentity();
        m_viewMatrix = Mat4::BuildViewMtx(m_position, m_target, m_up);
        m_viewDirty = false;
    }

    const Mat4& Camera::GetViewMatrix() const {
        if (m_viewDirty)
            RecalculateView();
        return m_viewMatrix;
    }

    const Mat4& Camera::GetProjectionMatrix() const {
        return m_projectionMatrix;
    }

    const Vec3& Camera::GetPosition() const {
        return m_position;
    }

    Vec3 Camera::GetForward() const {
        return (m_target - m_position).Normalized();
    }

}
