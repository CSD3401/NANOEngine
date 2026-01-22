#include "GLPipeline.hpp"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>


namespace NE::Graphics::OpenGL {

    GLPipeline::GLPipeline(const PipelineSpecification& spec, std::string name)
        : m_Spec(spec), m_name(name)
    {}

    GLPipeline::GLPipeline(const PipelineSpecification& spec) : 
        m_Spec(spec), m_Key(PipelineKey::MakeKey(spec))
    {}

    void GLPipeline::Bind() const {

		// Blending
        if (m_Spec.EnableBlending) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else {
            glDisable(GL_BLEND);
        }

		// Depth test
        if (m_Spec.EnableDepthTest) {
            glEnable(GL_DEPTH_TEST);
        }
        else {
            glDisable(GL_DEPTH_TEST);
        }
            
		// Depth write
        if (m_Spec.DepthWrite) {
            glDepthMask(GL_TRUE);
        }
        else {
            glDepthMask(GL_FALSE);
        }

		// Culling
        if (m_Spec.CullMode != GL_NONE) {
            glEnable(GL_CULL_FACE);
            glCullFace(m_Spec.CullMode);
        }
        else {
            glDisable(GL_CULL_FACE);
		}
        
		// Polygon mode
        glPolygonMode(GL_FRONT_AND_BACK, m_Spec.PolygonMode);

		// Shader
        m_Spec.shader->Bind();
    }


    const std::string_view GLPipeline::GetShaderUUID() const
    {
        return m_Spec.shader ? m_Spec.shader->GetUUID() : std::string_view();
    }

}
