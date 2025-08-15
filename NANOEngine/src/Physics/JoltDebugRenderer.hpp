#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include "../Graphics/Core/GraphicsManager.hpp"

#pragma warning(push)
#pragma warning(disable: 4100)


namespace NE::Physics {

    class JoltDebugRenderer : public JPH::DebugRenderer {
    public:
        void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override;
        void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override {};
        void DrawGeometry(const JPH::Mat44& modelMatrix, const JPH::AABox& worldSpaceBounds, float lodScale, JPH::ColorArg color, const GeometryRef& geometry, ECullMode cullMode, ECastShadow castShadow, EDrawMode drawMode) override {};
        void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) override {};
        Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override { return Batch(); };
        Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount) override { return Batch(); };
    };
}

#pragma warning(pop)
