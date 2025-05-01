#include "GLPipeline.hpp"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace NANOEngine::Graphics::OpenGL {

    GLPipeline::GLPipeline(const GraphicsPipelineDesc& desc) {
        m_ProgramID = LoadShaderProgram(desc.vertexShaderPath, desc.fragmentShaderPath);
    }

    GLPipeline::~GLPipeline() {
        glDeleteProgram(m_ProgramID);
    }

    void GLPipeline::Bind() {
        glUseProgram(m_ProgramID);
    }

    unsigned int GLPipeline::LoadShaderProgram(const char* vertexPath, const char* fragmentPath) {
        // Load vertex shader
        std::ifstream vertexFile(vertexPath);
        std::stringstream vertexStream;
        vertexStream << vertexFile.rdbuf();
        std::string vertexCode = vertexStream.str();
        vertexFile.close();

        // Load fragment shader
        std::ifstream fragmentFile(fragmentPath);
        std::stringstream fragmentStream;
        fragmentStream << fragmentFile.rdbuf();
        std::string fragmentCode = fragmentStream.str();
        fragmentFile.close();

        // Compile shaders
        unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexCode.c_str());
        unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentCode.c_str());

        // Create program
        unsigned int program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        // Check link status
        int success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, NULL, infoLog);
            std::cerr << "Shader Program Link Error: " << infoLog << std::endl;
        }

        // Clean up shaders (they're linked into program now)
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return program;
    }

    unsigned int GLPipeline::CompileShader(unsigned int type, const char* source) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        // Check compilation status
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            std::cerr << "Shader Compile Error: " << infoLog << std::endl;
        }

        return shader;
    }

}
