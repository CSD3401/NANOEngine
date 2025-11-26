#pragma once

#include "../../../src/Math/Vec3.hpp"
#include "../../../src/Math/Mat4.hpp"
#include "../../NANOEngineAPI.hpp"

namespace NE::Graphics {
    using NE::Math::Vec3;
    using NE::Math::Mat4;

    enum class ProjectionMode {
        Perspective,
        Orthographic
    };

    class NANOENGINE_API EditorCamera {
    public:
        EditorCamera();

        void SetPerspective(float fov, float aspectRatio, float nearPlane, float farPlane,
            bool reverseZ = false, bool flipY = false);

        void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);

        void SetPosition(const Vec3& position);
        void LookAt(const Vec3& target, const Vec3& up = { 0.f, 1.f, 0.f });

        const Mat4& GetViewMatrix() const;
        const Mat4& GetProjectionMatrix() const;
        const Vec3& GetPosition() const;
        Vec3 GetForward() const;
    private:
        Vec3 m_position;
        Vec3 m_target;
        Vec3 m_up;

        mutable Mat4 m_viewMatrix;
        Mat4 m_projectionMatrix;

        mutable bool m_viewDirty = true;

        void RecalculateView() const;
    };

}
