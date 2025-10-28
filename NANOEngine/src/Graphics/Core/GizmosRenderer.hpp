#ifndef GIZMO_RENDERER_HPP
#define GIZMO_RENDERER_HPP

#include "GraphicsManager.hpp"

namespace NE::Graphics {

    // Static utility class for drawing (like Unity's Gizmos static class)
    class GizmosRenderer {
    public:
        // basic primitives
        static void DrawLine(const Math::Vec3& from, const Math::Vec3& to);
        static void DrawTriangle(const Math::Vec3& p0, const Math::Vec3& p1, const Math::Vec3& p2);

        // lists
        static void DrawLineList(const std::vector<Math::Vec3>& points);               // pairs: (0,1), (2,3), ...
        static void DrawLineStrip(const std::vector<Math::Vec3>& points, bool looped); // 0-1-2-3...

        // shapes (solid)
        static void DrawCube(const Math::Vec3& center, const Math::Vec3& size);
        static void DrawSphere(const Math::Vec3& center, float radius, int slices = 16, int stacks = 12);

        // shapes (wireframe)
        static void DrawWireCube(const Math::Vec3& min, const Math::Vec3& max);
        static void DrawWireSphere(const Math::Vec3& center, float radius, int segments = 24);

        // utilities 
        static void DrawRay(const Math::Vec3& origin, const Math::Vec3& direction, float length = 1.0f);
        static void DrawAxes(const Math::Mat4& worldMatrix, float axisLength = 1.0f);

        // set global color (like Unity's Gizmos.color)
        static void SetColor(const Math::Vec3& color);
        static const Math::Vec3& GetColor();

        // other utilities
        static void DrawFrustum(const Math::Vec3& center, float fov, float maxRange, float minRange, float aspect);
        static float CalculateLOD(const Math::Vec3& position, float radius);
        static void TestGizmosRenderer();

    private:
        static Math::Vec3 m_CurrentColor;
    };

    // Interface for objects that can draw gizmos (like Unity's MonoBehaviour)
    class IGizmosDrawable {
    public:
        virtual ~IGizmosDrawable() = default;

        // Override these in your game objects/components
        virtual void OnDrawGizmos() {}                  // always visible
        virtual void OnDrawGizmosSelected() {}          // only when selected
    };

} // namespace NE::Graphics
#endif // END GIZMO_RENDERER_HPP