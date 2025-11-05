#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "Vertex.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "ResourceManagement/IResource.hpp"
#include "ResourceManagement/BinaryView.hpp"

namespace NE::Graphics {

    struct SubMesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::shared_ptr<IGeometryBuffer> buffer;
    };

    class Model : public Resource::IResource {
    public:
        std::vector<SubMesh> meshes;

        //NE::Math::Vec3 sphereCenterLS{ 0,0,0 };
        //float sphereRadiusLS = 0.0f;
        //bool hasSphereBoundsLS = false;

        bool Preload(Resource::BinaryView blob) override;
        void Finalize() override;

        //void ComputeModelSphereBounds();

    private:
        struct StagedSubmesh {
            const uint8_t* vdata = nullptr;
            uint32_t       vertexCount = 0;
            const uint8_t* idata = nullptr;
            uint32_t       indexCount = 0;
        };
        std::vector<StagedSubmesh> m_staged;

    };
    // // --- Skeleton / Animation structs ---
    // constexpr int MAX_BONES = 128;

    // struct BoneInfo {
    //     NE::Math::Mat4 offset;        // inverse bind matrix (from mesh to bone space)
    //     NE::Math::Mat4 finalTransform; // for skinning
    //     int parent = -1;
    //     std::string name;
    // };

    // struct AnimKeyVec3 { NE::Math::Vec3 value; double time; };
    // struct AnimKeyQuat { float x, y, z, w; double time; };

    // struct AnimChannel {
    //     int boneIndex = -1; // which bone this channel animates (-1 if not a skin bone)
    //     std::string nodeName;
    //     std::vector<AnimKeyVec3> positions;
    //     std::vector<AnimKeyQuat> rotations;
    //     std::vector<AnimKeyVec3> scales;
    // };

    // struct AnimationClip {
    //     std::string name;
    //     double duration = 0.0;        // in ticks
    //     double ticksPerSecond = 25.0; // default if not provided
    //     std::vector<AnimChannel> channels;
    //     // Node name -> index into channels (speed up lookups)
    //     std::unordered_map<std::string, int> nodeToChannel;
    // };

    // class Model : public Asset::IAsset {
    // public:
    //     std::vector<SubMesh> meshes;

    //     // Sphere bounds (model space)
    //     NE::Math::Vec3 sphereCenterLS{ 0,0,0 };
    //     float sphereRadiusLS = 0.0f;
    //     bool hasSphereBoundsLS = false;

    //     // Skeleton / Anim
    //     std::unordered_map<std::string, int> m_BoneMapping; // name->index
    //     std::vector<BoneInfo> m_Bones;
    //     NE::Math::Mat4 m_GlobalInverse{};

    //     // Node hierarchy for animation evaluation
    //     struct Node {
    //         std::string name;
    //         int parent = -1;
    //         NE::Math::Mat4 defaultTransform;
    //         std::vector<int> children;
    //     };
    //     std::vector<Node> m_Nodes;
    //     std::unordered_map<std::string, int> m_NodeIndex; // name->index

    //     std::vector<AnimationClip> m_Clips;
    //     int m_CurrentClip = 0;
    //     double m_AnimTime = 0.0;
    //     std::vector<NE::Math::Mat4> m_FinalBones; // size == MAX_BONES

    //     bool LoadFromFile(const std::string& path) override;

    //     void ComputeModelSphereBounds();

    //     // Animation API
    //     bool HasSkeleton() const { return !m_Bones.empty(); }
    //     const std::vector<NE::Math::Mat4>& GetBoneMatrices() const { return m_FinalBones; }
    //     void PlayAnimation(int index);
    //     void UpdateAnimation(double dt);

    //     // IAsset
    //     const std::string& GetUUID() const { return uuid; }
    //     void SetUUID(const std::string& id) { uuid = id; }

    // private:
    //     std::string m_UUID = "Model";


}