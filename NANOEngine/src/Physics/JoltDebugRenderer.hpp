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
        void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override;
        Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;
        Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount) override;
        void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) override;
        
        void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) override {};
    
    private:
        // helper class to store batch data
        class BatchPlaceHolder : public JPH::RefTargetVirtual {
        public:
            std::vector<JPH::Float3> verts;
            std::vector<JPH::uint32> indices;
            std::vector<std::pair<JPH::RVec3, JPH::RVec3>> edges; // for wireframe edges

            BatchPlaceHolder() = default;
            BatchPlaceHolder(const BatchPlaceHolder&) = default;
            BatchPlaceHolder(BatchPlaceHolder&&) = default;

            void AddRef() override { ++mRefCount; }
            void Release() override { if (--mRefCount == 0) delete this; }

        private:
            std::atomic<JPH::uint32> mRefCount{ 0 };
        };

        // helper functions
        static NE::Math::Vec3 ToVec3(JPH::RVec3Arg v);
        static NE::Math::Vec3 ToColor(JPH::ColorArg c);
    };
}

#pragma warning(pop)
