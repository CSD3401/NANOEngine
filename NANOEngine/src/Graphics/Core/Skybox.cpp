#include "Skybox.hpp"
#include "GraphicsManager.hpp"
#include "../OpenGL/GLVertexBuffer.hpp"
#include "../OpenGL/GLIndexBuffer.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../OpenGL/GLPipeline.hpp"
#include <glad/glad.h>
#include "../../Math/Mat4.hpp"
#include "../../Math/Vec3.hpp"
#include "Vertex.hpp"
#include "DrawCommand.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "RenderViewManager.hpp"

namespace NE::Graphics {

    Skybox::Skybox() {
        using namespace OpenGL;

        Vertex vertices[] = {
            {{-1.f, -1.f, -1.f}, {}, {}},
            {{ 1.f, -1.f, -1.f}, {}, {}},
            {{ 1.f,  1.f, -1.f}, {}, {}},
            {{-1.f,  1.f, -1.f}, {}, {}},
            {{-1.f, -1.f,  1.f}, {}, {}},
            {{ 1.f, -1.f,  1.f}, {}, {}},
            {{ 1.f,  1.f,  1.f}, {}, {}},
            {{-1.f,  1.f,  1.f}, {}, {}}
        };

        uint32_t indices[] = {
            0,1,2, 2,3,0,
            1,5,6, 6,2,1,
            7,6,5, 5,4,7,
            4,0,3, 3,7,4,
            4,5,1, 1,0,4,
            3,2,6, 6,7,3
        };

        auto vb = std::make_shared<GLVertexBuffer>(vertices, static_cast<uint32_t>(sizeof(vertices)), sizeof(Vertex));
        auto ib = std::make_shared<GLIndexBuffer>(indices, sizeof(indices) / sizeof(uint32_t));
        m_Mesh = std::make_shared<GLGeometryBuffer>(vb, ib);

        auto shader = Resource::ResourceManager::GetInstance().
            LoadResource<GLShader>("neskybox");
        PipelineSpecification spec;
        spec.shader = shader;
        spec.CullMode = GL_BACK;
        spec.EnableDepthTest = true;
        spec.PolygonMode = GL_FILL;
        auto pipeline = std::make_shared<GLPipeline>(spec, "Skybox");
        m_Material = std::make_shared<Material>(pipeline);
		m_Material->SetQueueBase(RenderQueue::BACKGROUND);
    }

    void Skybox::Draw(const RenderView& view) const {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        auto pipeline = m_Material->GetPipeline();
        auto shader = pipeline->GetSpecification().shader;

        m_Material->Bind();
        m_Mesh->Bind();

        Math::Mat4 viewMat = view.view;
		viewMat.SetTranslation(Math::Vec3(0.0f, 0.0f, 0.0f));

        shader->SetUniformMat4("u_View", viewMat);
        shader->SetUniformMat4("u_Projection", view.projection);
        Math::Mat4 modelMat;
        modelMat.SetToIdentity();
        shader->SetUniformMat4("u_Model", modelMat);

        m_Mesh->Draw();
        m_Mesh->Unbind();

        // Restore defaults expected by the rest of your renderer
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
    }

    void Skybox::Submit() const {
        DrawCommand cmd;
        cmd.mesh = m_Mesh;
        cmd.material = m_Material;
        cmd.transform = Math::Mat4::BuildScaling(50.f, 50.f, 50.f);
        GraphicsManager::Submit(cmd);
    }

}