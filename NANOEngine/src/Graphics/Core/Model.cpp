#include "Model.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../OpenGL/GLVertexBuffer.hpp"
#include "../OpenGL/GLIndexBuffer.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"

namespace NANOEngine::Graphics {

    //std::shared_ptr<Model> LoadModel(const std::string& path) {
    //    Assimp::Importer importer;
    //    const aiScene* scene = importer.ReadFile(path,
    //        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);
    //    if (!scene || !scene->HasMeshes())
    //        return nullptr;

    //    auto model = std::make_shared<Model>();

    //    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
    //        const aiMesh* mesh = scene->mMeshes[m];
    //        SubMesh sub;

    //        sub.vertices.reserve(mesh->mNumVertices);
    //        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
    //            Vertex v;
    //            v.Position = Math::Vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
    //            v.Normal = mesh->HasNormals() ?
    //                Math::Vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) :
    //                Math::Vec3(0.f, 0.f, 0.f);
    //            if (mesh->HasTextureCoords(0))
    //                v.TexCoord = Math::Vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
    //            else
    //                v.TexCoord = Math::Vec2(0.f, 0.f);
    //            sub.vertices.push_back(v);
    //        }

    //        sub.indices.reserve(mesh->mNumFaces * 3);
    //        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
    //            const aiFace& face = mesh->mFaces[i];
    //            for (unsigned int j = 0; j < face.mNumIndices; ++j)
    //                sub.indices.push_back(face.mIndices[j]);
    //        }

    //        auto vb = std::make_shared<OpenGL::GLVertexBuffer>(
    //            sub.vertices.data(),
    //            static_cast<uint32_t>(sub.vertices.size() * sizeof(Vertex)),
    //            sizeof(Vertex));
    //        auto ib = std::make_shared<OpenGL::GLIndexBuffer>(
    //            sub.indices.data(),
    //            static_cast<uint32_t>(sub.indices.size()));
    //        sub.buffer = std::make_shared<OpenGL::GLGeometryBuffer>(vb, ib);

    //        model->meshes.push_back(std::move(sub));
    //    }

    //    return model;
    //}

    bool Model::LoadFromFile(const std::string& path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);
        if (!scene || !scene->HasMeshes())
            return false;

        //auto model = std::make_shared<Model>();

        for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
            const aiMesh* mesh = scene->mMeshes[m];
            SubMesh sub;

            sub.vertices.reserve(mesh->mNumVertices);
            for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
                Vertex v;
                v.Position = Math::Vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
                v.Normal = mesh->HasNormals() ?
                    Math::Vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) :
                    Math::Vec3(0.f, 0.f, 0.f);
                if (mesh->HasTextureCoords(0))
                    v.TexCoord = Math::Vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
                else
                    v.TexCoord = Math::Vec2(0.f, 0.f);
                sub.vertices.push_back(v);
            }

            sub.indices.reserve(mesh->mNumFaces * 3);
            for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
                const aiFace& face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; ++j)
                    sub.indices.push_back(face.mIndices[j]);
            }

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

        return true;
    }
}