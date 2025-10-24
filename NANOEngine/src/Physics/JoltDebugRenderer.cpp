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

    JPH::RVec3 JoltDebugRenderer::Xform(JPH::RMat44Arg M, const JPH::Float3& p) {
        // Vec3(Float3) -> RMat44 * Vec3 -> RVec3
        return M * JPH::Vec3(p.x, p.y, p.z);
    }

	void JoltDebugRenderer::DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color)
    {
        // Convert Jolt vectors/colors to your engine's math/color types
        //NE::Graphics::GraphicsManager::AddDebugLine(
        //    NE::Math::Vec3(float(from.GetX()), float(from.GetY()), float(from.GetZ())),
        //    NE::Math::Vec3(float(to.GetX()), float(to.GetY()), float(to.GetZ())),
        //    NE::Math::Vec3(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f)
        //);

        try
        {

            NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(from), ToVec3(to), ToColor(color));

        }
        catch (...)
        {
            //nth
        }

    }

    void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
    {
        (void)inCastShadow;
        //NE::Graphics::GraphicsManager::AddDebugTriangle(ToVec3(inV1), ToVec3(inV2), ToVec3(inV3), ToColor(inColor));
        try {
            NE::Math::Vec3 color = ToColor(inColor);
            NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(inV1), ToVec3(inV2), color);
            NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(inV2), ToVec3(inV3), color);
            NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(inV3), ToVec3(inV1), color);
        }
        catch (...)
        {

        }
    }

    JPH::DebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
    {
        (void)inTriangles;
        (void)inTriangleCount;
        //if (inTriangles == nullptr || inTriangleCount <= 0) return {};

        //JPH::Ref<BatchPlaceHolder> batch = new BatchPlaceHolder();
        //batch->verts.reserve(size_t(inTriangleCount) * 3);
        //batch->indices.reserve(size_t(inTriangleCount) * 3);
        //batch->edges.reserve(size_t(inTriangleCount) * 3);

        //for (int i = 0; i < inTriangleCount; ++i) 
        //{
        //    const JPH::uint32 base = (JPH::uint32)batch->verts.size();

        //    const JPH::Float3 p0 = inTriangles[i].mV[0].mPosition;
        //    const JPH::Float3 p1 = inTriangles[i].mV[1].mPosition;
        //    const JPH::Float3 p2 = inTriangles[i].mV[2].mPosition;

        //    batch->verts.emplace_back(p0);
        //    batch->verts.emplace_back(p1);
        //    batch->verts.emplace_back(p2);

        //    batch->indices.push_back(base + 0);
        //    batch->indices.push_back(base + 1);
        //    batch->indices.push_back(base + 2);

        //    batch->edges.emplace_back(p0, p1);
        //    batch->edges.emplace_back(p1, p2);
        //    batch->edges.emplace_back(p2, p0);
        //}

        //return JPH::DebugRenderer::Batch(batch); // wrap in Ref<TriangleBatch>

        return Batch(new SimpleBatch());
    }

    JPH::DebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount)
    {
        (void)inVertices;
        (void)inVertexCount;
        (void)inIndices;
        (void)inIndexCount;
        
        //if (inVertices == nullptr || inVertexCount <= 0 || inIndices == nullptr || inIndexCount <= 0) return {};

        //JPH::Ref<BatchPlaceHolder> batch = new BatchPlaceHolder();
        //batch ->verts.reserve(size_t(inVertexCount));
        //batch->indices.reserve(size_t(inIndexCount));
        //batch->edges.reserve(size_t(inIndexCount)); // ~3 edges per tri

        //for (int i = 0; i < inVertexCount; ++i)
        //{
        //    batch->verts.emplace_back(inVertices[i].mPosition);
        //}

        //for (int i = 0; i < inIndexCount; ++i)
        //{
        //    batch->indices.push_back(inIndices[i]);
        //}

        //// create edges from indices (every 3 indices form a triangle)
        //for (int i = 0; i + 2 < inIndexCount; i += 3)
        //{
        //    const JPH::uint32 i0 = inIndices[i + 0];
        //    const JPH::uint32 i1 = inIndices[i + 1];
        //    const JPH::uint32 i2 = inIndices[i + 2];

        //    const JPH::Float3 p0 = inVertices[i0].mPosition;
        //    const JPH::Float3 p1 = inVertices[i1].mPosition;
        //    const JPH::Float3 p2 = inVertices[i2].mPosition;

        //    batch->edges.emplace_back(p0, p1);
        //    batch->edges.emplace_back(p1, p2);
        //    batch->edges.emplace_back(p2, p0);
        //}

        //return JPH::DebugRenderer::Batch(batch);

        return Batch(new SimpleBatch());
    }

    void JoltDebugRenderer::DrawGeometry(const JPH::Mat44& inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode)
    {
        //(void)inWorldSpaceBounds;
        //(void)inLODScaleSq;
        //(void)inCullMode;
        //(void)inCastShadow;
        //(void)inDrawMode;

        // COMPLETELY DISABLE complex geometry drawing
    // Just draw a simple marker at the origin to verify the renderer works

        (void)inWorldSpaceBounds;
        (void)inLODScaleSq;
        (void)inCullMode;
        (void)inCastShadow;
        (void)inDrawMode;
        (void)inGeometry; // Don't use the geometry at all
        (void)inModelColor;

        try {
            // Draw a simple cross at the origin of the object
            JPH::RVec3 center = inModelMatrix * JPH::Vec3::sZero();
            float size = 0.5f;

            // Draw XYZ axes
            DrawLine(center, center + inModelMatrix * JPH::Vec3(size, 0, 0), JPH::Color::sRed);   // X - Red
            DrawLine(center, center + inModelMatrix * JPH::Vec3(0, size, 0), JPH::Color::sGreen); // Y - Green  
            DrawLine(center, center + inModelMatrix * JPH::Vec3(0, 0, size), JPH::Color::sBlue);  // Z - Blue
        }
        catch (...) {
            // Ignore any errors
        }





        //if (!inGeometry || inGeometry->mLODs.empty()) return;

        //const DebugRenderer::LOD& lod = inGeometry->mLODs.front();

        //if (!lod.mTriangleBatch) return;

        //const auto* batch = static_cast<const BatchPlaceHolder*>(lod.mTriangleBatch.GetPtr());
        //if (!batch) return;

        //const NE::Math::Vec3 C = ToColor(inModelColor);

        //// draw based on the draw mode
        //if (inDrawMode == EDrawMode::Solid)
        //{
        //    // draw triangles
        //    for (size_t i = 0; i + 2 < batch->indices.size(); i += 3)
        //    {
        //        const JPH::Float3& p0 = batch->verts[batch->indices[i + 0]];
        //        const JPH::Float3& p1 = batch->verts[batch->indices[i + 1]];
        //        const JPH::Float3& p2 = batch->verts[batch->indices[i + 2]];

        //        const JPH::RVec3 v0 = Xform(inModelMatrix, p0);
        //        const JPH::RVec3 v1 = Xform(inModelMatrix, p1);
        //        const JPH::RVec3 v2 = Xform(inModelMatrix, p2);

        //        NE::Graphics::GraphicsManager::AddDebugTriangle(ToVec3(v0), ToVec3(v1), ToVec3(v2), C);
        //    }
        //}
        //else if (inDrawMode == EDrawMode::Wireframe)
        //{
        //    // draw edges for wireframe
        //    for (size_t i = 0; i + 2 < batch->indices.size(); i += 3)
        //    {
        //        const JPH::Float3& p0 = batch->verts[batch->indices[i + 0]];
        //        const JPH::Float3& p1 = batch->verts[batch->indices[i + 1]];
        //        const JPH::Float3& p2 = batch->verts[batch->indices[i + 2]];

        //        const JPH::RVec3 v0 = Xform(inModelMatrix, p0);
        //        const JPH::RVec3 v1 = Xform(inModelMatrix, p1);
        //        const JPH::RVec3 v2 = Xform(inModelMatrix, p2);

        //        NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(v0), ToVec3(v1), C);
        //        NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(v1), ToVec3(v2), C);
        //        NE::Graphics::GraphicsManager::AddDebugLine(ToVec3(v2), ToVec3(v0), C);
        //    }
        //}
    }

    //void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight)
    //{
    //}
}


