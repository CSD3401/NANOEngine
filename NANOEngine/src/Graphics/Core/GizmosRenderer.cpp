#include "GizmosRenderer.hpp"
#include "Frustum.hpp"
#include "../../Math/Mat4.hpp"
#include "GraphicsManager.hpp"
#include "EditorCamera.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace NE::Graphics {

	Math::Vec3 GizmosRenderer::m_CurrentColor = Math::Vec3(1.0f, 1.0f, 1.0f);
    std::vector<GizmosRenderer::LineData> GizmosRenderer::m_BatchedLines;
    std::vector<GizmosRenderer::TriData> GizmosRenderer::m_BatchedTriangles;
    size_t GizmosRenderer::m_LineIndex = 0;
    size_t GizmosRenderer::m_TriangleIndex = 0;

    void GizmosRenderer::BeginFrame() {
        // reset drawing counters
        m_LineIndex = 0;
        m_TriangleIndex = 0;

        if (m_BatchedLines.size() < INITIAL_LINE_CAPACITY) 
        {
            m_BatchedLines.resize(INITIAL_LINE_CAPACITY);
        }

        if (m_BatchedTriangles.size() < INITIAL_TRI_CAPACITY) 
        {
            m_BatchedTriangles.resize(INITIAL_TRI_CAPACITY);
        }
    }

    void GizmosRenderer::EndFrame() {
        if (m_LineIndex > 0)
        {
            // sort lines by color
            std::sort(m_BatchedLines.begin(), m_BatchedLines.begin() + m_LineIndex,
                [](const LineData& a, const LineData& b) {
                    if (a.color.x != b.color.x) return a.color.x < b.color.x;
                    if (a.color.y != b.color.y) return a.color.y < b.color.y;
                    return a.color.z < b.color.z;
                });

            std::vector<Math::Vec3> vertices;

            size_t start = 0;
            while (start < m_LineIndex) 
            {
                const Math::Vec3& currentColor = m_BatchedLines[start].color;
                size_t end = start + 1;

                while (end < m_LineIndex &&
                    m_BatchedLines[end].color.x == currentColor.x &&
                    m_BatchedLines[end].color.y == currentColor.y &&
                    m_BatchedLines[end].color.z == currentColor.z) 
                {
                    ++end;
                }

                // build vertex array for this color batch
                const size_t batchSize = end - start;
                vertices.clear();
                vertices.reserve(batchSize * 2);

                for (size_t i = start; i < end; ++i) 
                {
                    vertices.push_back(m_BatchedLines[i].from);
                    vertices.push_back(m_BatchedLines[i].to);
                }

                GraphicsManager::AddDebugLinesBatch(vertices, currentColor);
                start = end;
            }
        }

        if (m_TriangleIndex > 0)
        {
            // sort triangles by color
            std::sort(m_BatchedTriangles.begin(), m_BatchedTriangles.begin() + m_TriangleIndex,
                [](const TriData& a, const TriData& b) {
                    if (a.color.x != b.color.x) return a.color.x < b.color.x;
                    if (a.color.y != b.color.y) return a.color.y < b.color.y;
                    return a.color.z < b.color.z;
                });

            std::vector<Math::Vec3> vertices;

            size_t start = 0;
            while (start < m_TriangleIndex) 
            {
                const Math::Vec3& currentColor = m_BatchedTriangles[start].color;
                size_t end = start + 1;

                while (end < m_TriangleIndex &&
                    m_BatchedTriangles[end].color.x == currentColor.x &&
                    m_BatchedTriangles[end].color.y == currentColor.y &&
                    m_BatchedTriangles[end].color.z == currentColor.z) 
                {
                    ++end;
                }

                const size_t batchSize = end - start;
                vertices.clear();
                vertices.reserve(batchSize * 3);

                for (size_t i = start; i < end; ++i)
                {
                    vertices.push_back(m_BatchedTriangles[i].v0);
                    vertices.push_back(m_BatchedTriangles[i].v1);
                    vertices.push_back(m_BatchedTriangles[i].v2);
                }

                GraphicsManager::AddDebugTrianglesBatch(vertices, currentColor);
                start = end;
            }
        }
    }

	void GizmosRenderer::DrawLine(const Math::Vec3& from, const Math::Vec3& to) {
        if (m_LineIndex >= m_BatchedLines.size())
        {
            m_BatchedLines.resize(m_BatchedLines.size() * 2);
        }

        m_BatchedLines[m_LineIndex].from = from;
        m_BatchedLines[m_LineIndex].to = to;
        m_BatchedLines[m_LineIndex].color = m_CurrentColor;
        ++m_LineIndex;
    }

	void GizmosRenderer::DrawTriangle(const Math::Vec3& p0, const Math::Vec3& p1, const Math::Vec3& p2) {
        if (m_TriangleIndex >= m_BatchedTriangles.size()) 
        {
            m_BatchedTriangles.resize(m_BatchedTriangles.size() * 2);
        }

        m_BatchedTriangles[m_TriangleIndex].v0 = p0;
        m_BatchedTriangles[m_TriangleIndex].v1 = p1;
        m_BatchedTriangles[m_TriangleIndex].v2 = p2;
        m_BatchedTriangles[m_TriangleIndex].color = m_CurrentColor;
        ++m_TriangleIndex;
    }

    void GizmosRenderer::DrawLineList(const std::vector<Math::Vec3>& points) {
        const size_t numLines = points.size() / 2;

        if (m_LineIndex + numLines > m_BatchedLines.size()) 
        {
            m_BatchedLines.resize(std::max(m_BatchedLines.size() * 2, m_LineIndex + numLines));
        }

        for (size_t i = 0; i + 1 < points.size(); i += 2) 
        {
            m_BatchedLines[m_LineIndex].from = points[i];
            m_BatchedLines[m_LineIndex].to = points[i + 1];
            m_BatchedLines[m_LineIndex].color = m_CurrentColor;
            ++m_LineIndex;
        }
    }

    void GizmosRenderer::DrawLineStrip(const std::vector<Math::Vec3>& points, bool looped) {
        if (points.size() < 2) return;

        const size_t numSegments = points.size() - 1 + (looped ? 1 : 0);

        if (m_LineIndex + numSegments > m_BatchedLines.size()) 
        {
            m_BatchedLines.resize(std::max(m_BatchedLines.size() * 2, m_LineIndex + numSegments));
        }

        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            m_BatchedLines[m_LineIndex].from = points[i];
            m_BatchedLines[m_LineIndex].to = points[i + 1];
            m_BatchedLines[m_LineIndex].color = m_CurrentColor;
            ++m_LineIndex;
        }

        if (looped) 
        {
            m_BatchedLines[m_LineIndex].from = points[points.size() - 1];
            m_BatchedLines[m_LineIndex].to = points[0];
            m_BatchedLines[m_LineIndex].color = m_CurrentColor;
            ++m_LineIndex;
        }
    }

    void GizmosRenderer::DrawCube(const Math::Vec3& center, const Math::Vec3& size) {
        float radius = size.Length() * 0.5f;
        if (!IsVisible(center, radius)) return;

        Math::Vec3 h = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };

        // 8 vertices of the cube
        Math::Vec3 v[8] = {
          center + Math::Vec3{-h.x, -h.y, -h.z},    // 0
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

        // ensure enough capacity for 12 triangles
        if (m_TriangleIndex + 12 > m_BatchedTriangles.size()) 
        {
            m_BatchedTriangles.resize(std::max(m_BatchedTriangles.size() * 2, m_TriangleIndex + 12));
        }

        for (size_t i = 0; i < 36; i += 3) 
        {
            m_BatchedTriangles[m_TriangleIndex].v0 = v[indices[i]];
            m_BatchedTriangles[m_TriangleIndex].v1 = v[indices[i + 1]];
            m_BatchedTriangles[m_TriangleIndex].v2 = v[indices[i + 2]];
            m_BatchedTriangles[m_TriangleIndex].color = m_CurrentColor;
            ++m_TriangleIndex;
        }
    }

    void GizmosRenderer::DrawSphere(const Math::Vec3& center, float radius, int slices, int stacks) {
        if (!IsVisible(center, radius)) return;

        constexpr float TAU = 2.0f * Math::PI;

        const size_t numTriangles = 2 * slices * stacks;

        if (m_TriangleIndex + numTriangles > m_BatchedTriangles.size()) 
        {
            m_BatchedTriangles.resize(std::max(m_BatchedTriangles.size() * 2, m_TriangleIndex + numTriangles));
        }

        // pre-calculate sin/cos tables - stack allocated
        float* cosTheta = (float*)alloca((slices + 1) * sizeof(float));
        float* sinTheta = (float*)alloca((slices + 1) * sizeof(float));

        for (int i = 0; i <= slices; ++i) 
        {
            float theta = ((float)i / slices) * TAU;
            cosTheta[i] = std::cos(theta);
            sinTheta[i] = std::sin(theta);
        }

        for (int j = 0; j < stacks; ++j) 
        {
            float v0 = (float)j / stacks;
            float v1 = (float)(j + 1) / stacks;
            float phi0 = v0 * Math::PI;
            float phi1 = v1 * Math::PI;
            float sinPhi0 = std::sin(phi0), cosPhi0 = std::cos(phi0);
            float sinPhi1 = std::sin(phi1), cosPhi1 = std::cos(phi1);

            for (int i = 0; i < slices; ++i) 
            {
                float cosTheta0 = cosTheta[i];
                float sinTheta0 = sinTheta[i];
                float cosTheta1 = cosTheta[i + 1];
                float sinTheta1 = sinTheta[i + 1];

                Math::Vec3 p00 = center + Math::Vec3{
                    radius * sinPhi0 * cosTheta0,
                    radius * cosPhi0,
                    radius * sinPhi0 * sinTheta0
                };
                Math::Vec3 p10 = center + Math::Vec3{
                    radius * sinPhi0 * cosTheta1,
                    radius * cosPhi0,
                    radius * sinPhi0 * sinTheta1
                };
                Math::Vec3 p01 = center + Math::Vec3{
                    radius * sinPhi1 * cosTheta0,
                    radius * cosPhi1,
                    radius * sinPhi1 * sinTheta0
                };
                Math::Vec3 p11 = center + Math::Vec3{
                    radius * sinPhi1 * cosTheta1,
                    radius * cosPhi1,
                    radius * sinPhi1 * sinTheta1
                };

                // triangle 1
                m_BatchedTriangles[m_TriangleIndex].v0 = p00;
                m_BatchedTriangles[m_TriangleIndex].v1 = p10;
                m_BatchedTriangles[m_TriangleIndex].v2 = p11;
                m_BatchedTriangles[m_TriangleIndex].color = m_CurrentColor;
                ++m_TriangleIndex;

                // triangle 2
                m_BatchedTriangles[m_TriangleIndex].v0 = p00;
                m_BatchedTriangles[m_TriangleIndex].v1 = p11;
                m_BatchedTriangles[m_TriangleIndex].v2 = p01;
                m_BatchedTriangles[m_TriangleIndex].color = m_CurrentColor;
                ++m_TriangleIndex;
            }
        }
    }

    void GizmosRenderer::DrawWireCube(const Math::Vec3& min, const Math::Vec3& max) {
        Math::Vec3 center = (min + max) * 0.5f;
        float radius = (max - min).Length() * 0.5f;
        if (!IsVisible(center, radius)) return;

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

        if (m_LineIndex + 12 > m_BatchedLines.size()) 
        {
            m_BatchedLines.resize(std::max(m_BatchedLines.size() * 2, m_LineIndex + 12));
        }

        for (int i = 0; i < 24; i += 2) 
        {
            m_BatchedLines[m_LineIndex].from = p[edges[i]];
            m_BatchedLines[m_LineIndex].to = p[edges[i + 1]];
            m_BatchedLines[m_LineIndex].color = m_CurrentColor;
            ++m_LineIndex;
        }
    }

    void GizmosRenderer::DrawWireSphere(const Math::Vec3& center, float radius, int segments) {
        if (!IsVisible(center, radius)) return;

        constexpr float TAU = 2.0f * Math::PI;

        if (m_LineIndex + 3 * segments > m_BatchedLines.size()) 
        {
            m_BatchedLines.resize(std::max(m_BatchedLines.size() * 2, m_LineIndex + 3 * segments));
        }

        // Stack-allocated trig tables
        float* cosValues = (float*)alloca((segments + 1) * sizeof(float));
        float* sinValues = (float*)alloca((segments + 1) * sizeof(float));

        for (int i = 0; i <= segments; ++i) 
        {
            float angle = ((float)i / segments) * TAU;
            cosValues[i] = std::cos(angle);
            sinValues[i] = std::sin(angle);
        }

        auto drawCircle = [&](const Math::Vec3& axis1, const Math::Vec3& axis2)
        {
            Math::Vec3 prev = center + axis1 * radius;

            for (int i = 1; i <= segments; ++i) {
                Math::Vec3 curr = center + (axis1 * cosValues[i] + axis2 * sinValues[i]) * radius;
                m_BatchedLines[m_LineIndex].from = prev;
                m_BatchedLines[m_LineIndex].to = curr;
                m_BatchedLines[m_LineIndex].color = m_CurrentColor;
                ++m_LineIndex;
                prev = curr;
            }
        };

        // 3 orthogonal great circles
        drawCircle({ 1,0,0 }, { 0,1,0 });  // XY plane
        drawCircle({ 1,0,0 }, { 0,0,1 });  // XZ plane
        drawCircle({ 0,1,0 }, { 0,0,1 });  // YZ plane
    }

    void GizmosRenderer::DrawRay(const Math::Vec3& origin, const Math::Vec3& direction, float length) {
        if (!IsVisible(origin, length)) return;

        Math::Vec3 tip = origin + direction * length;

        if (m_LineIndex + 3 > m_BatchedLines.size()) 
        {
            m_BatchedLines.resize(std::max(m_BatchedLines.size() * 2, m_LineIndex + 3));
        }

        // main line
        m_BatchedLines[m_LineIndex].from = origin;
        m_BatchedLines[m_LineIndex].to = tip;
        m_BatchedLines[m_LineIndex].color = m_CurrentColor;
        ++m_LineIndex;

        float arrowSize = std::max(0.02f, length * 0.05f);

        Math::Vec3 perp1, perp2;
        if (std::abs(direction.y) > 0.9f) 
        {
            perp1 = { 1.0f, 0.0f, 0.0f };
            perp2 = { 0.0f, 0.0f, 1.0f };
        }
        else
        {
            perp1 = { direction.z, 0.0f, -direction.x };
            perp2 = { 0.0f, 1.0f, 0.0f };
        }

        Math::Vec3 base = tip - direction * arrowSize;
        Math::Vec3 side1 = base + perp1 * (arrowSize * 0.3f);
        Math::Vec3 side2 = base + perp2 * (arrowSize * 0.3f);

        // arrow head
        m_BatchedLines[m_LineIndex].from = tip;
        m_BatchedLines[m_LineIndex].to = side1;
        m_BatchedLines[m_LineIndex].color = m_CurrentColor;
        ++m_LineIndex;

        m_BatchedLines[m_LineIndex].from = tip;
        m_BatchedLines[m_LineIndex].to = side2;
        m_BatchedLines[m_LineIndex].color = m_CurrentColor;
        ++m_LineIndex;
    }

    void GizmosRenderer::DrawAxes(const Math::Mat4& worldMatrix, float axisLength) {
        const Math::Vec3 O = worldMatrix.GetTranslation();
        if (!IsVisible(O, axisLength)) return;

        // extract position from matrix (last column)
        const Math::Vec3 X = worldMatrix.GetCol3(0);       // world-space X axis direction
        const Math::Vec3 Y = worldMatrix.GetCol3(1);       // world-space Y axis direction
        const Math::Vec3 Z = worldMatrix.GetCol3(2);       // world-space Z axis direction

        if (m_LineIndex + 3 > m_BatchedLines.size())
        {
            m_BatchedLines.resize(std::max(m_BatchedLines.size() * 2, m_LineIndex + 3));
        }

        // X axis - Red
        m_BatchedLines[m_LineIndex].from = O;
        m_BatchedLines[m_LineIndex].to = O + X * axisLength;
        m_BatchedLines[m_LineIndex].color = { 1, 0, 0 };
        ++m_LineIndex;

        // Y axis - Green
        m_BatchedLines[m_LineIndex].from = O;
        m_BatchedLines[m_LineIndex].to = O + Y * axisLength;
        m_BatchedLines[m_LineIndex].color = { 0, 1, 0 };
        ++m_LineIndex;

        // Z axis - Blue
        m_BatchedLines[m_LineIndex].from = O;
        m_BatchedLines[m_LineIndex].to = O + Z * axisLength;
        m_BatchedLines[m_LineIndex].color = { 0, 0, 1 };
        ++m_LineIndex;
    }

    void GizmosRenderer::SetColor(const Math::Vec3& c) { 
        m_CurrentColor = c;
    }

    const Math::Vec3& GizmosRenderer::GetColor() { 
        return m_CurrentColor; 
    }

    void GizmosRenderer::DrawFrustum(const Math::Vec3& center, float fov, float maxRange, float minRange, float aspect) {
        if (!IsVisible(center, maxRange)) return;

        // convert FOV from degrees to radians
        float halfFovRad = (fov * 0.5f) * (Math::PI / 180.0f);

        // compute height & width of near/far planes
        float tanHalfFov = std::tan(halfFovRad);
        float nearHeight = 2.0f * tanHalfFov * minRange;
        float nearWidth = nearHeight * aspect;
        float farHeight = 2.0f * tanHalfFov * maxRange;
        float farWidth = farHeight * aspect;

        // camera faces -Z in local space
        Math::Vec3 forward = { 0.0f, 0.0f, -1.0f };  // -Z direction
        Math::Vec3 up = { 0.0f, 1.0f, 0.0f };        // Y axis
        Math::Vec3 right = { 1.0f, 0.0f, 0.0f };     // X axis

        // centers of near and far planes
        Math::Vec3 nearCenter = center + forward * minRange;
        Math::Vec3 farCenter = center + forward * maxRange;

        // 8 corners
        const Math::Vec3 corners[8] = {
            // near plane
            { nearCenter + (up * (nearHeight * 0.5f)) - (right * (nearWidth * 0.5f)) }, // top-left
            { nearCenter + (up * (nearHeight * 0.5f)) + (right * (nearWidth * 0.5f)) }, // top-right
            { nearCenter - (up * (nearHeight * 0.5f)) - (right * (nearWidth * 0.5f)) }, // bottom-left
            { nearCenter - (up * (nearHeight * 0.5f)) + (right * (nearWidth * 0.5f)) }, // bottom-right

            // far plane
            { farCenter + (up * (farHeight * 0.5f)) - (right * (farWidth * 0.5f)) },    // top-left
            { farCenter + (up * (farHeight * 0.5f)) + (right * (farWidth * 0.5f)) },    // top-right
            { farCenter - (up * (farHeight * 0.5f)) - (right * (farWidth * 0.5f)) },    // bottom-left
            { farCenter - (up * (farHeight * 0.5f)) + (right * (farWidth * 0.5f)) }     // bottom-right
        };

        if (m_LineIndex + 16 > m_BatchedLines.size()) 
        {
            m_BatchedLines.resize(std::max(m_BatchedLines.size() * 2, m_LineIndex + 16));
        }

        // near plane (4 lines)
        m_BatchedLines[m_LineIndex++] = { corners[0], corners[1], m_CurrentColor };
        m_BatchedLines[m_LineIndex++] = { corners[1], corners[3], m_CurrentColor };
        m_BatchedLines[m_LineIndex++] = { corners[3], corners[2], m_CurrentColor };
        m_BatchedLines[m_LineIndex++] = { corners[2], corners[0], m_CurrentColor };

        // far plane (4 lines)
        m_BatchedLines[m_LineIndex++] = { corners[4], corners[5], m_CurrentColor };
        m_BatchedLines[m_LineIndex++] = { corners[5], corners[7], m_CurrentColor };
        m_BatchedLines[m_LineIndex++] = { corners[7], corners[6], m_CurrentColor };
        m_BatchedLines[m_LineIndex++] = { corners[6], corners[4], m_CurrentColor };

        // connecting edges (4 lines)
        m_BatchedLines[m_LineIndex++] = { corners[0], corners[4], m_CurrentColor };
        m_BatchedLines[m_LineIndex++] = { corners[1], corners[5], m_CurrentColor };
        m_BatchedLines[m_LineIndex++] = { corners[2], corners[6], m_CurrentColor };
        m_BatchedLines[m_LineIndex++] = { corners[3], corners[7], m_CurrentColor };
    }

    float GizmosRenderer::CalculateLOD(const Math::Vec3& position, float radius) {
        // get the camera position
        Math::Vec3 cameraPos = GraphicsManager::GetEditorCamera()->GetPosition();

        // compute distance between the camera and the object
        float distance = (position - cameraPos).Length();

        // basic LOD rule: smaller value = closer (more detail)
        // larger value = farther (less detail)
        // here we just divide radius by distance, clamped so it doesn't blow up
        float lod = radius / std::max(distance, 0.001f);

        // optionally clamp between 0 and 1 for consistency
        // LOD ~ 1.0 -> close to camera (high detail)
        // LOD ~ 0.0 -> far away (low detail)
        lod = std::clamp(lod, 0.0f, 1.0f);

        return lod;
    }

    bool GizmosRenderer::IsVisible(const Math::Vec3& center, float radius) {
        auto* cam = GraphicsManager::GetEditorCamera();
        if (!cam) return true; // no camera = draw everything

        // build frustum
        const Math::Mat4& V = cam->GetViewMatrix();
        const Math::Mat4& P = cam->GetProjectionMatrix();
        Mat4 nonConstPCopy = P;
        Frustum frustum = Frustum::ExtractPlanesFromVP(nonConstPCopy * V);

        // test sphere intersection
        return frustum.IntersectsSphere(center, radius);
    }

    void GizmosRenderer::Cleanup() {
        m_BatchedLines.clear();
        m_BatchedLines.shrink_to_fit();

        m_BatchedTriangles.clear();
        m_BatchedTriangles.shrink_to_fit();
    }

    void GizmosRenderer::TestGizmosRenderer() {
        //std::cout << "=== Testing GizmosRenderer ===" << std::endl;

        BeginFrame();

        // Test 1: Basic Line
        //std::cout << "\n[1] Testing DrawLine..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 0.0f, 0.0f }); // Red
        GizmosRenderer::DrawLine({ 1, 3, 0 }, { -1, 3, 0 });
        //std::cout << "Drew red line from (1,3,0) to (-1,3,0)" << std::endl;

        // Test 2: Triangle
        //std::cout << "\n[2] Testing DrawTriangle..." << std::endl;
        GizmosRenderer::SetColor({ 0.0f, 1.0f, 0.0f }); // Green
        GizmosRenderer::DrawTriangle({ 0, 0, 0 }, { 1, 0, 0 }, { 0.5f, 1, 0 });
        //std::cout << "Drew green triangle" << std::endl;

        // Test 3: Line List
        //std::cout << "\n[3] Testing DrawLineList..." << std::endl;
        GizmosRenderer::SetColor({ 0.0f, 0.0f, 1.0f }); // Blue
        std::vector<Math::Vec3> lineList = {
            {0, 0, 0}, {1, 0, 0},  // First line
            {0, 1, 0}, {1, 1, 0}   // Second line
        };
        GizmosRenderer::DrawLineList(lineList);
        //std::cout << "Drew 2 blue lines as line list" << std::endl;

        // Test 4: Line Strip (non-looped)
        //std::cout << "\n[4] Testing DrawLineStrip (non-looped)..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 1.0f, 0.0f }); // Yellow
        std::vector<Math::Vec3> stripOpen = {
            {2, 0, 0}, {3, 0, 0}, {3, 1, 0}, {2, 1, 0}
        };
        GizmosRenderer::DrawLineStrip(stripOpen, false);
        //std::cout << "Drew yellow line strip (3 segments)" << std::endl;

        // Test 5: Line Strip (looped)
        //std::cout << "\n[5] Testing DrawLineStrip (looped)..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 0.0f, 1.0f }); // Magenta
        std::vector<Math::Vec3> stripClosed = {
            {4, 0, 0}, {5, 0, 0}, {5, 1, 0}, {4, 1, 0}
        };
        GizmosRenderer::DrawLineStrip(stripClosed, true);
        //std::cout << "Drew magenta closed loop (4 segments)" << std::endl;

        // Test 6: Solid Cube
        //std::cout << "\n[6] Testing DrawCube..." << std::endl;
        GizmosRenderer::SetColor({ 0.5f, 0.5f, 1.0f }); // Light blue
        GizmosRenderer::DrawCube({ 0, 2, 0 }, { 1, 1, 1 });
        //std::cout << "Drew light blue cube at (0,2,0) with size (1,1,1)" << std::endl;

        // Test 7: Solid Sphere
        //std::cout << "\n[7] Testing DrawSphere..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 0.5f, 0.0f }); // Orange
        GizmosRenderer::DrawSphere({ 3, 2, 0 }, 0.5f, 16, 12);
        //std::cout << "Drew orange sphere at (3,2,0) with radius 0.5" << std::endl;

        // Test 8: Wire Cube
        //std::cout << "\n[8] Testing DrawWireCube..." << std::endl;
        GizmosRenderer::SetColor({ 0.0f, 1.0f, 1.0f }); // Cyan
        GizmosRenderer::DrawWireCube({ -2, 0, 0 }, { -1, 1, 1 });
        //std::cout << "Drew cyan wire cube from (-2,0,0) to (-1,1,1)" << std::endl;

        // Test 9: Wire Sphere (low resolution)
        //std::cout << "\n[9] Testing DrawWireSphere (12 segments)..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 1.0f, 1.0f }); // White
        GizmosRenderer::DrawWireSphere({ -3, 2, 0 }, 0.5f, 12);
        //std::cout << "Drew white wire sphere at (-3,2,0) with 12 segments" << std::endl;

        // Test 10: Wire Sphere (high resolution)
        //std::cout << "\n[10] Testing DrawWireSphere (32 segments)..." << std::endl;
        GizmosRenderer::SetColor({ 0.7f, 0.7f, 0.7f }); // Gray
        GizmosRenderer::DrawWireSphere({ -3, 4, 0 }, 0.5f, 32);
        //std::cout << "Drew gray wire sphere at (-3,4,0) with 32 segments" << std::endl;

        // Test 11: Ray with arrow
        //std::cout << "\n[11] Testing DrawRay..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 0.0f, 0.0f }); // Red
        GizmosRenderer::DrawRay({ 0, -2, 2 }, { 1, 0, 0 }, 2.0f);
        //std::cout << "Drew red ray from (0,0,2) pointing in X direction, length 2" << std::endl;

        // Test 12: Ray pointing up
        //std::cout << "\n[12] Testing DrawRay (vertical)..." << std::endl;
        GizmosRenderer::SetColor({ 0.0f, 1.0f, 0.0f }); // Green
        GizmosRenderer::DrawRay({ 2, 0, 2 }, { 0, 1, 0 }, 1.5f);
        //std::cout << "Drew green ray from (2,0,2) pointing up, length 1.5" << std::endl;

        // Test 13: Coordinate axes
        //std::cout << "\n[13] Testing DrawAxes..." << std::endl;
        Math::Mat4 identityMatrix = {
            1, 0, 0, -5,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        GizmosRenderer::DrawAxes(identityMatrix, 1.0f);
        //std::cout << "Drew RGB axes at (-5,0,0)" << std::endl;

        // Test 14: Transformed axes
        //std::cout << "\n[14] Testing DrawAxes (transformed)..." << std::endl;
        Math::Mat4 transformedMatrix = {
            1, 0, 0, 0,
            0, 1, 0, 5,
            0, 0, 1, 0,
            0, 0, 0, 1  // Position at (0,5,0)
        };
        GizmosRenderer::DrawAxes(transformedMatrix, 0.5f);
        //std::cout << "Drew RGB axes at (0,5,0) with length 0.5" << std::endl;

        // Test 15: Color getter
        //std::cout << "\n[15] Testing GetColor..." << std::endl;
        GizmosRenderer::SetColor({ 0.1f, 0.2f, 0.3f });
        Math::Vec3 currentColor = GizmosRenderer::GetColor();
        //std::cout << "Set color to (0.1, 0.2, 0.3)" << std::endl;
        //std::cout << "GetColor returned: ("
        //    << currentColor.x << ", "
        //    << currentColor.y << ", "
        //    << currentColor.z << ")" << std::endl;

        // Test 16: Edge case - empty line strip
        //std::cout << "\n[16] Testing DrawLineStrip (empty)..." << std::endl;
        std::vector<Math::Vec3> emptyStrip;
        GizmosRenderer::DrawLineStrip(emptyStrip, false);
        //std::cout << "Handled empty line strip gracefully" << std::endl;

        // Test 17: Edge case - single point line strip
       // std::cout << "\n[17] Testing DrawLineStrip (single point)..." << std::endl;
        std::vector<Math::Vec3> singlePoint = { {5, 5, 0} };
        GizmosRenderer::DrawLineStrip(singlePoint, false);
        //std::cout << "Handled single-point line strip gracefully" << std::endl;

        // Test 18: Complex scene
        //std::cout << "\n[18] Testing complex scene..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 1.0f, 1.0f });
        GizmosRenderer::DrawWireCube({ -2, -2, -2 }, { 2, 2, 2 }); // Bounding box

        GizmosRenderer::SetColor({ 1.0f, 0.0f, 0.0f });            // red
        GizmosRenderer::DrawSphere({ -1, 0, 0 }, 0.3f, 8, 6);

        GizmosRenderer::SetColor({ 0.0f, 1.0f, 0.0f });            // green
        GizmosRenderer::DrawSphere({ 0, 0, 0 }, 0.3f, 8, 6);

        GizmosRenderer::SetColor({ 0.0f, 0.0f, 1.0f });            // blue
        GizmosRenderer::DrawSphere({ 1, 0, 0 }, 0.3f, 8, 6);
        //std::cout << "Drew complex scene with bounding box and 3 colored spheres" << std::endl;

        // Test 19: Drawing Frustum
        //std::cout << "\n[19] Testing DrawFrustum..." << std::endl;
        GizmosRenderer::SetColor({ 1.0f, 1.0f, 1.0f });
        GizmosRenderer::DrawFrustum(Math::Vec3(0, 1, 0), 60.f, 10.f, 1.f, 16.f / 9.f);
        //std::cout << "Drew frustum successfully" << std::endl;

        //// Test 20: Calculating LODs
        //std::cout << "\n[20] Testing calculating LODs..." << std::endl;

        //// Get active camera from GraphicsManager
        //Camera* cam = GraphicsManager::GetCamera();
        //if (!cam)
        //{
        //    std::cout << "No active camera found!\n";
        //    return;
        //}

        //// Mock the camera position: Set its position manually
        //cam->SetPosition({ 0.0f, 0.0f, 0.0f });

        //// Example objects at different distances
        //struct Obj { Math::Vec3 pos; float radius; };
        //Obj objects[] = {
        //    { { 0.0f, 0.0f, 2.0f }, 1.0f },   // near
        //    { { 0.0f, 0.0f, 10.0f }, 1.0f },  // medium
        //    { { 0.0f, 0.0f, 50.0f }, 1.0f },  // far
        //    { { 0.0f, 0.0f, 100.0f }, 5.0f }  // very far, large radius
        //};

        //std::cout << "Testing LOD function:\n";
        //for (auto& o : objects)
        //{
        //    float lod = NE::Graphics::GizmosRenderer::CalculateLOD(o.pos, o.radius);
        //    std::cout << "Object at " << o.pos.z << " -> LOD = " << lod << "\n";
        //}

        ////Expected output:
        //Object at 2->LOD = 0.5
        //Object at 10->LOD = 0.1
        //Object at 50->LOD = 0.02
        //Object at 100->LOD = 0.05

        //std::cout << "Calculating LODs completed" << std::endl;

        EndFrame();

        //std::cout << "\n=== All tests completed! ===" << std::endl;
        //std::cout << "Total tests: 18" << std::endl;
        //std::cout << "\nNote: Verify visual output in your graphics window" << std::endl;
    }

#pragma region example implementations for overriding virutal functions
    // example implementation when overriding for scripting
    //// Example: PlayerController.hpp
    //#include "GizmosRenderer.hpp"

    //    class PlayerController : public IGizmosDrawable {
    //    private:
    //        Math::Vec3 m_Position;
    //        float m_ViewRadius = 5.0f;

    //    public:
    //        // Implement the virtual methods HERE
    //        void OnDrawGizmos() override {
    //            // Draw view radius (always visible)
    //            GizmosRenderer::SetColor({ 0.0f, 1.0f, 0.0f });
    //            GizmosRenderer::DrawWireSphere(m_Position, m_ViewRadius, 24);
    //        }

    //        void OnDrawGizmosSelected() override {
    //            // Draw detailed info when selected
    //            GizmosRenderer::SetColor({ 1.0f, 1.0f, 0.0f });
    //            GizmosRenderer::DrawWireCube(
    //                m_Position - Math::Vec3{ 0.5f, 0.5f, 0.5f },
    //                m_Position + Math::Vec3{ 0.5f, 2.0f, 0.5f }
    //            );

    //            Math::Mat4 transform = Math::Mat4::Identity();
    //            transform.SetTranslation(m_Position);
    //            GizmosRenderer::DrawAxes(transform, 1.0f);
    //        }
    //    };

    //// then in editor/scene manager
    //// EditorManager.cpp or SceneRenderer.cpp
    //void EditorManager::RenderGizmos() {
    //    // Loop through all objects in scene
    //    for (auto* obj : m_SceneObjects) {
    //        // Check if object implements IGizmosDrawable
    //        if (auto* drawable = dynamic_cast<IGizmosDrawable*>(obj)) {
    //            drawable->OnDrawGizmos(); // Call always
    //        }
    //    }

    //    // Draw selected object's gizmos
    //    if (m_SelectedObject) {
    //        if (auto* drawable = dynamic_cast<IGizmosDrawable*>(m_SelectedObject)) {
    //            drawable->OnDrawGizmosSelected(); // Call only for selected
    //        }
    //    }
    //}
#pragma endregion
}