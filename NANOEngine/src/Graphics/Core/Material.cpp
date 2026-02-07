#include "Material.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../OpenGL/GLPipeline.hpp"
#include "../OpenGL/GLTexture.hpp"
#include "PipelineCache.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "ResourceManagement/BinaryHeaders/NanoMatHeader.hpp"
#include <glad/glad.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <cctype>


namespace NE::Graphics {
    namespace {
        bool IsEngineUniform(std::string_view n) {
            // Built-in engine uniforms
            if (n == "u_Model" || n == "u_View" || n == "u_Projection" ||
                n == "u_NormalMatrix" || n == "u_CameraPos" ||
                n == "u_numLights" || n.rfind("u_lights", 0) == 0 ||
                n == "u_ShadingModel" || n.rfind("u_Has", 0) == 0)
                return true;

            if (n.rfind("i_", 0) == 0)
                return true;

            return false;
        }

        bool IsSampler(GLenum t) {
            switch (t) {
            case GL_SAMPLER_2D: case GL_SAMPLER_2D_ARRAY: case GL_SAMPLER_CUBE:
            case GL_INT_SAMPLER_2D: case GL_UNSIGNED_INT_SAMPLER_2D:
            case GL_SAMPLER_2D_SHADOW: case GL_SAMPLER_CUBE_SHADOW:
                return true;
            default: return false;
            }
        }

        NE::Graphics::RenderQueue ParseRenderQueue(std::string_view name) {
            using namespace NE::Graphics;
		    // Case-insensitive comparison
            auto equalsIgnoreCase = [](std::string_view a, std::string_view b) {
                if (a.size() != b.size()) return false;
                for (size_t i = 0; i < a.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(a[i])) !=
                        std::tolower(static_cast<unsigned char>(b[i])))
                        return false;
                }
                return true;
            };

            if (equalsIgnoreCase(name, "Background"))  return RenderQueue::BACKGROUND;
            if (equalsIgnoreCase(name, "Geometry"))    return RenderQueue::GEOMETRY;
            if (equalsIgnoreCase(name, "AlphaTest"))   return RenderQueue::ALPHATEST;
            if (equalsIgnoreCase(name, "Transparent")) return RenderQueue::TRANSPARENT;
            if (equalsIgnoreCase(name, "Overlay"))     return RenderQueue::OVERLAY;

		    SPD_WARNING("Unknown render queue name '" << name << "', defaulting to GEOMETRY");
            return RenderQueue::GEOMETRY;
        }
    }

    //void Material::SetUniformMat4Array(const std::string& name, const std::vector<Mat4>& values) {
    //    m_Mat4ArrayUniforms[name] = values;
    //}

    Material::Material(std::shared_ptr<IPipeline> pipeline)
        : m_Pipeline(std::move(pipeline)) {}

    Material::~Material() {}

    void Material::SetUniformInt(const std::string& uName, int value) {
        m_IntUniforms[uName] = value;
    }

    void Material::SetUniformFloat(const std::string& uName, float value) {
        m_FloatUniforms[uName] = value;
    }

    void Material::SetUniformVec2(const std::string& uName, const Vec2& value) {
        m_Vec2Uniforms[uName] = value;
    }

    void Material::SetUniformVec3(const std::string& uName, const Vec3& value) {
        m_Vec3Uniforms[uName] = value;
    }

    void Material::SetUniformVec4(const std::string& uName, const Vec4& value) {
        m_Vec4Uniforms[uName] = value;
    }

    void Material::SetUniformMat4(const std::string& uName, const Mat4& value) {
        m_Mat4Uniforms[uName] = value;
    }

    void Material::SetTexture(const std::string& uName, const std::string& uuid) {
        if (uuid == "") {
            m_Textures[uName] = nullptr;
            return;
        }

        m_Textures[uName] = Resource::ResourceManager::GetInstance().
            LoadResource<NE::Graphics::OpenGL::GLTexture>(uuid);
        m_Textures[uName]->MakeResident();
        //m_Textures[uName] = Resource::ResourceManager::GetInstance().
        //    LoadResource<NE::Graphics::OpenGL::GLTexture>(uuid);
        //if (uuid != "")
        //    m_Textures[uName]->MakeResident();
    }

    void Material::SetUniformHandle(const std::string& uName, uint64_t handle) {
        m_HandleUniforms[uName] = handle;
    }

    void Material::SetQueueBase(RenderQueue queue) {
		m_BaseRQ = queue;
    }

	void Material::SetQueueOffset(int32_t offset) {
		m_OffsetRQ = offset;
	}

    void Material::Bind() const {
        auto* shader = m_Pipeline->GetSpecification().shader.get();

        for (const auto& [uName, val] : m_FloatUniforms)
            shader->SetUniformFloat(uName, val);
        for (const auto& [uName, val] : m_Vec2Uniforms)
            shader->SetUniformVec2(uName, val);
        for (const auto& [uName, val] : m_Vec3Uniforms)
            shader->SetUniformVec3(uName, val);
        for (const auto& [uName, val] : m_Vec4Uniforms)
            shader->SetUniformVec4(uName, val);
        for (const auto& [uName, val] : m_Mat4Uniforms)
            shader->SetUniformMat4(uName, val);
        for (const auto& [uName, val] : m_IntUniforms)
            shader->SetUniformInt(uName, val);

        //for (const auto& [name, mats] : m_Mat4ArrayUniforms) {
        //    shader->SetUniformMat4Array(name, mats.data(), static_cast<int>(mats.size()));
        //}

        // Bind bindless texture handles from m_HandleUniforms
        for (const auto& [uName, handle] : m_HandleUniforms) {
            shader->SetUniformHandle(uName, handle);
        }

        // Bind bindless texture handles from m_Textures
        for (auto& [uName, tex] : m_Textures) {
            if (!tex) continue;
            uint64_t h = tex->GetBindlessHandle();
            shader->SetUniformHandle(uName, h);
        }

    }

    void Material::SetShader(const std::string& shaderUUID) {
        auto shader = Resource::ResourceManager::GetInstance()
            .LoadResource<OpenGL::GLShader>(shaderUUID);

        auto spec = m_Pipeline->GetSpecification();
        spec.shader = shader;
        spec.shaderName = shaderUUID;
        m_Pipeline = Graphics::GetPipelineCache().GetOrCreate(spec);

        const auto oldInts = m_IntUniforms;
        const auto oldFloats = m_FloatUniforms;
        const auto oldVec2s = m_Vec2Uniforms;
        const auto oldVec3s = m_Vec3Uniforms;
        const auto oldVec4s = m_Vec4Uniforms;
        const auto oldMat4s = m_Mat4Uniforms;
        const auto oldHandles = m_HandleUniforms;
        const auto oldTex = m_Textures;

        m_IntUniforms.clear();
        m_FloatUniforms.clear();
        m_Vec2Uniforms.clear();
        m_Vec3Uniforms.clear();
        m_Vec4Uniforms.clear();
        m_Mat4Uniforms.clear();
        m_HandleUniforms.clear();
        m_Textures.clear();

        auto TryRestore = [&](const OpenGL::UniformDesc& u) {
            const std::string& name = u.name;

            if (IsSampler(u.type)) {
                if (auto it = oldTex.find(name); it != oldTex.end()) {
                    m_Textures[name] = it->second;
                }

                if (name.rfind("u_", 0) == 0) {
                    std::string hasName = "h_Has" + name.substr(2);
                    if (auto it = oldInts.find(hasName); it != oldInts.end())
                        m_IntUniforms[hasName] = it->second;
                    else
                        m_IntUniforms[hasName] = (m_Textures[name] != nullptr) ? 1 : 0;
                }
                return;
            }

            switch (u.type) {
            case GL_INT:
            case GL_BOOL:
                if (auto it = oldInts.find(name); it != oldInts.end())
                    m_IntUniforms[name] = it->second;
                break;

            case GL_FLOAT:
                if (auto it = oldFloats.find(name); it != oldFloats.end())
                    m_FloatUniforms[name] = it->second;
                break;

            case GL_FLOAT_VEC2:
                if (auto it = oldVec2s.find(name); it != oldVec2s.end())
                    m_Vec2Uniforms[name] = it->second;
                break;

            case GL_FLOAT_VEC3:
                if (auto it = oldVec3s.find(name); it != oldVec3s.end())
                    m_Vec3Uniforms[name] = it->second;
                break;

            case GL_FLOAT_VEC4:
                if (auto it = oldVec4s.find(name); it != oldVec4s.end())
                    m_Vec4Uniforms[name] = it->second;
                break;

            case GL_FLOAT_MAT4:
                if (auto it = oldMat4s.find(name); it != oldMat4s.end())
                    m_Mat4Uniforms[name] = it->second;
                break;

            default:
                break;
            }
            };

        for (auto& u : shader->EnumerateActiveUniforms()) {
            if (IsEngineUniform(u.name)) continue;

            if (IsSampler(u.type)) {
                m_Textures.emplace(u.name, nullptr);
                if (u.name.rfind("u_", 0) == 0) {
                    std::string hasName = "h_Has" + u.name.substr(2);
                    m_IntUniforms.emplace(hasName, 0);
                }

                TryRestore(u);
                continue;
            }

            switch (u.type) {
            case GL_INT:
            case GL_BOOL:
                m_IntUniforms.emplace(u.name, 0);
                break;

            case GL_FLOAT:
                if (u.name.find("Color") != std::string::npos ||
                    u.name.find("color") != std::string::npos ||
                    u.name.find("albedo") != std::string::npos) {
                    m_FloatUniforms.emplace(u.name, 1.0f);
                } else if (u.name.find("AlphaCutoff") != std::string::npos ||
                    u.name.find("Alphacutoff") != std::string::npos ||
                    u.name.find("alphacutoff") != std::string::npos) {
                    m_FloatUniforms.emplace(u.name, 0.5f);
                } else if (u.name.find("Opacity") != std::string::npos ||
                    u.name.find("opacity") != std::string::npos) {
                    m_FloatUniforms.emplace(u.name, 1.0f);
                } else {
                    m_FloatUniforms.emplace(u.name, 0.0f);
                }
                break;

            case GL_FLOAT_VEC2:
                m_Vec2Uniforms.emplace(u.name, Vec2{ 0,0 });
                break;

            case GL_FLOAT_VEC3:
                if (u.name.find("Color") != std::string::npos ||
                    u.name.find("color") != std::string::npos ||
                    u.name.find("albedo") != std::string::npos ||
                    u.name.find("Tiling") != std::string::npos ||
                    u.name.find("tiling") != std::string::npos) {
                    m_Vec3Uniforms.emplace(u.name, Vec3{ 1,1,1 });
                } else {
                    m_Vec3Uniforms.emplace(u.name, Vec3{ 0,0,0 });
                }
                break;

            case GL_FLOAT_MAT4:
                m_Mat4Uniforms.emplace(u.name, Mat4{});
                break;

            default:
                break;
            }

            TryRestore(u);
        }
    }

    bool Material::Preload(Resource::BinaryView blob) {
        if (blob.size < sizeof(NE::Resource::NanoMatHeader)) return false;
        const auto* h = blob.as<NE::Resource::NanoMatHeader>(0);
        if (!h || h->magic != NE::Resource::NMAT_MAGIC || h->version != NE::Resource::CURRENT_NANOMAT_FORMAT_VERSION) return false;

        if (h->shaderNameOffset + h->shaderNameLen > blob.size) return false;
        const char* s0 = reinterpret_cast<const char*>(blob.data + h->shaderNameOffset);
        m_stage.shaderName.assign(s0, s0 + h->shaderNameLen);

        m_stage.depthTest = (h->depthTest != 0);
        m_stage.blend = (h->blendMode != 0);
        m_stage.cullMode = h->cullMode;
        m_stage.polygonMode = h->polygonMode;

        m_stage.rqOffset = h->renderQueueOffset;
        if (h->renderQueueNameOffset != 0 && h->renderQueueNameLen != 0) {
            if (size_t(h->renderQueueNameOffset) + h->renderQueueNameLen > blob.size) return false;

            const char* rq0 = reinterpret_cast<const char*>(blob.data + h->renderQueueNameOffset);
            std::string_view rqView(rq0, rq0 + h->renderQueueNameLen);
            m_stage.rqBase = ParseRenderQueue(rqView);
        }
        else {
            // Backward compat default (or if not authored)
            m_stage.rqBase = RenderQueue::GEOMETRY;
        }

        const size_t recTableSize = size_t(h->propCount) * sizeof(NE::Resource::MatPropRecord);
        if (h->propsOffset + recTableSize > blob.size) return false;

        const auto* recs = blob.as<NE::Resource::MatPropRecord>(h->propsOffset);
        m_stage.props.clear();
        m_stage.props.reserve(h->propCount);

        for (uint16_t i = 0; i < h->propCount; ++i) {
            const auto& r = recs[i];

            if (size_t(r.nameOffset) + r.nameLen > blob.size) return false;
            if (size_t(r.dataOffset) + r.dataSize > blob.size) return false;

            const char* n0 = reinterpret_cast<const char*>(blob.data + r.nameOffset);
            MatStage::Prop p{};
            p.name.assign(n0, n0 + r.nameLen);
            p.type = r.type;

            const uint8_t* d0 = blob.data + r.dataOffset;
            p.bytes.assign(d0, d0 + r.dataSize);

            m_stage.props.push_back(std::move(p));
        }

        if (h->texCount > 0) {
            const size_t texTableSize = size_t(h->texCount) * sizeof(NE::Resource::MatTexRecord);
            if (h->texTableOffset + texTableSize > blob.size) return false;

            const auto* texRecs = blob.as<NE::Resource::MatTexRecord>(h->texTableOffset);
            for (uint16_t i = 0; i < h->texCount; ++i) {
                const auto& tr = texRecs[i];

                if (size_t(tr.nameOffset) + tr.nameLen > blob.size) return false;
                const char* n0 = reinterpret_cast<const char*>(blob.data + tr.nameOffset);

                MatStage::Prop p{};
                p.name.assign(n0, n0 + tr.nameLen);
                p.type = NE::Resource::MatPropType::HANDLE;
                p.bytes.assign(reinterpret_cast<const uint8_t*>(tr.uuid),
                    reinterpret_cast<const uint8_t*>(tr.uuid) + 36);
                m_stage.props.push_back(std::move(p));
            }
        }

        m_stage.has = true;
        return true;
    }

    void Material::Finalize() {
        if (!m_stage.has) return;

        PipelineSpecification spec{};
        spec.shaderName = m_stage.shaderName;
        spec.shader = Resource::ResourceManager::GetInstance().
            LoadResource<Graphics::OpenGL::GLShader>(spec.shaderName);
        spec.EnableDepthTest = m_stage.depthTest;
        spec.EnableBlending = m_stage.blend;
        spec.CullMode = m_stage.cullMode;
        spec.PolygonMode = m_stage.polygonMode;

        m_Pipeline = NE::Graphics::GetPipelineCache().GetOrCreate(spec);

		m_BaseRQ = m_stage.rqBase;
		m_OffsetRQ = m_stage.rqOffset;

        for (const auto& p : m_stage.props) {
            switch (p.type) {
            case NE::Resource::MatPropType::INT: {
                int v = 0;
                if (p.bytes.size() >= sizeof(int))
                    std::memcpy(&v, p.bytes.data(), sizeof(int));
                SetUniformInt(p.name, v);
            } break;
            case NE::Resource::MatPropType::FLOAT: {
                float f = 0.f;
                if (p.bytes.size() >= sizeof(float))
                    std::memcpy(&f, p.bytes.data(), sizeof(float));
                SetUniformFloat(p.name, f);
            } break;
            case NE::Resource::MatPropType::VEC3: {
                if (p.bytes.size() >= 3 * sizeof(float)) {
                    const float* f = reinterpret_cast<const float*>(p.bytes.data());
                    SetUniformVec3(p.name, Vec3{ f[0], f[1], f[2] });
                }
            } break;
            case NE::Resource::MatPropType::MAT4: {
            //    if (p.bytes.size() >= 16 * sizeof(float)) {
            //        const float* m = reinterpret_cast<const float*>(p.bytes.data());
            //        Mat4 M{}; // fill according to your Mat4 storage
            //        for (int i = 0; i < 16; ++i) M.data[i] = m[i];
            //        SetUniformMat4(p.name, M);
            //    }
            } break;
            case NE::Resource::MatPropType::HANDLE: {
                if (p.bytes.size() == 36) {
                    // check empty
                    bool allZeroOrNull = true;
                    for (uint8_t b : p.bytes) {
                        if (b != 0 && b != '\0') { allZeroOrNull = false; break; }
                    }

                    if (!allZeroOrNull) {
                        std::string uuid(reinterpret_cast<const char*>(p.bytes.data()), 36);
                        SetTexture(p.name, uuid);

                        if (p.name.rfind("u_", 0) == 0) {
                            auto hasName = "h_Has" + p.name.substr(2);
                            m_IntUniforms[hasName] = 1;
                        }
                    } else {
                        m_Textures[p.name] = nullptr;
                    }
                }
            } break;
            default: break;
            }
        }
        m_stage = {};
    }

    void Material::ApplyPipelineSpec(const PipelineSpecification& requested) {
        PipelineSpecification spec = requested;

        if (!spec.shader && !spec.shaderName.empty()) {
            spec.shader = Resource::ResourceManager::GetInstance()
                .LoadResource<Graphics::OpenGL::GLShader>(spec.shaderName);
        }
        m_Pipeline = NE::Graphics::GetPipelineCache().GetOrCreate(spec);
    }
}
