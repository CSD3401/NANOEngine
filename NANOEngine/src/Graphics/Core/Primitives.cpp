#include "Primitives.hpp"
//#include <cmath>
#include "../OpenGL/GLVertexBuffer.hpp"
#include "../OpenGL/GLIndexBuffer.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"
#include <corecrt_math_defines.h>

namespace NANOEngine::Graphics {

    std::shared_ptr<Model> CreateCube(float width, float height, float depth) {
        using namespace OpenGL;
        float hw = width * 0.5f;
        float hh = height * 0.5f;
        float hd = depth * 0.5f;

        Vertex verts[] = {
            // Front
            {{-hw, -hh,  hd}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
            {{ hw, -hh,  hd}, {0.f, 0.f, 1.f}, {1.f, 0.f}},
            {{ hw,  hh,  hd}, {0.f, 0.f, 1.f}, {1.f, 1.f}},
            {{-hw,  hh,  hd}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
            // Back
            {{ hw, -hh, -hd}, {0.f, 0.f,-1.f}, {0.f, 0.f}},
            {{-hw, -hh, -hd}, {0.f, 0.f,-1.f}, {1.f, 0.f}},
            {{-hw,  hh, -hd}, {0.f, 0.f,-1.f}, {1.f, 1.f}},
            {{ hw,  hh, -hd}, {0.f, 0.f,-1.f}, {0.f, 1.f}},
            // Left
            {{-hw, -hh, -hd}, {-1.f, 0.f, 0.f}, {0.f, 0.f}},
            {{-hw, -hh,  hd}, {-1.f, 0.f, 0.f}, {1.f, 0.f}},
            {{-hw,  hh,  hd}, {-1.f, 0.f, 0.f}, {1.f, 1.f}},
            {{-hw,  hh, -hd}, {-1.f, 0.f, 0.f}, {0.f, 1.f}},
            // Right
            {{ hw, -hh,  hd}, {1.f, 0.f, 0.f}, {0.f, 0.f}},
            {{ hw, -hh, -hd}, {1.f, 0.f, 0.f}, {1.f, 0.f}},
            {{ hw,  hh, -hd}, {1.f, 0.f, 0.f}, {1.f, 1.f}},
            {{ hw,  hh,  hd}, {1.f, 0.f, 0.f}, {0.f, 1.f}},
            // Top
            {{-hw,  hh,  hd}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
            {{ hw,  hh,  hd}, {0.f, 1.f, 0.f}, {1.f, 0.f}},
            {{ hw,  hh, -hd}, {0.f, 1.f, 0.f}, {1.f, 1.f}},
            {{-hw,  hh, -hd}, {0.f, 1.f, 0.f}, {0.f, 1.f}},
            // Bottom
            {{-hw, -hh, -hd}, {0.f,-1.f, 0.f}, {0.f, 0.f}},
            {{ hw, -hh, -hd}, {0.f,-1.f, 0.f}, {1.f, 0.f}},
            {{ hw, -hh,  hd}, {0.f,-1.f, 0.f}, {1.f, 1.f}},
            {{-hw, -hh,  hd}, {0.f,-1.f, 0.f}, {0.f, 1.f}}
        };

        uint32_t inds[] = {
            0,1,2, 2,3,0,
            4,5,6, 6,7,4,
            8,9,10, 10,11,8,
            12,13,14, 14,15,12,
            16,17,18, 18,19,16,
            20,21,22, 22,23,20
        };

        auto model = std::make_shared<Model>();
        SubMesh sub;
        sub.vertices.assign(std::begin(verts), std::end(verts));
        sub.indices.assign(std::begin(inds), std::end(inds));
        auto vb = std::make_shared<GLVertexBuffer>(sub.vertices.data(),
            static_cast<uint32_t>(sub.vertices.size() * sizeof(Vertex)),
            sizeof(Vertex));
        auto ib = std::make_shared<GLIndexBuffer>(sub.indices.data(),
            static_cast<uint32_t>(sub.indices.size()));
        sub.buffer = std::make_shared<GLGeometryBuffer>(vb, ib);
        model->meshes.push_back(std::move(sub));
        return model;
    }

    std::shared_ptr<Model> CreatePlane(float width, float depth) {
        using namespace OpenGL;
        float hw = width * 0.5f;
        float hd = depth * 0.5f;

        Vertex verts[] = {
            {{-hw, 0.f, -hd}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
            {{ hw, 0.f, -hd}, {0.f, 1.f, 0.f}, {1.f, 0.f}},
            {{ hw, 0.f,  hd}, {0.f, 1.f, 0.f}, {1.f, 1.f}},
            {{-hw, 0.f,  hd}, {0.f, 1.f, 0.f}, {0.f, 1.f}}
        };

        uint32_t inds[] = { 0,1,2, 2,3,0 };

        auto model = std::make_shared<Model>();
        SubMesh sub;
        sub.vertices.assign(std::begin(verts), std::end(verts));
        sub.indices.assign(std::begin(inds), std::end(inds));
        auto vb = std::make_shared<GLVertexBuffer>(sub.vertices.data(),
            static_cast<uint32_t>(sub.vertices.size() * sizeof(Vertex)),
            sizeof(Vertex));
        auto ib = std::make_shared<GLIndexBuffer>(sub.indices.data(),
            static_cast<uint32_t>(sub.indices.size()));
        sub.buffer = std::make_shared<GLGeometryBuffer>(vb, ib);
        model->meshes.push_back(std::move(sub));
        return model;
    }

    std::shared_ptr<Model> CreateCylinder(float radius, float height, int segments) {
        using namespace OpenGL;
        float hh = height * 0.5f;
        const float step = 2.f * static_cast<float>(M_PI) / segments;

        std::vector<Vertex> verts;
        std::vector<uint32_t> inds;

        // Side vertices
        for (int i = 0; i <= segments; ++i) {
            float a = step * i;
            float ca = std::cos(a);
            float sa = std::sin(a);
            float u = static_cast<float>(i) / segments;
            verts.push_back({ {radius * ca, -hh, radius * sa}, {ca, 0.f, sa}, {u, 0.f} });
            verts.push_back({ {radius * ca,  hh, radius * sa}, {ca, 0.f, sa}, {u, 1.f} });
        }

        for (int i = 0; i < segments; ++i) {
            uint32_t b = i * 2;
            inds.push_back(b);
            inds.push_back(b + 1);
            inds.push_back(b + 2);
            inds.push_back(b + 1);
            inds.push_back(b + 3);
            inds.push_back(b + 2);
        }

        // Top center
        uint32_t topCenter = static_cast<uint32_t>(verts.size());
        verts.push_back({ {0.f, hh, 0.f}, {0.f, 1.f, 0.f}, {0.5f, 0.5f} });
        uint32_t topRingStart = static_cast<uint32_t>(verts.size());
        for (int i = 0; i <= segments; ++i) {
            float a = step * i;
            float ca = std::cos(a);
            float sa = std::sin(a);
            float u = (ca + 1.f) * 0.5f;
            float v = (sa + 1.f) * 0.5f;
            verts.push_back({ {radius * ca, hh, radius * sa}, {0.f, 1.f, 0.f}, {u, v} });
        }
        for (int i = 0; i < segments; ++i) {
            inds.push_back(topCenter);
            inds.push_back(topRingStart + i);
            inds.push_back(topRingStart + i + 1);
        }

        // Bottom center
        uint32_t bottomCenter = static_cast<uint32_t>(verts.size());
        verts.push_back({ {0.f, -hh, 0.f}, {0.f, -1.f, 0.f}, {0.5f, 0.5f} });
        uint32_t bottomRingStart = static_cast<uint32_t>(verts.size());
        for (int i = 0; i <= segments; ++i) {
            float a = step * i;
            float ca = std::cos(a);
            float sa = std::sin(a);
            float u = (ca + 1.f) * 0.5f;
            float v = (sa + 1.f) * 0.5f;
            verts.push_back({ {radius * ca, -hh, radius * sa}, {0.f, -1.f, 0.f}, {u, v} });
        }
        for (int i = 0; i < segments; ++i) {
            inds.push_back(bottomCenter);
            inds.push_back(bottomRingStart + i + 1);
            inds.push_back(bottomRingStart + i);
        }

        auto model = std::make_shared<Model>();
        SubMesh sub;
        sub.vertices = std::move(verts);
        sub.indices = std::move(inds);
        auto vb = std::make_shared<GLVertexBuffer>(sub.vertices.data(),
            static_cast<uint32_t>(sub.vertices.size() * sizeof(Vertex)),
            sizeof(Vertex));
        auto ib = std::make_shared<GLIndexBuffer>(sub.indices.data(),
            static_cast<uint32_t>(sub.indices.size()));
        sub.buffer = std::make_shared<GLGeometryBuffer>(vb, ib);
        model->meshes.push_back(std::move(sub));
        return model;
    }

} // namespace NANOEngine::Graphics
