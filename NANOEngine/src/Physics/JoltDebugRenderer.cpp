#include "JoltDebugRenderer.hpp"
#include "../Graphics/Core/GraphicsManager.hpp"

namespace NANOEngine::Physics {
	void JoltDebugRenderer::DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color)
    {
        // Convert Jolt vectors/colors to your engine's math/color types
        NANOEngine::Graphics::GraphicsManager::AddDebugLine(
            NANOEngine::Math::Vec3(float(from.GetX()), float(from.GetY()), float(from.GetZ())),
            NANOEngine::Math::Vec3(float(to.GetX()), float(to.GetY()), float(to.GetZ())),
            NANOEngine::Math::Vec3(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f)
        );
    }
    //void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
    //{
    //}
    //void JoltDebugRenderer::DrawGeometry(const JPH::Mat44& modelMatrix, const JPH::AABox& worldSpaceBounds, float lodScale, JPH::ColorArg color, const GeometryRef& geometry, ECullMode cullMode, ECastShadow castShadow, EDrawMode drawMode)
    //{
    //}
    //void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight)
    //{
    //}
    //Batch JoltDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
    //{
    //    return Batch();
    //}
    //Batch JoltDebugRenderer::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount)
    //{
    //    return Batch();
    //}
}
