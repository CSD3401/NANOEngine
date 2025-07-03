#include "GLShader.hpp"
#include <fstream>
#include <sstream>
#include "glad/glad.h"
#include "../../Core/Logger.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"


namespace NANOEngine::Graphics::OpenGL {
    static GLenum ShaderTypeFromString(const std::string& type) {
        if (type == " vertex") return GL_VERTEX_SHADER;
        if (type == " fragment") return GL_FRAGMENT_SHADER;
        throw std::runtime_error("Unknown shader type: " + type);
    }

    GLShader::GLShader(const std::string& filepath) {
        std::string source = LoadShaderSource(filepath);
        auto shaderSources = Preprocess(source);
        Compile(shaderSources);
    }

    GLShader::~GLShader() {
        glDeleteProgram(m_programID);
    }

    void GLShader::Bind() const {
        glUseProgram(m_programID);
    }

    void GLShader::Unbind() const {
        glUseProgram(0);
    }

    void GLShader::SetUniformInt(const std::string& name, int value) {
        glUniform1i(GetUniformLocation(name), value);
    }

    void GLShader::SetUniformFloat(const std::string& name, float value) {
        glUniform1f(GetUniformLocation(name), value);
    }

    void GLShader::SetUniformVec3(const std::string& name, const Vec3& value) {
        glUniform3fv(GetUniformLocation(name), 1, value.Data());
    }

    void GLShader::SetUniformMat4(const std::string& name, const Mat4& matrix) {
        glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, matrix.Data());
    }

    std::string GLShader::LoadShaderSource(const std::string& path)
    {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    std::unordered_map<GLenum, std::string> GLShader::Preprocess(const std::string& source)
    {
        std::unordered_map<GLenum, std::string> shaderSources;

        const std::string typeToken = "#type";
        size_t pos = source.find(typeToken);
        while (pos != std::string::npos) {
            size_t eol = source.find_first_of("\r\n", pos);
            std::string type = source.substr(pos + typeToken.length(), eol - pos - typeToken.length());
            size_t nextLinePos = source.find_first_not_of("\r\n", eol);
            size_t nextTypePos = source.find(typeToken, nextLinePos);
            shaderSources[ShaderTypeFromString(type)] = source.substr(nextLinePos, nextTypePos - nextLinePos);
            pos = nextTypePos;
        }

        return shaderSources;
    }

    void GLShader::Compile(const std::unordered_map<GLenum, std::string>& shaderSources)
    {
        uint32_t program = glCreateProgram();
        std::vector<GLuint> shaderIDs;

        for (auto& [type, source] : shaderSources) {
            GLuint shader = glCreateShader(type);
            const char* src = source.c_str();
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);

            GLint compiled;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled != GL_TRUE) {
                char log[1024];
                glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
                //LOG_WARNING("Shader compilation failed: ", log);
                LOG_WARNING(std::string("Shader compilation failed:\n") + log);
            }

            glAttachShader(program, shader);
            shaderIDs.push_back(shader);
        }

        glLinkProgram(program);
        GLint linked;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            char log[1024];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            LOG_WARNING(std::string("Program linking failed:\n") + log);
        }

        for (auto id : shaderIDs) {
            glDetachShader(program, id);
            glDeleteShader(id);
        }

        m_programID = program;
    }

    int GLShader::GetUniformLocation(const std::string& name) {
        if (m_uniformLocationCache.count(name))
            return m_uniformLocationCache[name];
        int location = glGetUniformLocation(m_programID, name.c_str());
        m_uniformLocationCache[name] = location;
        return location;
    }
}