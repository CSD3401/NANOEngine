#include "RenderSystem.hpp"
#include "../Components/Renderer.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Light.hpp"
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
	//static std::shared_ptr<Graphics::IShader> basicShader;
	//static std::shared_ptr<Graphics::IPipeline> pipeline;
	//static std::shared_ptr<Graphics::Material> material;

    static std::shared_ptr<Graphics::IShader> pickingShader;
    static std::shared_ptr<Graphics::IPipeline> pickingPipeline;
    static std::shared_ptr<Graphics::Material> pickingMaterial;

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


   //     const auto& entities = GetEntities();
   //     for (Entity entity : entities) {
			//auto& renderer = m_componentManager->GetComponent<Component::Renderer>(entity);
   //     }

		//basicShader = std::make_shared<Graphics::OpenGL::GLShader>("Library/Shaders/Basic.glsl");
		//Graphics::PipelineSpecification pipelineSpec;
		//pipelineSpec.shader = basicShader;
		//pipelineSpec.CullMode = GL_BACK;
		//pipelineSpec.PolygonMode = GL_FILL;
		//pipelineSpec.EnableDepthTest = true;
		//pipeline = std::make_shared<Graphics::OpenGL::GLPipeline>(pipelineSpec, "Basic");
		//material = std::make_shared<Graphics::Material>(pipeline);

        //pickingShader = std::make_shared<Graphics::OpenGL::GLShader>("Library/Shaders/Picking.glsl");
        //Graphics::PipelineSpecification pickSpec;
        //pickSpec.shader = pickingShader;
        //pickSpec.CullMode = GL_BACK;
        //pickSpec.PolygonMode = GL_FILL;
        //pickSpec.EnableDepthTest = true;
        //pickingPipeline = std::make_shared<Graphics::OpenGL::GLPipeline>(pickSpec, "Picking");
        //pickingMaterial = std::make_shared<Graphics::Material>(pickingPipeline);
    }

    void RenderSystem::Update(double) {
		NE_PROFILE_FUNCTION();
        const auto& entities = GetEntities();
        for (Entity entity : entities) {
            auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
            auto& renderer = m_componentManager->GetComponent<Component::Renderer>(entity);

			if (!renderer.model && !renderer.modelPath.empty())
				renderer.model = Graphics::LoadModel(renderer.modelPath.string());
			if (!renderer.model)
				continue;

			for (auto& sub : renderer.model->meshes) {
				Graphics::DrawCommand cmd;
				cmd.mesh = sub.buffer;
				cmd.material = renderer.material;
				cmd.transform = transform.modelMatrix;

                //cmd.material->SetUniformVec3("u_Material.ambient", { 0.1f, 0.1f, 0.1f });
                //cmd.material->SetUniformVec3("u_Material.diffuse", { 1.0f, 0.5f, 0.31f });
                //cmd.material->SetUniformVec3("u_Material.specular", { 0.5f, 0.5f, 0.5f });
                //cmd.material->SetUniformFloat("u_Material.shininess", 32.0f);

				Graphics::GraphicsManager::Submit(cmd);
			}
        }
    }

    void RenderSystem::RenderPicking() {
        //const auto& entities = GetEntities();
        //for (Entity entity : entities) {
        //    auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
        //    auto& renderer = m_componentManager->GetComponent<Component::Renderer>(entity);

        //    //if (!renderer.model && !renderer.modelPath.empty())
        //    //    renderer.model = Graphics::LoadModel(renderer.modelPath.string());
        //    if (!renderer.model)
        //        continue;

        //    float r = (float)(entity & 0xFF) / 255.0f;
        //    float g = (float)((entity >> 8) & 0xFF) / 255.0f;
        //    float b = (float)((entity >> 16) & 0xFF) / 255.0f;

        //    for (auto& sub : renderer.model->meshes) {
        //        Graphics::DrawCommand cmd;
        //        cmd.mesh = sub.buffer;
        //        cmd.material = pickingMaterial;
        //        cmd.transform = transform.modelMatrix;
        //        pickingMaterial->SetUniformVec3("u_ID", { r, g, b });
        //        Graphics::GraphicsManager::Submit(cmd);
        //    }
        //}
    }

    void RenderSystem::Exit()
    {
    }

}
