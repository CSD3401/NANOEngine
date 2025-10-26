#include "GizmosRenderer.hpp"
#include "../../Math/Mat4.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace NE::Graphics {

	Math::Vec3 GizmosRenderer::m_CurrentColor = Math::Vec3(1.0f, 1.0f, 1.0f);

	void GizmosRenderer::DrawLine(const Math::Vec3& from, const Math::Vec3& to) {
		GraphicsManager::AddDebugLine(from, to, m_CurrentColor);
	}

	void GizmosRenderer::DrawTriangle(const Math::Vec3& p0, const Math::Vec3& p1, const Math::Vec3& p2) {
		GraphicsManager::AddDebugTriangle(p0, p1, p2, m_CurrentColor);
	}

    void GizmosRenderer::DrawLineList(const std::vector<Math::Vec3>& points) {
        for (size_t i = 0; i + 1 < points.size(); i += 2)
        {
            DrawLine(points[i], points[i + 1]);
        }
    }

    void GizmosRenderer::DrawLineStrip(const std::vector<Math::Vec3>& points, bool looped) {
        if (points.size() < 2) return;

        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            DrawLine(points[i], points[i + 1]);
        }

        // connects the end and start points forming a continous path
        if (looped)
        {
            DrawLine(points.back(), points.front());
        }
    }

    void GizmosRenderer::DrawCube(const Math::Vec3& center, const Math::Vec3& size) {
        Math::Vec3 h = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };

        // 8 vertices of the cube
        Math::Vec3 v[8] = {
          center + Math::Vec3{-h.x, -h.y, -h.z},  // 0
            center + Math::Vec3{ h.x, -h.y, -h.z},  // 1
            center + Math::Vec3{-h.x,  h.y, -h.z},  // 2
            center + Math::Vec3{ h.x,  h.y, -h.z},  // 3
            center + Math::Vec3{-h.x, -h.y,  h.z},  // 4
            center + Math::Vec3{ h.x, -h.y,  h.z},  // 5
            center + Math::Vec3{-h.x,  h.y,  h.z},  // 6
            center + Math::Vec3{ h.x,  h.y,  h.z},  // 7
        };

        // 6 faces, 2 triangles each (counter-clockwise winding)
        const int indices[36] = {
            0,1,3, 0,3,2,  // -Z face
            4,6,7, 4,7,5,  // +Z face
            0,4,5, 0,5,1,  // -Y face
            2,3,7, 2,7,6,  // +Y face
            0,2,6, 0,6,4,  // -X face
            1,5,7, 1,7,3   // +X face
        };

        for (int i = 0; i < 36; i += 3) {
            DrawTriangle(v[indices[i]], v[indices[i + 1]], v[indices[i + 2]]);
        }
    }

    void GizmosRenderer::DrawSphere(const Math::Vec3& center, float radius, int slices, int stacks) {
        constexpr float TAU = 2.0f * Math::PI;

        // stacks: “rows” from top to bottom
        for (int j = 0; j < stacks; ++j) 
        {
            float v0 = (float)j / stacks;
            float v1 = (float)(j + 1) / stacks;
            float phi0 = v0 * Math::PI;
            float phi1 = v1 * Math::PI;
            float sinPhi0 = std::sin(phi0), cosPhi0 = std::cos(phi0);
            float sinPhi1 = std::sin(phi1), cosPhi1 = std::cos(phi1);

            // slices: “columns” around the sphere
            for (int i = 0; i < slices; ++i) 
            {
                float u0 = (float)i / slices;
                float u1 = (float)(i + 1) / slices;
                float theta0 = u0 * TAU;
                float theta1 = u1 * TAU;
                float cosTheta0 = std::cos(theta0), sinTheta0 = std::sin(theta0);
                float cosTheta1 = std::cos(theta1), sinTheta1 = std::sin(theta1);

                // 4 points of the little quad on the sphere surface
                Math::Vec3 p00 = center + Math::Vec3{ radius * sinPhi0 * cosTheta0, radius * cosPhi0, radius * sinPhi0 * sinTheta0 };
                Math::Vec3 p10 = center + Math::Vec3{ radius * sinPhi0 * cosTheta1, radius * cosPhi0, radius * sinPhi0 * sinTheta1 };
                Math::Vec3 p01 = center + Math::Vec3{ radius * sinPhi1 * cosTheta0, radius * cosPhi1, radius * sinPhi1 * sinTheta0 };
                Math::Vec3 p11 = center + Math::Vec3{ radius * sinPhi1 * cosTheta1, radius * cosPhi1, radius * sinPhi1 * sinTheta1 };

                // make 2 triangles from that quad
                DrawTriangle(p00, p10, p11);
                DrawTriangle(p00, p11, p01);
            }
        }
    }

    void GizmosRenderer::DrawWireCube(const Math::Vec3& min, const Math::Vec3& max) {
        // 8 vertices of the cube
        const Math::Vec3 p[8] = {
            {min.x, min.y, min.z}, {max.x, min.y, min.z},
            {min.x, max.y, min.z}, {max.x, max.y, min.z},
            {min.x, min.y, max.z}, {max.x, min.y, max.z},
            {min.x, max.y, max.z}, {max.x, max.y, max.z}
        };

        // 12 edges (4 bottom + 4 top + 4 vertical)
        const int edges[24] = {
            0,1, 1,3, 3,2, 2,0,  // bottom
            4,5, 5,7, 7,6, 6,4,  // top
            0,4, 1,5, 2,6, 3,7   // pillars
        };

        for (int i = 0; i < 24; i += 2) {
            DrawLine(p[edges[i]], p[edges[i + 1]]);
        }
    }

    void GizmosRenderer::DrawWireSphere(const Math::Vec3& center, float radius, int segments) {
        constexpr float TAU = 2.0f * Math::PI;

        auto drawCircle = [&](const Math::Vec3& axis1, const Math::Vec3& axis2) {
            Math::Vec3 prev = center + axis1 * radius;

            for (int i = 1; i <= segments; ++i) {
                float angle = (float)i / segments * TAU;
                float c = std::cos(angle);
                float s = std::sin(angle);
                Math::Vec3 curr = center + (axis1 * c + axis2 * s) * radius;
                DrawLine(prev, curr);
                prev = curr;
            }
            };

        // 3 orthogonal great circles
        drawCircle({ 1,0,0 }, { 0,1,0 });  // XY plane
        drawCircle({ 1,0,0 }, { 0,0,1 });  // XZ plane
        drawCircle({ 0,1,0 }, { 0,0,1 });  // YZ plane
    }

    void GizmosRenderer::DrawRay(const Math::Vec3& origin, const Math::Vec3& direction, float length) {
        Math::Vec3 tip = origin + direction * length;
        DrawLine(origin, tip);

        // arrow heead
        float arrowSize = std::max(0.02f, length * 0.05f);

        // create perpendicular vectors for arrow head
        Math::Vec3 perp1, perp2;

        if (std::abs(direction.y) > 0.9f) 
        {
            // Nearly vertical (up/down)
            perp1 = { 1.0f, 0.0f, 0.0f };  // X axis
            perp2 = { 0.0f, 0.0f, 1.0f };  // Z axis
        }
        else 
        {
            perp1 = { direction.z, 0.0f, -direction.x };  // Original perpendicular
            perp2 = { 0.0f, 1.0f, 0.0f };                 // Y axis
        }

        Math::Vec3 base = tip - direction * arrowSize;
        Math::Vec3 side1 = base + perp1 * (arrowSize * 0.3f);
        Math::Vec3 side2 = base + perp2 * (arrowSize * 0.3f);

        // two short lines to make a little arrow head
        DrawLine(tip, side1);
        DrawLine(tip, side2);
    }

    void GizmosRenderer::DrawAxes(const Math::Mat4& worldMatrix, float axisLength) {
        // extract position from matrix (last column)
        const Math::Vec3 O = worldMatrix.GetTranslation(); 
        const Math::Vec3 X = worldMatrix.GetCol3(0);       // world-space X axis direction
        const Math::Vec3 Y = worldMatrix.GetCol3(1);       // world-space Y axis direction
        const Math::Vec3 Z = worldMatrix.GetCol3(2);       // world-space Z axis direction

        NE::Graphics::GraphicsManager::AddDebugLine(O, O + X * axisLength, { 1,0,0 }); // red X
        NE::Graphics::GraphicsManager::AddDebugLine(O, O + Y * axisLength, { 0,1,0 }); // green Y
        NE::Graphics::GraphicsManager::AddDebugLine(O, O + Z * axisLength, { 0,0,1 }); // blue Z
    }

    void GizmosRenderer::SetColor(const Math::Vec3& c) { m_CurrentColor = c; }
    const Math::Vec3& GizmosRenderer::GetColor() { return m_CurrentColor; }

    void GizmosRenderer::TestGizmosRenderer() {
        std::cout << "=== Testing GizmosRenderer ===" << std::endl;

        // Test 1: Basic Line
        std::cout << "\n[1] Testing DrawLine..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 0.0f, 0.0f }); // Red
        GizmosRenderer::DrawLine({ 1, 3, 0 }, { -1, 3, 0 });
        std::cout << "Drew red line from (1,3,0) to (-1,3,0)" << std::endl;

        // Test 2: Triangle
        std::cout << "\n[2] Testing DrawTriangle..." << std::endl;
        GizmosRenderer::SetColor({ 0.0f, 1.0f, 0.0f }); // Green
        GizmosRenderer::DrawTriangle({ 0, 0, 0 }, { 1, 0, 0 }, { 0.5f, 1, 0 });
        std::cout << "Drew green triangle" << std::endl;

        // Test 3: Line List
        std::cout << "\n[3] Testing DrawLineList..." << std::endl;
        GizmosRenderer::SetColor({ 0.0f, 0.0f, 1.0f }); // Blue
        std::vector<Math::Vec3> lineList = {
            {0, 0, 0}, {1, 0, 0},  // First line
            {0, 1, 0}, {1, 1, 0}   // Second line
        };
        GizmosRenderer::DrawLineList(lineList);
        std::cout << "Drew 2 blue lines as line list" << std::endl;

        // Test 4: Line Strip (non-looped)
        std::cout << "\n[4] Testing DrawLineStrip (non-looped)..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 1.0f, 0.0f }); // Yellow
        std::vector<Math::Vec3> stripOpen = {
            {2, 0, 0}, {3, 0, 0}, {3, 1, 0}, {2, 1, 0}
        };
        GizmosRenderer::DrawLineStrip(stripOpen, false);
        std::cout << "Drew yellow line strip (3 segments)" << std::endl;

        // Test 5: Line Strip (looped)
        std::cout << "\n[5] Testing DrawLineStrip (looped)..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 0.0f, 1.0f }); // Magenta
        std::vector<Math::Vec3> stripClosed = {
            {4, 0, 0}, {5, 0, 0}, {5, 1, 0}, {4, 1, 0}
        };
        GizmosRenderer::DrawLineStrip(stripClosed, true);
        std::cout << "Drew magenta closed loop (4 segments)" << std::endl;

        // Test 6: Solid Cube
        std::cout << "\n[6] Testing DrawCube..." << std::endl;
        GizmosRenderer::SetColor({ 0.5f, 0.5f, 1.0f }); // Light blue
        GizmosRenderer::DrawCube({ 0, 2, 0 }, { 1, 1, 1 });
        std::cout << "Drew light blue cube at (0,2,0) with size (1,1,1)" << std::endl;

        // Test 7: Solid Sphere
        std::cout << "\n[7] Testing DrawSphere..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 0.5f, 0.0f }); // Orange
        GizmosRenderer::DrawSphere({ 3, 2, 0 }, 0.5f, 16, 12);
        std::cout << "Drew orange sphere at (3,2,0) with radius 0.5" << std::endl;

        // Test 8: Wire Cube
        std::cout << "\n[8] Testing DrawWireCube..." << std::endl;
        GizmosRenderer::SetColor({ 0.0f, 1.0f, 1.0f }); // Cyan
        GizmosRenderer::DrawWireCube({ -2, 0, 0 }, { -1, 1, 1 });
        std::cout << "Drew cyan wire cube from (-2,0,0) to (-1,1,1)" << std::endl;

        // Test 9: Wire Sphere (low resolution)
        std::cout << "\n[9] Testing DrawWireSphere (12 segments)..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 1.0f, 1.0f }); // White
        GizmosRenderer::DrawWireSphere({ -3, 2, 0 }, 0.5f, 12);
        std::cout << "Drew white wire sphere at (-3,2,0) with 12 segments" << std::endl;

        // Test 10: Wire Sphere (high resolution)
        std::cout << "\n[10] Testing DrawWireSphere (32 segments)..." << std::endl;
        GizmosRenderer::SetColor({ 0.7f, 0.7f, 0.7f }); // Gray
        GizmosRenderer::DrawWireSphere({ -3, 4, 0 }, 0.5f, 32);
        std::cout << "Drew gray wire sphere at (-3,4,0) with 32 segments" << std::endl;

        // Test 11: Ray with arrow
        std::cout << "\n[11] Testing DrawRay..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 0.0f, 0.0f }); // Red
        GizmosRenderer::DrawRay({ 0, -2, 2 }, { 1, 0, 0 }, 2.0f);
        std::cout << "Drew red ray from (0,0,2) pointing in X direction, length 2" << std::endl;

        // Test 12: Ray pointing up
        std::cout << "\n[12] Testing DrawRay (vertical)..." << std::endl;
        GizmosRenderer::SetColor({ 0.0f, 1.0f, 0.0f }); // Green
        GizmosRenderer::DrawRay({ 2, 0, 2 }, { 0, 1, 0 }, 1.5f);
        std::cout << "Drew green ray from (2,0,2) pointing up, length 1.5" << std::endl;

        // Test 13: Coordinate axes
        std::cout << "\n[13] Testing DrawAxes..." << std::endl;
        Math::Mat4 identityMatrix = {
            1, 0, 0, -5,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        GizmosRenderer::DrawAxes(identityMatrix, 1.0f);
        std::cout << "Drew RGB axes at (-5,0,0)" << std::endl;

        // Test 14: Transformed axes
        std::cout << "\n[14] Testing DrawAxes (transformed)..." << std::endl;
        Math::Mat4 transformedMatrix = {
            1, 0, 0, 0,
            0, 1, 0, 5,
            0, 0, 1, 0,
            0, 0, 0, 1  // Position at (0,5,0)
        };
        GizmosRenderer::DrawAxes(transformedMatrix, 0.5f);
        std::cout << "Drew RGB axes at (0,5,0) with length 0.5" << std::endl;

        // Test 15: Color getter
        std::cout << "\n[15] Testing GetColor..." << std::endl;
        GizmosRenderer::SetColor({ 0.1f, 0.2f, 0.3f });
        Math::Vec3 currentColor = GizmosRenderer::GetColor();
        std::cout << "Set color to (0.1, 0.2, 0.3)" << std::endl;
        std::cout << "GetColor returned: ("
            << currentColor.x << ", "
            << currentColor.y << ", "
            << currentColor.z << ")" << std::endl;

        // Test 16: Edge case - empty line strip
        std::cout << "\n[16] Testing DrawLineStrip (empty)..." << std::endl;
        std::vector<Math::Vec3> emptyStrip;
        GizmosRenderer::DrawLineStrip(emptyStrip, false);
        std::cout << "Handled empty line strip gracefully" << std::endl;

        // Test 17: Edge case - single point line strip
        std::cout << "\n[17] Testing DrawLineStrip (single point)..." << std::endl;
        std::vector<Math::Vec3> singlePoint = { {5, 5, 0} };
        GizmosRenderer::DrawLineStrip(singlePoint, false);
        std::cout << "Handled single-point line strip gracefully" << std::endl;

        // Test 18: Complex scene
        std::cout << "\n[18] Testing complex scene..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 1.0f, 1.0f });
        GizmosRenderer::DrawWireCube({ -2, -2, -2 }, { 2, 2, 2 }); // Bounding box

        GizmosRenderer::SetColor({ 1.0f, 0.0f, 0.0f });            // red
        GizmosRenderer::DrawSphere({ -1, 0, 0 }, 0.3f, 8, 6);

        GizmosRenderer::SetColor({ 0.0f, 1.0f, 0.0f });            // green
        GizmosRenderer::DrawSphere({ 0, 0, 0 }, 0.3f, 8, 6);

        GizmosRenderer::SetColor({ 0.0f, 0.0f, 1.0f });            // blue
        GizmosRenderer::DrawSphere({ 1, 0, 0 }, 0.3f, 8, 6);
        std::cout << "Drew complex scene with bounding box and 3 colored spheres" << std::endl;

        std::cout << "\n=== All tests completed! ===" << std::endl;
        std::cout << "Total tests: 18" << std::endl;
        std::cout << "\nNote: Verify visual output in your graphics window" << std::endl;
    }

    // editor hooks (optional)
    //float CalculateLOD(const Math::Vec3& position, float radius);
    //void GizmosRenderer::OnDrawGizmos();         // always visible
    //void GizmosRenderer::OnDrawGizmosSelected(); // only for selected entities
}