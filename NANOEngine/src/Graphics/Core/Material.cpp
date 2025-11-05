#include "Material.hpp"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <fstream>
//#include "../../AssetManager.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../OpenGL/GLPipeline.hpp"
#include "../OpenGL/GLTexture.hpp"
#include "PipelineCache.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include <glad/glad.h>
#include <vector>
namespace {
    static bool IsEngineUniform(std::string_view n) {
        return n == "u_Model" || n == "u_View" || n == "u_Projection" ||
            n == "u_NormalMatrix" || n == "u_CameraPos" ||
            n == "u_numLights" || n.rfind("u_lights", 0) == 0 ||
            n == "u_ShadingModel" || n.rfind("u_Has", 0) == 0;
    }

    static bool IsSampler(GLenum t) {
        switch (t) {
        case GL_SAMPLER_2D: case GL_SAMPLER_2D_ARRAY: case GL_SAMPLER_CUBE:
        case GL_INT_SAMPLER_2D: case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_SAMPLER_2D_SHADOW: case GL_SAMPLER_CUBE_SHADOW:
            return true;
        default: return false;
        }
    }
}

#include <iostream>
#include "ResourceManagement/BinaryHeaders/NanoMatHeader.hpp"

namespace NE::Graphics {

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

    void Material::SetUniformVec3(const std::string& uName, const Vec3& value) {
        m_Vec3Uniforms[uName] = value;
    }

    void Material::SetUniformMat4(const std::string& uName, const Mat4& value) {
        m_Mat4Uniforms[uName] = value;
    }

    void Material::SetTexture(const std::string& uName, std::shared_ptr<ITexture> texture) {
        m_Textures[uName] = std::move(texture);
        m_Textures[uName]->MakeResident();
    }

    void Material::SetQueueBase(RenderQueue queue) {
		m_BaseRQ = queue;
    }

	void Material::SetQueueOffset(uint16_t offset) {
		m_OffsetRQ = offset;
	}

    void Material::Bind() const {
        //m_Pipeline->Bind();

        auto* shader = m_Pipeline->GetSpecification().shader.get();
        for (const auto& [uName, val] : m_FloatUniforms)
            shader->SetUniformFloat(uName, val);
        for (const auto& [uName, val] : m_Vec3Uniforms)
            shader->SetUniformVec3(uName, val);
        for (const auto& [uName, val] : m_Mat4Uniforms)
            shader->SetUniformMat4(uName, val);
        for (const auto& [uName, val] : m_IntUniforms)
            shader->SetUniformInt(uName, val);

        //for (const auto& [name, mats] : m_Mat4ArrayUniforms) {
        //    shader->SetUniformMat4Array(name, mats.data(), static_cast<int>(mats.size()));
        //}

        for (auto& [uName, tex] : m_Textures) {
            if (!tex) continue;
            //tex->MakeResident();
            uint64_t h = tex->GetBindlessHandle();
            shader->SetUniformHandle(uName, h);
        }

    }

    // Fix for AddMember issue in SaveMaterial method
    void Material::SaveMaterial(const std::string& filePath) const {
        using namespace rapidjson;
        Document doc;
        doc.SetObject();
        Document::AllocatorType& alloc = doc.GetAllocator();

        if (m_Pipeline) {
            doc.AddMember("Shader", Value(m_Pipeline->GetSpecification().shaderName.data(), alloc).Move(), alloc);
            auto spec = m_Pipeline->GetSpecification();
            doc.AddMember("DepthTest", spec.EnableDepthTest, alloc);
            doc.AddMember("BlendMode", spec.EnableBlending, alloc);
            doc.AddMember("CullMode", spec.CullMode, alloc);
            doc.AddMember("PolygonMode", spec.PolygonMode, alloc);
        }

        Value uniforms(kObjectType);

        for (const auto& [name, value] : m_IntUniforms) {
            uniforms.AddMember(Value(name.c_str(), alloc).Move(), Value(value).Move(), alloc);
        }
        for (const auto& [name, value] : m_FloatUniforms) {
            uniforms.AddMember(Value(name.c_str(), alloc).Move(), Value(value).Move(), alloc);
        }
        for (const auto& [name, value] : m_Vec3Uniforms) {
            Value arr(kArrayType);
            arr.PushBack(value.x, alloc).PushBack(value.y, alloc).PushBack(value.z, alloc);
            uniforms.AddMember(Value(name.c_str(), alloc).Move(), arr, alloc);
        }
        // Mat4 uniforms (if needed)
        // for (const auto& [name, value] : m_Mat4Uniforms) {
        //     Value arr(kArrayType);
        //     for (int i = 0; i < 16; ++i) arr.PushBack(value.data[i], alloc); // adjust based on your Mat4 storage
        //     uniforms.AddMember(Value(name.c_str(), alloc).Move(), arr, alloc);
        // }
        // Textures (TBD SOON)
        for (const auto& [name, tex] : m_Textures) {
            std::string _uuid = tex ? tex->uuid : "";
            uniforms.AddMember(Value(name.c_str(), alloc).Move(), Value(_uuid.c_str(), alloc).Move(), alloc);
        }

        doc.AddMember("Properties", uniforms, alloc);

        // Write to file
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::ofstream out(filePath);
        if (out.is_open())
            out << buffer.GetString();
    }

    //bool Material::LoadFromFile(const std::string& /*path*/) {
  //      using namespace rapidjson;

  //      std::ifstream in(path);
  //      if (!in.is_open())
  //          throw std::runtime_error("Failed to open material file: " + path);

  //      std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  //      Document doc;
  //      doc.Parse(json.c_str());

  //      if (!doc.IsObject())
  //          throw std::runtime_error("Invalid material file");

  //      std::string shaderUUID = doc["Shader"].GetString();

  //      if (shaderUUID.empty()) shaderUUID = "Basic";
		//	//throw std::runtime_error("Material file missing shader UUID");
  //      auto shader = Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLShader>(shaderUUID);
  //      auto depthTest = doc["DepthTest"].GetBool();
  //      auto blendMode = doc["BlendMode"].GetBool();
		//auto cullMode = doc["CullMode"].GetInt();
		//auto polygonMode = doc["PolygonMode"].GetInt();

  //      Graphics::PipelineSpecification pipelineSpec;
  //      pipelineSpec.shader = shader;
  //      pipelineSpec.shaderName = shaderUUID;
  //      pipelineSpec.EnableDepthTest = depthTest;
		//pipelineSpec.EnableBlending = blendMode;
  //      pipelineSpec.CullMode = cullMode;
  //      pipelineSpec.PolygonMode = polygonMode;
  //      m_Pipeline = Graphics::GetPipelineCache().GetOrCreate(pipelineSpec);
  //      //m_Pipeline = std::make_shared<Graphics::OpenGL::GLPipeline>(pipelineSpec);

  //      if (doc.HasMember("Properties") && doc["Properties"].IsObject()) {
  //          const auto& props = doc["Properties"];
  //          for (auto it = props.MemberBegin(); it != props.MemberEnd(); ++it) {
  //              std::string uName = it->name.GetString();
  //              const auto& value = it->value;

  //              if (value.IsNumber()) {
  //                  SetUniformFloat(uName, value.GetFloat());
  //              } else if (value.IsArray()) {
  //                  if (value.Size() == 3) {
  //                      Vec3 v{ value[0].GetFloat(), value[1].GetFloat(), value[2].GetFloat() };
  //                      SetUniformVec3(uName, v);
  //                  } else if (value.Size() == 16) {
  //                      Mat4 m{};
  //                      for (SizeType i = 0; i < 16; ++i)
  //                          m[i] = value[i].GetFloat();
  //                      SetUniformMat4(uName, m);
  //                  }
  //              } else if (value.IsString()) {
  //                  // Texture serialization not yet done
  //                  // mat->SetTexture(name, LoadTexture(value.GetString()));
  //              }
  //          }
  //      }

  //      return true;
        //return false;
    //}

    void Material::SetShader(const std::string& shaderUUID) {
        auto shader = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLShader>(shaderUUID);
        auto spec = m_Pipeline->GetSpecification();
        spec.shader = shader;
        spec.shaderName = shaderUUID;
        m_Pipeline = Graphics::GetPipelineCache().GetOrCreate(spec);

        m_IntUniforms.clear();
        m_FloatUniforms.clear();
        m_Vec3Uniforms.clear();
        m_Mat4Uniforms.clear();
        m_Textures.clear();

        //auto defaultWhite = Asset::AssetManager::GetInstance().Load<NE::Graphics::OpenGL::GLTexture>("WhiteTex");

        for (auto& u : shader->EnumerateActiveUniforms()) {
            std::cout << u.name << " type=" << std::hex << u.type << "\n";
            if (IsEngineUniform(u.name)) continue;

            if (IsSampler(u.type)) {
                m_Textures.emplace(u.name, nullptr);

                if (u.name.rfind("u_", 0) == 0) {
                    std::string hasName = "u_Has" + u.name.substr(2); // u_BaseMap -> u_HasBaseMap
                    //m_IntUniforms.emplace(hasName, defaultWhite ? 1 : 0);
                    m_IntUniforms.emplace(hasName, 0);
                }

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
                    u.name.find("albedo") != std::string::npos)
                    m_FloatUniforms.emplace(u.name, 1.0f);
                else
                    m_FloatUniforms.emplace(u.name, 0.0f);
                break;
            case GL_FLOAT_VEC3:
                if (u.name.find("Color") != std::string::npos ||
                    u.name.find("color") != std::string::npos ||
                    u.name.find("albedo") != std::string::npos)
                    m_Vec3Uniforms.emplace(u.name, Vec3{ 1,1,1 });
                else
                    m_Vec3Uniforms.emplace(u.name, Vec3{ 0,0,0 });
                break;
            case GL_FLOAT_MAT4:
                m_Mat4Uniforms.emplace(u.name, Mat4{});
                break;
            default:
                break;
            }
        }
    }

    bool Material::Preload(Resource::BinaryView blob) {
        if (blob.size < sizeof(NE::Resource::NanoMatHeader)) return false;
        const auto* h = blob.as<NE::Resource::NanoMatHeader>(0);
        if (!h || h->magic != NE::Resource::NMAT_MAGIC || h->version != NE::Resource::CURRENT_NANOMAT_FORMAT_VERSION) return false;

        // Bounds for shader name
        if (h->shaderNameOffset + h->shaderNameLen > blob.size) return false;
        const char* s0 = reinterpret_cast<const char*>(blob.data + h->shaderNameOffset);
        m_stage.shaderName.assign(s0, s0 + h->shaderNameLen);

        // Global state
        m_stage.depthTest = (h->depthTest != 0);
        m_stage.blend = (h->blendMode != 0);
        m_stage.cullMode = h->cullMode;
        m_stage.polygonMode = h->polygonMode;

        // Property table bounds
        const size_t recTableSize = size_t(h->propCount) * sizeof(NE::Resource::MatPropRecord);
        if (h->propsOffset + recTableSize > blob.size) return false;

        const auto* recs = blob.as<NE::Resource::MatPropRecord>(h->propsOffset);
        m_stage.props.clear();
        m_stage.props.reserve(h->propCount);

        for (uint16_t i = 0; i < h->propCount; ++i) {
            const auto& r = recs[i];

            // Bounds: name and data
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

        m_stage.has = true;
        return true;
    }

    void Material::Finalize() {
        if (!m_stage.has) return;

        // Build (or fetch) a pipeline from the shader name + state
        PipelineSpecification spec{};

        spec.shaderName = m_stage.shaderName;  // your cache will resolve it
        spec.shader = Resource::ResourceManager::GetInstance().LoadResource<Graphics::OpenGL::GLShader>(spec.shaderName);
        spec.EnableDepthTest = m_stage.depthTest;
        spec.EnableBlending = m_stage.blend;
        spec.CullMode = m_stage.cullMode;
        spec.PolygonMode = m_stage.polygonMode;

        m_Pipeline = NE::Graphics::GetPipelineCache().GetOrCreate(spec);

        // Push properties into the Material (name-based, matches your Bind())
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
            default: break; // future: textures/handles
            }
        }

        // Drop staging to free memory
        m_stage = {};
    }

}
