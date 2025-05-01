#pragma once

#include "../IPipeline.hpp"

namespace NANOEngine::Graphics::OpenGL {

    class GLPipeline final : public IPipeline {
    public:
        GLPipeline(const GraphicsPipelineDesc& desc);
        ~GLPipeline();

        void Bind() override;

    private:
        unsigned int m_ProgramID; // OpenGL shader program

        unsigned int LoadShaderProgram(const char* vertexPath, const char* fragmentPath);
        unsigned int CompileShader(unsigned int type, const char* source);
    };

}
