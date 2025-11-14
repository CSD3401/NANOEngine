#include "Primitives.hpp"
//#include <cmath>
#include "../OpenGL/GLVertexBuffer.hpp"
#include "../OpenGL/GLIndexBuffer.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"
#include <corecrt_math_defines.h>
#include <algorithm>

namespace NE::Graphics {

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

        //model->ComputeModelSphereBounds();

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

        //model->ComputeModelSphereBounds();

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

        //model->ComputeModelSphereBounds();

        return model;
    }

    std::shared_ptr<Model> CreateSphere(float radius, int slices, int stacks) {
        using namespace OpenGL;
        // Guard rails
        slices = std::max(3, slices);
        stacks = std::max(2, stacks);

        const float PI = static_cast<float>(M_PI);
        const float TWO_PI = 2.f * PI;

        std::vector<Vertex> verts;
        std::vector<uint32_t> inds;

        // Build rings from south pole (phi = 0) to north pole (phi = PI)
        for (int iy = 0; iy <= stacks; ++iy) {
            float v = static_cast<float>(iy) / stacks;
            float phi = v * PI;                 // 0..PI
            float y = radius * std::cos(phi); // Y up
            float r = radius * std::sin(phi); // ring radius

            for (int ix = 0; ix <= slices; ++ix) {
                float u = static_cast<float>(ix) / slices;
                float theta = u * TWO_PI; // 0..2PI

                float cx = std::cos(theta);
                float sx = std::sin(theta);

                NE::Math::Vec3 pos(r * cx, y, r * sx);
                NE::Math::Vec3 nrm = pos.Normalized();
                NE::Math::Vec2 uv(u, 1.f - v); // flip V to match common convention

                verts.push_back({ pos, nrm, uv });
            }
        }

        // Indices (grid quads -> 2 triangles)
        const uint32_t stride = static_cast<uint32_t>(slices + 1);
        for (int iy = 0; iy < stacks; ++iy) {
            for (int ix = 0; ix < slices; ++ix) {
                uint32_t a = iy * stride + ix;
                uint32_t b = (iy + 1) * stride + ix;

                inds.push_back(a);
                inds.push_back(b);
                inds.push_back(a + 1);

                inds.push_back(a + 1);
                inds.push_back(b);
                inds.push_back(b + 1);
            }
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
        //model->ComputeModelSphereBounds();
        return model;
    }

    // height = tip-to-tip (including both hemispheres)
    std::shared_ptr<Model> CreateCapsule(float radius, float height, int slices, int stacks) {
        using namespace OpenGL;
        slices = std::max(3, slices);
        stacks = std::max(2, stacks); // hemisphere resolution; cylinder uses same vertical density

        const float PI = static_cast<float>(M_PI);
        const float HALF_PI = 0.5f * PI;
        const float TWO_PI = 2.f * PI;

        // Cylinder half-height excluding caps
        float hh = std::max(0.f, height * 0.5f - radius);

        std::vector<Vertex> verts;
        std::vector<uint32_t> inds;
        std::vector<uint32_t> ringStart; ringStart.reserve(2 * stacks + stacks + 3);

        auto pushRing = [&](float y, float ringR, bool sphereNormal, float sphereCenterY) {
            ringStart.push_back(static_cast<uint32_t>(verts.size()));
            for (int ix = 0; ix <= slices; ++ix) {
                float u = static_cast<float>(ix) / slices;
                float theta = u * TWO_PI;
                float cx = std::cos(theta);
                float sx = std::sin(theta);

                NE::Math::Vec3 pos(ringR * cx, y, ringR * sx);
                NE::Math::Vec3 nrm;
                if (sphereNormal) {
                    // Normal from sphere center
                    NE::Math::Vec3 fromC(pos.x, pos.y - sphereCenterY, pos.z);
                    nrm = fromC.Normalized();
                } else {
                    nrm = (NE::Math::Vec3(cx, 0.f, sx)).Normalized();
                }

                // v across the full tip-to-tip height
                float v = (pos.y + (hh + radius)) / (2.f * (hh + radius));
                verts.push_back({ pos, nrm, {u, 1.f - v} });
            }
            };

        // ----- Bottom hemisphere (from pole up to the join at y = -hh)
        // phi: 0 (pole) -> HALF_PI (equator)
        for (int iy = 0; iy <= stacks; ++iy) {
            float t = static_cast<float>(iy) / stacks;   // 0..1
            float phi = t * HALF_PI;
            float y = -hh - radius * std::cos(phi);      // -hh - r .. -hh
            float r = radius * std::sin(phi);
            pushRing(y, r, /*sphereNormal=*/true, /*sphereCenterY=*/-hh);
        }

        // ----- Cylinder body (exclude the two end rings already present at -hh and +hh)
        int cylStacks = stacks; // vertical density
        for (int iy = 1; iy < cylStacks; ++iy) {
            float t = static_cast<float>(iy) / cylStacks; // (0,1)
            float y = -hh + t * (2.f * hh);               // -hh..+hh
            pushRing(y, radius, /*sphereNormal=*/false, /*center=*/0.f);
        }

        // ----- Top hemisphere (from join at y = +hh up to the pole)
        // phi: HALF_PI (equator) -> 0 (pole), but we generate from pole to equator for nicer UV continuity
        for (int iy = 0; iy <= stacks; ++iy) {
            float t = static_cast<float>(iy) / stacks;   // 0..1
            float phi = t * HALF_PI;
            float y = hh + radius * std::cos(phi);      // +hh + r .. +hh
            float r = radius * std::sin(phi);
            // For the top, generating from pole->equator duplicates +hh ring; that's OK because
            // the cylinder omitted endpoints. We keep it for consistent topology.
            pushRing(y, r, /*sphereNormal=*/true, /*sphereCenterY=*/ hh);
        }

        // ----- Indices between consecutive rings
        for (size_t ri = 0; ri + 1 < ringStart.size(); ++ri) {
            uint32_t a0 = ringStart[ri];
            uint32_t b0 = ringStart[ri + 1];
            for (int ix = 0; ix < slices; ++ix) {
                uint32_t a = a0 + ix;
                uint32_t b = b0 + ix;

                inds.push_back(a);
                inds.push_back(b);
                inds.push_back(a + 1);

                inds.push_back(a + 1);
                inds.push_back(b);
                inds.push_back(b + 1);
            }
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
        //model->ComputeModelSphereBounds();
        return model;
    }

} // namespace NANOEngine::Graphics
