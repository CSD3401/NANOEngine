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
        m_mesh = std::make_shared<GLGeometryBuffer>(vb, ib);

        auto shader = Resource::ResourceManager::GetInstance().
            LoadResource<GLShader>("neskybox");
        PipelineSpecification spec;
        spec.shader = shader;
        spec.CullMode = GL_BACK;
        spec.EnableDepthTest = true;
        spec.DepthWrite = false;
        spec.PolygonMode = GL_FILL;
        auto pipeline = std::make_shared<GLPipeline>(spec, "Skybox");
        m_material = std::make_shared<Material>(pipeline);
		m_material->SetQueueBase(RenderQueue::BACKGROUND);
    }

    void Skybox::Draw(const RenderView& view) const {
        glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);

        auto pipeline = m_material->GetPipeline();
        auto shader = pipeline->GetSpecification().shader;

        m_material->Bind();
        m_mesh->Bind();

        shader->SetUniformMat4("u_View", view.view);
        shader->SetUniformMat4("u_Projection", view.projection);

        m_mesh->Draw();
        m_mesh->Unbind();

		glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    std::shared_ptr<IPipeline> Skybox::GetSkyboxPipeline() const {
		return m_material->GetPipeline();
    }

}