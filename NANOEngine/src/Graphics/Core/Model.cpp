#include "Model.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../OpenGL/GLVertexBuffer.hpp"
#include "../OpenGL/GLIndexBuffer.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"
#include <algorithm>
#include <limits>

using namespace NE::Math;

namespace NE::Graphics {

    static Mat4 AiToMat4(const aiMatrix4x4& m) {
        return Mat4(
            m.a1, m.a2, m.a3, m.a4,
            m.b1, m.b2, m.b3, m.b4,
            m.c1, m.c2, m.c3, m.c4,
            m.d1, m.d2, m.d3, m.d4
        );
    }

    static Vec3 AiToVec3(const aiVector3D& v) { return { v.x, v.y, v.z }; }

    static aiMatrix4x4 ComposeTRS(const aiVector3D& t, const aiQuaternion& r, const aiVector3D& s) {
        aiMatrix4x4 T; aiMatrix4x4::Translation(t, T);
        aiMatrix3x3 R3 = r.GetMatrix();
        aiMatrix4x4 R(
            R3.a1, R3.a2, R3.a3, 0.0f,
            R3.b1, R3.b2, R3.b3, 0.0f,
            R3.c1, R3.c2, R3.c3, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
        aiMatrix4x4 S; aiMatrix4x4::Scaling(s, S);
        return T * R * S;
    }

    // Helper to add up to 4 influences per vertex
    static void AddBoneData(Vertex& v, int boneID, float weight) {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            if (v.Weights[i] == 0.0f) {
                v.BoneIDs[i] = boneID;
                v.Weights[i] = weight;
                return;
            }
        }
        // Replace the smallest weight if full
        int minIndex = 0;
        for (int i = 1; i < MAX_BONE_INFLUENCE; ++i)
            if (v.Weights[i] < v.Weights[minIndex]) minIndex = i;
        if (weight > v.Weights[minIndex]) {
            v.BoneIDs[minIndex] = boneID;
            v.Weights[minIndex] = weight;
        }
    }

    // Build node list recursively
    static void BuildNodes(Model& M, const aiNode* node, int parent) {
        int idx = (int)M.m_Nodes.size();
        Model::Node N;
        N.name = node->mName.C_Str();
        N.parent = parent;
        N.defaultTransform = AiToMat4(node->mTransformation);
        M.m_NodeIndex[N.name] = idx;
        M.m_Nodes.push_back(N);

        for (unsigned i = 0; i < node->mNumChildren; ++i) {
            int childIndexStart = (int)M.m_Nodes.size();
            BuildNodes(M, node->mChildren[i], idx);
            M.m_Nodes[idx].children.push_back(childIndexStart);
        }
    }

    bool Model::LoadFromFile(const std::string& path) {
        Assimp::Importer importer;

        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, 0);

        const unsigned int kFlags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_FlipUVs |
            aiProcess_GenSmoothNormals |
            aiProcess_ImproveCacheLocality |
            aiProcess_SortByPType |
            aiProcess_ValidateDataStructure |
            aiProcess_OptimizeMeshes |
            aiProcess_OptimizeGraph |
            aiProcess_LimitBoneWeights; // no PreTransformVertices for skinning

        const aiScene* scene = importer.ReadFile(path, kFlags);
        if (!scene || !scene->HasMeshes())
            return false;

        meshes.clear();
        m_Bones.clear();
        m_BoneMapping.clear();
        m_Nodes.clear();
        m_NodeIndex.clear();
        m_Clips.clear();
        m_FinalBones.assign(MAX_BONES, Mat4{});

        // Global inverse for skinning
        aiMatrix4x4 global = scene->mRootNode ? scene->mRootNode->mTransformation : aiMatrix4x4();
        aiMatrix4x4 invGlobal = global;
        invGlobal.Inverse();
        m_GlobalInverse = AiToMat4(invGlobal);

        // Build node hierarchy
        if (scene->mRootNode) {
            BuildNodes(*this, scene->mRootNode, -1);
        }

        // Create meshes and collect bones/weights
        meshes.reserve(scene->mNumMeshes);
        for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
            const aiMesh* mesh = scene->mMeshes[m];
            SubMesh sub;

            sub.vertices.resize(mesh->mNumVertices);
            for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
                Vertex v{};
                v.Position = AiToVec3(mesh->mVertices[i]);
                v.Normal = mesh->HasNormals() ? AiToVec3(mesh->mNormals[i]) : Vec3{ 0,1,0 };
                if (mesh->HasTextureCoords(0))
                    v.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
                else
                    v.TexCoord = { 0.0f, 0.0f };
                sub.vertices[i] = v;
            }

            // Indices
            sub.indices.reserve(mesh->mNumFaces * 3);
            for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
                const aiFace& face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; ++j)
                    sub.indices.push_back(face.mIndices[j]);
            }

            // Bones
            if (mesh->HasBones()) {
                for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
                    const aiBone* bone = mesh->mBones[b];
                    std::string boneName = bone->mName.C_Str();

                    int boneIndex = 0;
                    auto it = m_BoneMapping.find(boneName);
                    if (it == m_BoneMapping.end()) {
                        boneIndex = (int)m_Bones.size();
                        m_BoneMapping[boneName] = boneIndex;
                        BoneInfo info;
                        info.name = boneName;
                        info.offset = AiToMat4(bone->mOffsetMatrix);
                        info.finalTransform = Mat4{};
                        // fill parent if we have such node
                        auto nit = m_NodeIndex.find(boneName);
                        if (nit != m_NodeIndex.end()) info.parent = m_Nodes[nit->second].parent;
                        m_Bones.push_back(info);
                    }
                    else {
                        boneIndex = it->second;
                    }

                    for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                        const aiVertexWeight& vw = bone->mWeights[w];
                        if (vw.mVertexId < sub.vertices.size()) {
                            AddBoneData(sub.vertices[vw.mVertexId], boneIndex, vw.mWeight);
                        }
                    }
                }
            }

            // Build GL buffers
            auto vb = std::make_shared<OpenGL::GLVertexBuffer>(
                sub.vertices.data(),
                static_cast<uint32_t>(sub.vertices.size() * sizeof(Vertex)),
                sizeof(Vertex));
            auto ib = std::make_shared<OpenGL::GLIndexBuffer>(
                sub.indices.data(),
                static_cast<uint32_t>(sub.indices.size()));
            sub.buffer = std::make_shared<OpenGL::GLGeometryBuffer>(vb, ib);

            meshes.push_back(std::move(sub));
        }

        // Animations (first clip by default)
        if (scene->HasAnimations()) {
            for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
                const aiAnimation* anim = scene->mAnimations[a];
                AnimationClip clip;
                clip.name = anim->mName.C_Str();
                clip.duration = anim->mDuration;
                clip.ticksPerSecond = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;

                for (unsigned c = 0; c < anim->mNumChannels; ++c) {
                    const aiNodeAnim* ch = anim->mChannels[c];
                    AnimChannel channel;
                    channel.nodeName = ch->mNodeName.C_Str();

                    auto bIt = m_BoneMapping.find(channel.nodeName);
                    channel.boneIndex = (bIt != m_BoneMapping.end()) ? bIt->second : -1;

                    for (unsigned i = 0; i < ch->mNumPositionKeys; ++i)
                        channel.positions.push_back({ AiToVec3(ch->mPositionKeys[i].mValue), ch->mPositionKeys[i].mTime });

                    for (unsigned i = 0; i < ch->mNumRotationKeys; ++i) {
                        const aiQuatKey& qk = ch->mRotationKeys[i];
                        channel.rotations.push_back({ qk.mValue.x, qk.mValue.y, qk.mValue.z, qk.mValue.w, qk.mTime });
                    }
                    for (unsigned i = 0; i < ch->mNumScalingKeys; ++i)
                        channel.scales.push_back({ AiToVec3(ch->mScalingKeys[i].mValue), ch->mScalingKeys[i].mTime });

                    int chIndex = (int)clip.channels.size();
                    clip.nodeToChannel[channel.nodeName] = chIndex;
                    clip.channels.push_back(std::move(channel));
                }

                m_Clips.push_back(std::move(clip));
            }
        }

        ComputeModelSphereBounds();
        // Initialize final bones to identity
        for (int i = 0; i < MAX_BONES; ++i) m_FinalBones[i].SetToIdentity();

        return true;
    }

    void Model::ComputeModelSphereBounds() {
        if (meshes.empty()) {
            hasSphereBoundsLS = false;
            sphereCenterLS = { 0,0,0 };
            sphereRadiusLS = 0.0f;
            return;
        }

        Vec3 minLS{ std::numeric_limits<float>::infinity(),
                     std::numeric_limits<float>::infinity(),
                     std::numeric_limits<float>::infinity() };
        Vec3 maxLS{ -std::numeric_limits<float>::infinity(),
                    -std::numeric_limits<float>::infinity(),
                    -std::numeric_limits<float>::infinity() };

        for (const auto& mesh : meshes) {
            for (const auto& vertex : mesh.vertices) {
                const Vec3& p = vertex.Position;
                minLS = { std::min(minLS.x, p.x), std::min(minLS.y, p.y), std::min(minLS.z, p.z) };
                maxLS = { std::max(maxLS.x, p.x), std::max(maxLS.y, p.y), std::max(maxLS.z, p.z) };
            }
        }

        sphereCenterLS = { (minLS.x + maxLS.x) * 0.5f,
                           (minLS.y + maxLS.y) * 0.5f,
                           (minLS.z + maxLS.z) * 0.5f };

        const Vec3 extents{ (maxLS.x - minLS.x) * 0.5f,
                            (maxLS.y - minLS.y) * 0.5f,
                            (maxLS.z - minLS.z) * 0.5f };

        sphereRadiusLS = extents.Length();
        hasSphereBoundsLS = true;
    }

    void Model::PlayAnimation(int index) {
        if (index >= 0 && index < (int)m_Clips.size()) {
            m_CurrentClip = index;
            m_AnimTime = 0.0;
        }
    }

    static int FindKey(double t, const std::vector<AnimKeyVec3>& keys) {
        if (keys.empty()) return -1;
        for (int i = (int)keys.size() - 2; i >= 0; --i)
            if (t >= keys[i].time) return i;
        return 0;
    }
    static int FindKeyQ(double t, const std::vector<AnimKeyQuat>& keys) {
        if (keys.empty()) return -1;
        for (int i = (int)keys.size() - 2; i >= 0; --i)
            if (t >= keys[i].time) return i;
        return 0;
    }

    static aiVector3D InterpVec3(double t, const std::vector<AnimKeyVec3>& keys) {
        if (keys.empty()) return { 0,0,0 };
        if (keys.size() == 1) return { keys[0].value.x, keys[0].value.y, keys[0].value.z };
        int i = FindKey(t, keys);
        int j = std::min(i + 1, (int)keys.size() - 1);
        double dt = keys[j].time - keys[i].time;
        float f = (dt <= 0.0) ? 0.0f : float((t - keys[i].time) / dt);
        const Vec3& a = keys[i].value, & b = keys[j].value;
        return { a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f };
    }

    static aiQuaternion InterpQuat(double t, const std::vector<AnimKeyQuat>& keys) {
        if (keys.empty()) return aiQuaternion(1, 0, 0, 0);
        if (keys.size() == 1) return aiQuaternion(keys[0].w, keys[0].x, keys[0].y, keys[0].z);
        int i = FindKeyQ(t, keys);
        int j = std::min(i + 1, (int)keys.size() - 1);
        double dt = keys[j].time - keys[i].time;
        float f = (dt <= 0.0) ? 0.0f : float((t - keys[i].time) / dt);
        aiQuaternion qa(keys[i].w, keys[i].x, keys[i].y, keys[i].z);
        aiQuaternion qb(keys[j].w, keys[j].x, keys[j].y, keys[j].z);
        aiQuaternion out; aiQuaternion::Interpolate(out, qa, qb, f); out.Normalize();
        return out;
    }

    void Model::UpdateAnimation(double dt) {
        if (m_Clips.empty() || m_Nodes.empty()) return;

        const AnimationClip& clip = m_Clips[m_CurrentClip];
        double tps = (clip.ticksPerSecond != 0.0) ? clip.ticksPerSecond : 25.0;
        m_AnimTime += dt * tps;
        double timeInTicks = fmod(m_AnimTime, clip.duration);

        // Evaluate node transforms
        std::vector<Mat4> nodeGlobals(m_Nodes.size());
        for (size_t ni = 0; ni < m_Nodes.size(); ++ni) {
            const Node& N = m_Nodes[ni];
            aiVector3D T(0, 0, 0), S(1, 1, 1);
            aiQuaternion R(1, 0, 0, 0);

            Mat4 local;
            auto chIt = clip.nodeToChannel.find(N.name);
            if (chIt != clip.nodeToChannel.end()) {
                const AnimChannel& ch = clip.channels[chIt->second];

                // Fallbacks per track to avoid zero-scale issues
                const bool hasPos = !ch.positions.empty();
                const bool hasRot = !ch.rotations.empty();
                const bool hasScl = !ch.scales.empty();

                aiVector3D T = hasPos ? InterpVec3(timeInTicks, ch.positions) : aiVector3D(0, 0, 0);
                aiQuaternion R = hasRot ? InterpQuat(timeInTicks, ch.rotations) : aiQuaternion(1, 0, 0, 0);
                aiVector3D S = hasScl ? InterpVec3(timeInTicks, ch.scales) : aiVector3D(1, 1, 1);

                aiMatrix4x4 M = ComposeTRS(T, R, S);
                local = AiToMat4(M);
            }
            else {
                // Use the node's bind/default local transform if it has no animated channel
                local = N.defaultTransform;
            }

            if (N.parent >= 0) nodeGlobals[ni] = nodeGlobals[N.parent] * local;
            else               nodeGlobals[ni] = local;

        }

        // Build final bone matrices
        for (size_t b = 0; b < m_Bones.size() && b < (size_t)MAX_BONES; ++b) {
            const std::string& name = m_Bones[b].name;
            auto nit = m_NodeIndex.find(name);
            Mat4 global;
            if (nit != m_NodeIndex.end())
                global = nodeGlobals[nit->second];
            else
                global.SetToIdentity();
            m_FinalBones[b] = m_GlobalInverse * global * m_Bones[b].offset;
        }
    }
}