#include "Model.hpp"

#include "../OpenGL/GLVertexBuffer.hpp"
#include "../OpenGL/GLIndexBuffer.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"
#include "ResourceManagement/BinaryHeaders/NanoMeshHeader.hpp"

namespace {
    struct CookVertex {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };
}

namespace NE::Graphics {

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

    //bool Model::LoadFromFile(const std::string& path) {
        //Assimp::Importer importer;
        //const aiScene* scene = importer.ReadFile(path,
        //    aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);
        //if (!scene || !scene->HasMeshes())
        //    return false;

        ////auto model = std::make_shared<Model>();

        //for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        //    const aiMesh* mesh = scene->mMeshes[m];
        //    SubMesh sub;

        //    sub.vertices.reserve(mesh->mNumVertices);
        //    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        //        Vertex v;
        //        v.Position = Math::Vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        //        v.Normal = mesh->HasNormals() ?
        //            Math::Vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) :
        //            Math::Vec3(0.f, 0.f, 0.f);
        //        if (mesh->HasTextureCoords(0))
        //            v.TexCoord = Math::Vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        //        else
        //            v.TexCoord = Math::Vec2(0.f, 0.f);
        //        sub.vertices.push_back(v);
        //    }

        //    sub.indices.reserve(mesh->mNumFaces * 3);
        //    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        //        const aiFace& face = mesh->mFaces[i];
        //        for (unsigned int j = 0; j < face.mNumIndices; ++j)
        //            sub.indices.push_back(face.mIndices[j]);
        //    }

        //    auto vb = std::make_shared<OpenGL::GLVertexBuffer>(
        //        sub.vertices.data(),
        //        static_cast<uint32_t>(sub.vertices.size() * sizeof(Vertex)),
        //        sizeof(Vertex));
        //    auto ib = std::make_shared<OpenGL::GLIndexBuffer>(
        //        sub.indices.data(),
        //        static_cast<uint32_t>(sub.indices.size()));
        //    sub.buffer = std::make_shared<OpenGL::GLGeometryBuffer>(vb, ib);

        //    meshes.push_back(std::move(sub));
        //}

        ////ComputeModelSphereBounds();

        //return true;
    //}

    bool Model::Preload(Resource::BinaryView blob) {

        // 1) basic size check
        if (blob.size < sizeof(Resource::NanoMeshHeader))
            return false;

        const auto* hdr = blob.as<Resource::NanoMeshHeader>(0);
        if (!hdr) return false;
        if (hdr->magic != Resource::NMSH_MAGIC) return false;
        if (hdr->version != Resource::CURRENT_NANOMESH_FORMAT_VERSION) return false;

        const size_t subTableOff = sizeof(Resource::NanoMeshHeader);
        const size_t subTableSize = static_cast<size_t>(hdr->submeshCount) * sizeof(Resource::NanoSubmeshDesc);
        if (blob.size < subTableOff + subTableSize)
            return false;

        const auto* subdescs = blob.as<Resource::NanoSubmeshDesc>(subTableOff);
        if (!subdescs) return false;

        m_staged.clear();
        m_staged.reserve(hdr->submeshCount);

        // for each submesh, validate that the vertex/index data is inside the blob
        for (uint16_t i = 0; i < hdr->submeshCount; ++i) {
            const auto& d = subdescs[i];

            const size_t vbytes = static_cast<size_t>(d.vertexCount) * sizeof(CookVertex);;
            // because cook-time vertex = {px,py,pz, nx,ny,nz, u,v} = 8 floats

            const uint8_t* vptr = blob.at(d.vertexDataOffset, vbytes);
            if (!vptr) return false;

            const size_t ibytes = static_cast<size_t>(d.indexCount) * sizeof(uint32_t);
            const uint8_t* iptr = blob.at(d.indexDataOffset, ibytes);
            if (!iptr) return false;

            StagedSubmesh sm{};
            sm.vdata = vptr;
            sm.vertexCount = d.vertexCount;
            sm.idata = iptr;
            sm.indexCount = d.indexCount;

            m_staged.push_back(sm);
        }

        // sphere bounds
        //if (hdr->sphereRadius > 0.0f) {
        //    hasSphereBoundsLS = true;
        //    sphereCenterLS = {
        //        hdr->sphereCenter[0],
        //        hdr->sphereCenter[1],
        //        hdr->sphereCenter[2]
        //    };
        //    sphereRadiusLS = hdr->sphereRadius;
        //} else {
        //    hasSphereBoundsLS = false;
        //}

        return true;
    }

    void Model::Finalize() {
        meshes.clear();
        meshes.reserve(m_staged.size());

        for (const auto& sm : m_staged) {
            SubMesh sub{};

            // 1) copy vertices
            const auto* cv = reinterpret_cast<const CookVertex*>(sm.vdata);
            sub.vertices.resize(sm.vertexCount);
            for (uint32_t i = 0; i < sm.vertexCount; ++i) {
                NE::Graphics::Vertex v{};
                v.Position = { cv[i].px, cv[i].py, cv[i].pz };
                v.Normal = { cv[i].nx, cv[i].ny, cv[i].nz };
                v.TexCoord = { cv[i].u,  cv[i].v };
                sub.vertices[i] = v;
            }

            // 2) copy indices
            const auto* idx = reinterpret_cast<const uint32_t*>(sm.idata);
            sub.indices.assign(idx, idx + sm.indexCount);

            // 3) create GPU buffers (same as your old code)
            auto vb = std::make_shared<NE::Graphics::OpenGL::GLVertexBuffer>(
                sub.vertices.data(),
                static_cast<uint32_t>(sub.vertices.size() * sizeof(NE::Graphics::Vertex)),
                sizeof(NE::Graphics::Vertex));

            auto ib = std::make_shared<NE::Graphics::OpenGL::GLIndexBuffer>(
                sub.indices.data(),
                static_cast<uint32_t>(sub.indices.size()));

            sub.buffer = std::make_shared<NE::Graphics::OpenGL::GLGeometryBuffer>(vb, ib);

            meshes.push_back(std::move(sub));
        }

        m_staged.clear();
    }

    //void Model::ComputeModelSphereBounds() {
    //    if (meshes.empty())
    //    {
    //        return;
    //    }

    //    Vec3 minLS{ 
    //        std::numeric_limits<float>::infinity(),
    //        std::numeric_limits<float>::infinity(),
    //        std::numeric_limits<float>::infinity() 
    //    };

    //    Vec3 maxLS{ 
    //        -std::numeric_limits<float>::infinity(),
    //        -std::numeric_limits<float>::infinity(),
    //        -std::numeric_limits<float>::infinity()
    //    };

    //    for (const auto& mesh : meshes) 
    //    {
    //        for (const auto& vertex : mesh.vertices) 
    //        {
    //            const Vec3& pos = vertex.Position;

    //            minLS = Vec3{ std::min(minLS.x, pos.x), std::min(minLS.y, pos.y), std::min(minLS.z, pos.z) };
    //            maxLS = Vec3{ std::max(maxLS.x, pos.x), std::max(maxLS.y, pos.y), std::max(maxLS.z, pos.z) };

    //        }
    //    }

    //    sphereCenterLS = { 
    //        (minLS.x + maxLS.x) * 0.5f,
    //        (minLS.y + maxLS.y) * 0.5f,
    //        (minLS.z + maxLS.z) * 0.5f 
    //    };

    //    const Vec3 extents{ 
    //        (maxLS.x - minLS.x) * 0.5f,
    //        (maxLS.y - minLS.y) * 0.5f,
    //        (maxLS.z - minLS.z) * 0.5f
    //    };

    //    sphereRadiusLS = extents.Length();
    //    hasSphereBoundsLS = true;
    //}
}