#include "RenderSystem.hpp"
#include "../Components/Renderer.hpp"
#include "../Components/Transform.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"

//temp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../../Graphics/Core/Vertex.hpp"
#include "../../Graphics/OpenGL/GLVertexBuffer.hpp"
#include "../../Graphics/OpenGL/GLIndexBuffer.hpp"
#include "../../Graphics/OpenGL/GLGeometryBuffer.hpp"
#include "../../Graphics/OpenGL/GLShader.hpp"
#include "../../Graphics/OpenGL/GLPipeline.hpp"
#include "../../Graphics/Core/Material.hpp"
#include "../../Core/Profiler.hpp"
#include <glad/glad.h>

namespace NANOEngine::ECS::Systems {

	static std::shared_ptr<Graphics::IGeometryBuffer> monkeyMesh;
	static std::shared_ptr<Graphics::IShader> basicShader;
	static std::shared_ptr<Graphics::IPipeline> pipeline;
	static std::shared_ptr<Graphics::Material> material;

	//  TEMPORARY
	std::shared_ptr<Graphics::IGeometryBuffer> LoadModel(const std::string& path) {
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);
		if (!scene || !scene->HasMeshes())
			return nullptr;

		const aiMesh* mesh = scene->mMeshes[0]; // Only first mesh for testing

		//std::vector<float> vertices;
		std::vector<uint32_t> indices;

		//for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
		//	aiVector3D pos = mesh->mVertices[i];
		//	vertices.push_back(pos.x);
		//	vertices.push_back(pos.y);
		//	vertices.push_back(pos.z);
		//}
		std::vector<Graphics::Vertex> vertices;
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
			Graphics::Vertex v;
			v.Position = Math::Vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
			v.Normal = mesh->HasNormals() ? Math::Vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : Math::Vec3(0, 0, 0);
			if (mesh->HasTextureCoords(0))
				v.TexCoord = Math::Vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
			else
				v.TexCoord = Math::Vec2(0.0f, 0.0f);
			vertices.push_back(v);
		}


		for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; ++j)
				indices.push_back(face.mIndices[j]);
		}

		//auto vb = std::make_shared<Graphics::OpenGL::GLVertexBuffer>(
		//	vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float)), 3 * sizeof(float));
		auto vb = std::make_shared<Graphics::OpenGL::GLVertexBuffer>(
			vertices.data(),
			static_cast<uint32_t>(vertices.size() * sizeof(Graphics::Vertex)),
			sizeof(Graphics::Vertex));
		auto ib = std::make_shared<Graphics::OpenGL::GLIndexBuffer>(
			indices.data(), static_cast<uint32_t>(indices.size()));
		return std::make_shared<Graphics::OpenGL::GLGeometryBuffer>(vb, ib);
	}

    RenderSystem::RenderSystem(ComponentManager* cm) : m_componentManager(cm)
    {
    }

    void RenderSystem::OnEntityAdded(Entity)
    {
    }

    void RenderSystem::OnEntityRemoved(Entity)
    {
    }

    void RenderSystem::Init()
    {
		monkeyMesh = LoadModel("Assets/Models/suzanne.obj");

		basicShader = std::make_shared<Graphics::OpenGL::GLShader>("Library/Shaders/Basic.glsl");
		Graphics::PipelineSpecification pipelineSpec;
		pipelineSpec.shader = basicShader;
		pipelineSpec.CullMode = GL_BACK;
		pipelineSpec.PolygonMode = GL_FILL;
		pipelineSpec.EnableDepthTest = true;
		pipeline = std::make_shared<Graphics::OpenGL::GLPipeline>(pipelineSpec);

		material = std::make_shared<Graphics::Material>(pipeline);
    }

    void RenderSystem::Update(double) {
		NE_PROFILE_FUNCTION();
        const auto& entities = GetEntities();
        for (Entity entity : entities) {
            auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
            //auto& renderer = m_componentManager->GetComponent<Renderer>(entity);

            Graphics::DrawCommand cmd;
            cmd.mesh = monkeyMesh;
            cmd.material = material;
            cmd.transform = transform.modelMatrix;

            Graphics::GraphicsManager::Submit(cmd);
        }
    }

    void RenderSystem::Exit()
    {
    }

}
