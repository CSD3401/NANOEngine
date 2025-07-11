#include "GLPipeline.hpp"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace NANOEngine::Graphics::OpenGL {

    GLPipeline::GLPipeline(const PipelineSpecification& spec, std::string name)
        : m_Spec(spec), m_name(name)
    {}

    GLPipeline::GLPipeline(const PipelineSpecification & spec) : m_Spec(spec)
    {
    }

    void GLPipeline::Bind() const {
        if (m_Spec.EnableDepthTest)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        if (m_Spec.EnableBlending) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else {
            glDisable(GL_BLEND);
        }

        glCullFace(m_Spec.CullMode);
        glPolygonMode(GL_FRONT_AND_BACK, m_Spec.PolygonMode);

        m_Spec.shader->Bind();
    }

    const std::string_view GLPipeline::GetShaderUUID() const
    {
        return m_Spec.shader ? m_Spec.shader->GetUUID() : std::string_view();
    }

}
