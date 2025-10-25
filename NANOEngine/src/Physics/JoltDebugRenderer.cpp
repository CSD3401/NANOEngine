#include "JoltDebugRenderer.hpp"
#include "../Graphics/Core/GraphicsManager.hpp"

namespace NE::Physics {

    NE::Math::Vec3 JoltDebugRenderer::ToVec3(JPH::RVec3Arg v) 
    {
        return { (float)v.GetX(), (float)v.GetY(), (float)v.GetZ() };
    }

    NE::Math::Vec3 JoltDebugRenderer::ToColor(JPH::ColorArg c) 
    {
        return { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f };
    }

	void JoltDebugRenderer::DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color)
    {
        // Convert Jolt vectors/colors to your engine's math/color types
        //NE::Graphics::GraphicsManager::AddDebugLine(
        //    NE::Math::Vec3(float(from.GetX()), float(from.GetY()), float(from.GetZ())),
        //    NE::Math::Vec3(float(to.GetX()), float(to.GetY()), float(to.GetZ())),
        //    NE::Math::Vec3(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f)
        //);
        NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(from), ToVec3(to), ToColor(color));
    }

    void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
    {
        (void)inCastShadow;
        NE::Graphics::GraphicsManager::AddDebugTriangle(ToVec3(inV1), ToVec3(inV2), ToVec3(inV3), ToColor(inColor));
    }

    JPH::DebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
    {
        if (inTriangles == nullptr || inTriangleCount <= 0) return {};

        JPH::Ref<BatchPlaceHolder> batch = new BatchPlaceHolder();
        batch->verts.reserve(size_t(inTriangleCount) * 3);
        batch->indices.reserve(size_t(inTriangleCount) * 3);
        batch->edges.reserve(size_t(inTriangleCount) * 3);

        for (int i = 0; i < inTriangleCount; ++i) 
        {
            const JPH::uint32 base = (JPH::uint32)batch->verts.size();

            const JPH::Float3 p0 = inTriangles[i].mV[0].mPosition;
            const JPH::Float3 p1 = inTriangles[i].mV[1].mPosition;
            const JPH::Float3 p2 = inTriangles[i].mV[2].mPosition;

            batch->verts.emplace_back(p0);
            batch->verts.emplace_back(p1);
            batch->verts.emplace_back(p2);

            batch->indices.push_back(base + 0);
            batch->indices.push_back(base + 1);
            batch->indices.push_back(base + 2);

            batch->edges.emplace_back(p0, p1);
            batch->edges.emplace_back(p1, p2);
            batch->edges.emplace_back(p2, p0);
        }

        return JPH::DebugRenderer::Batch(batch); // wrap in Ref<TriangleBatch>
    }

    JPH::DebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount)
    {
        if (inVertices == nullptr || inVertexCount <= 0 || inIndices == nullptr || inIndexCount <= 0) return {};

        JPH::Ref<BatchPlaceHolder> batch = new BatchPlaceHolder();
        batch ->verts.reserve(size_t(inVertexCount));
        batch->indices.reserve(size_t(inIndexCount));
        batch->edges.reserve(size_t(inIndexCount)); // ~3 edges per tri

        for (int i = 0; i < inVertexCount; ++i)
        {
            batch->verts.emplace_back(inVertices[i].mPosition);
        }

        for (int i = 0; i < inIndexCount; ++i)
        {
            batch->indices.push_back(inIndices[i]);
        }

        // create edges from indices (every 3 indices form a triangle)
        for (int i = 0; i + 2 < inIndexCount; i += 3)
        {
            const JPH::uint32 i0 = inIndices[i + 0];
            const JPH::uint32 i1 = inIndices[i + 1];
            const JPH::uint32 i2 = inIndices[i + 2];

            // bounds check
            // if (i0 >= (JPH::uint32)inVertexCount ||
            //     i1 >= (JPH::uint32)inVertexCount ||
            //     i2 >= (JPH::uint32)inVertexCount)
            //     continue;

            const JPH::Float3 p0 = inVertices[i0].mPosition;
            const JPH::Float3 p1 = inVertices[i1].mPosition;
            const JPH::Float3 p2 = inVertices[i2].mPosition;

            batch->edges.emplace_back(p0, p1);
            batch->edges.emplace_back(p1, p2);
            batch->edges.emplace_back(p2, p0);
        }

        return JPH::DebugRenderer::Batch(batch);
    }

    void JoltDebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
    {
        (void)inModelMatrix;
        (void)inWorldSpaceBounds;
        (void)inLODScaleSq;
        (void)inModelColor;
        (void)inGeometry;
        (void)inCullMode;
        (void)inCastShadow;
        (void)inDrawMode;

        //if (!inGeometry || inGeometry->mLODs.empty()) return;

        //const DebugRenderer::LOD& lod = inGeometry->mLODs.front();
        //if (!lod.mTriangleBatch) return;

        //const auto* batch = static_cast<const BatchPlaceHolder*>(lod.mTriangleBatch.GetPtr());
        //if (!batch) return;

        //const NE::Math::Vec3 color = ToColor(inModelColor);

        //// draw based on the draw mode
        //if (inDrawMode == EDrawMode::Solid)
        //{
        //    // draw triangles
        //    for (size_t i = 0; i + 2 < batch->indices.size(); i += 3)
        //    {
        //        const JPH::uint32 idx0 = batch->indices[i + 0];
        //        const JPH::uint32 idx1 = batch->indices[i + 1];
        //        const JPH::uint32 idx2 = batch->indices[i + 2];

        //        // bounds check
        //        if (idx0 >= batch->verts.size() ||
        //            idx1 >= batch->verts.size() ||
        //            idx2 >= batch->verts.size())
        //            continue;

        //        const JPH::Float3& p0 = batch->verts[idx0];
        //        const JPH::Float3& p1 = batch->verts[idx1];
        //        const JPH::Float3& p2 = batch->verts[idx2];

        //        const JPH::RVec3 v0 = inModelMatrix * JPH::Vec3(p0.x, p0.y, p0.z);
        //        const JPH::RVec3 v1 = inModelMatrix * JPH::Vec3(p1.x, p1.y, p1.z);
        //        const JPH::RVec3 v2 = inModelMatrix * JPH::Vec3(p2.x, p2.y, p2.z);


        //        if (inDrawMode == EDrawMode::Solid)
        //        {
        //            NE::Graphics::GraphicsManager::AddDebugTriangle(ToVec3(v0), ToVec3(v1), ToVec3(v2), color);
        //        }
        //        else if (inDrawMode == EDrawMode::Wireframe)
        //        {
        //            NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(v0), ToVec3(v1), color);
        //            NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(v1), ToVec3(v2), color);
        //            NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(v2), ToVec3(v0), color);
        //        }
        //    }
        //}

        return;
    }

    //void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight)
    //{
    //}
}
