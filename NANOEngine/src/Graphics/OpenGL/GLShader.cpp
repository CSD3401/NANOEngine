#include "GLShader.hpp"
#include <fstream>
#include <sstream>
#include <glad/glad.h>
#include "Core/SpdLogger.hpp"
#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"
#include <iostream>
#include "ResourceManagement/BinaryHeaders/NanoShdHeader.hpp"

namespace {

	// These stage flags must match those in CookShader
    enum : uint32_t {
        STAGE_VS = 1u << 0,
        STAGE_FS = 1u << 4,
        STAGE_CS = 1u << 8
    };

    static GLuint CompileStage(GLenum type, std::string_view src) {
        GLuint sh = glCreateShader(type);
        if (!sh) return 0;
        const char* ptr = src.data();
        GLint len = static_cast<GLint>(src.size());
        glShaderSource(sh, 1, &ptr, &len);
        glCompileShader(sh);
        GLint ok = GL_FALSE;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (ok == GL_TRUE) return sh;

        // log
        GLint logLen = 0; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen ? logLen : 1, '\0');
        if (logLen) glGetShaderInfoLog(sh, logLen, nullptr, log.data());
        SPD_WARNING("Shader stage compile failed: " << log);
        glDeleteShader(sh);
        return 0;
    }

    static GLuint LinkProgram(GLuint vs, GLuint fs) {
        GLuint prog = glCreateProgram();
        if (!prog) return 0;

        // Ensure binary can be retrieved if you want to re-dump later
        glProgramParameteri(prog, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);

        GLint ok = GL_FALSE;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (ok == GL_TRUE) {
            glDetachShader(prog, vs);
            glDetachShader(prog, fs);
            return prog;
        }

        // log
        GLint logLen = 0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen ? logLen : 1, '\0');
        if (logLen) glGetProgramInfoLog(prog, logLen, nullptr, log.data());
        SPD_WARNING("Program link failed: " << log);

        glDetachShader(prog, vs);
        glDetachShader(prog, fs);
        glDeleteProgram(prog);
        return 0;
    }

}

namespace NE::Graphics::OpenGL {

    GLShader::GLShader() : m_programID(0) 
    {
    }

    //GLShader::GLShader(const std::string& filePath)
    //{
    //    std::string source = LoadShaderSource(filePath);
    //    auto shaderSources = Preprocess(source);
    //    Compile(shaderSources);
    //}

    GLShader::~GLShader() 
    {
        glDeleteProgram(m_programID);
    }

    void GLShader::Bind() const 
    {
        glUseProgram(m_programID);
    }

    void GLShader::Unbind() const 
    {
        glUseProgram(0);
    }

    void GLShader::SetUniformInt(const std::string& uName, int value) 
    {
        glUniform1i(GetUniformLocation(uName), value);
    }

    void GLShader::SetUniformFloat(const std::string& uName, float value) 
    {
        glUniform1f(GetUniformLocation(uName), value);
    }

    void GLShader::SetUniformVec3(const std::string& uName, const Vec3& value) 
    {
        glUniform3fv(GetUniformLocation(uName), 1, value.Data());
    }

    void GLShader::SetUniformMat4(const std::string& uName, const Mat4& matrix) 
    {
        glUniformMatrix4fv(GetUniformLocation(uName), 1, GL_FALSE, matrix.Data());
    }

    void GLShader::SetUniformHandle(const std::string& uName, uint64_t handle) 
    {
        // Require ARB_bindless_texture to be loaded by GLAD
        if (!glUniformHandleui64ARB) {
            SPD_WARNING("Bindless texture functions not loaded. Did you enable GL_ARB_bindless_texture in GLAD?");
            return;
        }
        GLint loc = GetUniformLocation(uName);
        if (loc >= 0) glUniformHandleui64ARB(loc, handle);
    }

    void GLShader::SetUniformHandlev(const std::string& uName, const uint64_t* handles, int count) 
    {
        if (!glUniformHandleui64vARB) {
            SPD_WARNING("Bindless texture functions not loaded. Did you enable GL_ARB_bindless_texture in GLAD?");
            return;
        }
        GLint loc = GetUniformLocation(uName);
        if (loc >= 0) glUniformHandleui64vARB(loc, count, handles);
    }

    int GLShader::GetUniformLocation(const std::string& uName) 
    {
        if (m_uniformLocationCache.count(uName))
            return m_uniformLocationCache[uName];
        int location = glGetUniformLocation(m_programID, uName.c_str());
        m_uniformLocationCache[uName] = location;
        return location;
    }

    bool GLShader::Preload(NE::Resource::BinaryView blob) 
    {
        if (blob.size < sizeof(NE::Resource::NanoShdHeader)) return false;
        const auto* h = blob.as<NE::Resource::NanoShdHeader>(0);
        if (!h) return false;
        if (h->magic != NE::Resource::NSHD_MAGIC) return false;
        if (h->importerVersion != NE::Resource::CURRENT_NANOSHD_FORMAT_VERSION) return false;

        const uint64_t progEnd = h->programOffset + h->programSize;
        if (progEnd > blob.size) return false;

        progFormat = h->programBinaryFormat;
        progBlob = blob.data + h->programOffset;
        progSize = static_cast<size_t>(h->programSize);

		const uint32_t stages = h->stagesMask;
		const bool hasVS = (stages & STAGE_VS) != 0;
		const bool hasFS = (stages & STAGE_FS) != 0;
		const bool hasCS = (stages & STAGE_CS) != 0;
		isCompute = hasCS && !hasVS && !hasFS;

        // embedded source fallback layout
        hasFallback = (h->programFlags & 1u) != 0;
        if (hasFallback) {
            size_t off = static_cast<size_t>(progEnd);
			// Fallback for compute shader
            if (isCompute) {
                if (off + 4 > blob.size) {
                    hasFallback = false;

                    return true;
                }
                uint32_t csLen32 = 0;
                std::memcpy(&csLen32, blob.data + off, 4);
                off += 4;

                if (off + csLen32 > blob.size) {
                    hasFallback = false;
                    return true;
                }
                csSrc = reinterpret_cast<const char*>(blob.data + off);
                csLen = static_cast<size_t>(csLen32);
            }
			// Fallback for vertex + fragment shader
            else {
                if (off + 4 > blob.size) { hasFallback = false; return true; }
                uint32_t vsLen32 = 0;
                std::memcpy(&vsLen32, blob.data + off, 4);
                off += 4;

                if (off + vsLen32 + 4 > blob.size) { hasFallback = false; return true; }
                vsSrc = reinterpret_cast<const char*>(blob.data + off);
                vsLen = static_cast<size_t>(vsLen32);
                off += vsLen32;

                if (off + 4 > blob.size) { hasFallback = false; return true; }
                uint32_t fsLen32 = 0;
                std::memcpy(&fsLen32, blob.data + off, 4);
                off += 4;

                if (off + fsLen32 > blob.size) { hasFallback = false; return true; }
                fsSrc = reinterpret_cast<const char*>(blob.data + off);
                fsLen = static_cast<size_t>(fsLen32);
            }
        }
        return true;
    }

    // Need to delete blob after finalizing TODO
    void GLShader::Finalize() 
    {
        if (!progBlob || progSize == 0) { m_programID = 0; return; }

        m_programID = glCreateProgram();
        if (!m_programID) { return; }

        glProgramBinary(m_programID,
            static_cast<GLenum>(progFormat),
            progBlob,
            static_cast<GLsizei>(progSize));

        GLint linked = GL_FALSE;
        glGetProgramiv(m_programID, GL_LINK_STATUS, &linked);
        if (linked == GL_TRUE) {
            return; // success
        } else {
            SPD_WARNING("glProgramBinary failed; attempting embedded source fallback.");
        }

        // Failed to load binary... fall back to embedded source if available
        glDeleteProgram(m_programID);
        m_programID = 0;

        if (!hasFallback) {
            SPD_WARNING("glProgramBinary failed and no source fallback embedded.");
            return;
        }

        GLuint prog = 0;
        // Compute fallback
        if (isCompute) {
            if (!csSrc || csLen == 0) {
                SPD_WARNING("Compute shader fallback requested but no compute source embedded.");
                return;
            }

            GLuint cs = CompileStage(GL_COMPUTE_SHADER, std::string_view(csSrc, csLen));
            if (!cs) {
                return;
            }

            prog = glCreateProgram();
            if (!prog) {
                glDeleteShader(cs);
                return;
            }

            glProgramParameteri(prog, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
            glAttachShader(prog, cs);
            glLinkProgram(prog);
            glDeleteShader(cs);

            GLint linkStatus = GL_FALSE;
            glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);
            if (linkStatus != GL_TRUE) {
                char log[1024];
                glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
                SPD_WARNING("Compute shader program link failed in fallback: " << log);
                glDeleteProgram(prog);
                return;
            }
		}
        // Vertex + Fragment fallback
        else {
            GLuint vs = CompileStage(GL_VERTEX_SHADER, std::string_view(vsSrc, vsLen));
            GLuint fs = CompileStage(GL_FRAGMENT_SHADER, std::string_view(fsSrc, fsLen));
            if (!vs || !fs) {
                if (vs) glDeleteShader(vs);
                if (fs) glDeleteShader(fs);
                return;
            }

            prog = LinkProgram(vs, fs);
            glDeleteShader(vs);
            glDeleteShader(fs);
        }

        if (prog) {
            m_programID = prog;

            // TODO maybe redump fresh binary to disk to refresh the cache and write back into .nshbin
            // using the embedded fallback
        } else {
            // leave program_ = 0 (Finalize failed)
        }
    }

    std::vector<UniformDesc> GLShader::EnumerateActiveUniforms() const 
    {
        GLint count = 0;
        glGetProgramiv(m_programID, GL_ACTIVE_UNIFORMS, &count);
        std::vector<UniformDesc> out;
        out.reserve(count);

        for (GLint i = 0; i < count; ++i) {
            GLchar buf[256]; GLsizei len = 0; GLint size = 0; GLenum type = 0;
            glGetActiveUniform(m_programID, i, sizeof(buf), &len, &size, &type, buf);
            if (len <= 0) continue;

            // Trim GLSL array name suffix "[0]" into a friendlier "name"
            std::string name(buf, buf + len);
            if (auto pos = name.find("[0]"); pos != std::string::npos) name.erase(pos);

            out.push_back({ std::move(name), type, size });
        }
        return out;
    }

    bool GLShader::HasUniform(std::string_view name) const 
    {
        return glGetUniformLocation(m_programID, std::string(name).c_str()) != -1;
    }

    void GLShader::SetUniformMat4Array(const std::string& uName, const Mat4* data, int count) 
    {
        GLint loc = GetUniformLocation(uName);
        if (loc >= 0) {
            glUniformMatrix4fv(loc, count, GL_FALSE, reinterpret_cast<const float*>(data));
        }
    }
}