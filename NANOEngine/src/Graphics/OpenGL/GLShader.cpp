#include "GLShader.hpp"
#include <fstream>
#include <sstream>
#include "glad/glad.h"
#include "../../Core/Logger.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"


namespace NE::Graphics::OpenGL {

    static std::string Trim(const std::string& str) {
        const char* whitespace = " \t\n\r";
        size_t start = str.find_first_not_of(whitespace);
        if (start == std::string::npos)
            return "";
        size_t end = str.find_last_not_of(whitespace);
        return str.substr(start, end - start + 1);
    }

    static GLenum ShaderTypeFromString(std::string& type) {
        type = Trim(type);
        if (type == "vertex") return GL_VERTEX_SHADER;
        if (type == "fragment") return GL_FRAGMENT_SHADER;
        throw std::runtime_error("Unknown shader type: " + type);
    }

    GLShader::GLShader() : m_programID(0) {
    }

    GLShader::GLShader(const std::string& filePath)
    {
        std::string source = LoadShaderSource(filePath);
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

    void GLShader::SetUniformInt(const std::string& uName, int value) {
        glUniform1i(GetUniformLocation(uName), value);
    }

    void GLShader::SetUniformFloat(const std::string& uName, float value) {
        glUniform1f(GetUniformLocation(uName), value);
    }

    void GLShader::SetUniformVec3(const std::string& uName, const Vec3& value) {
        glUniform3fv(GetUniformLocation(uName), 1, value.Data());
    }

    void GLShader::SetUniformMat4(const std::string& uName, const Mat4& matrix) {
        glUniformMatrix4fv(GetUniformLocation(uName), 1, GL_FALSE, matrix.Data());
    }

    bool GLShader::LoadFromFile(const std::string& fileName)
    {
        std::string source = LoadShaderSource(fileName);
        auto shaderSources = Preprocess(source);
        return Compile(shaderSources);
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

    bool GLShader::Compile(const std::unordered_map<GLenum, std::string>& shaderSources)
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
                //LOG_WARNING(std::string("Shader compilation failed:\n") + log);
                LOG_WARNING("Shader compilation failed:\n" << log << "\nShader Source:\n" << source);
                return false;
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
            //LOG_WARNING(std::string("Program linking failed:\n") + log);
            LOG_WARNING("Program linking failed:\n" << log);
            return false;
        }

        for (auto id : shaderIDs) {
            glDetachShader(program, id);
            glDeleteShader(id);
        }

        m_programID = program;
        return true;
    }

    int GLShader::GetUniformLocation(const std::string& uName) {
        if (m_uniformLocationCache.count(uName))
            return m_uniformLocationCache[uName];
        int location = glGetUniformLocation(m_programID, uName.c_str());
        m_uniformLocationCache[uName] = location;
        return location;
    }
}