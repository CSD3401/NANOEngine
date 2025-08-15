#include "Material.hpp"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <fstream>
#include "../../AssetManager.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../OpenGL/GLPipeline.hpp"

namespace {
    using namespace NE::Graphics;
    std::unordered_map<std::string, std::shared_ptr<IPipeline>> s_PipelineRegistry;
}

namespace NE::Graphics {

    void RegisterPipeline(std::shared_ptr<IPipeline> pipeline) {
        if (pipeline)
            s_PipelineRegistry[pipeline->GetName()] = std::move(pipeline);
    }

    std::shared_ptr<IPipeline> GetPipelineByName(const std::string& name) {
        auto it = s_PipelineRegistry.find(name);
        if (it != s_PipelineRegistry.end())
            return it->second;
        return nullptr;
    }

    Material::Material(std::shared_ptr<IPipeline> pipeline)
        : m_Pipeline(std::move(pipeline)) {}

    Material::~Material()
    {
        SaveMaterial(filePath);
    }

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
    }

    void Material::Bind() const {
        m_Pipeline->Bind();

        auto* shader = m_Pipeline->GetSpecification().shader.get();
        for (const auto& [uName, val] : m_FloatUniforms)
            shader->SetUniformFloat(uName, val);
        for (const auto& [uName, val] : m_Vec3Uniforms)
            shader->SetUniformVec3(uName, val);
        for (const auto& [uName, val] : m_Mat4Uniforms)
            shader->SetUniformMat4(uName, val);
        for (const auto& [uName, val] : m_IntUniforms)
            shader->SetUniformInt(uName, val);

        // Bind textures (optional): assumes ITexture has Bind(slot)
        int slot = 0;
        for (const auto& [uName, tex] : m_Textures) {
            //tex->Bind(slot);
            shader->SetUniformInt(uName, slot);
            ++slot;
        }
    }

    // Fix for AddMember issue in SaveMaterial method
    void Material::SaveMaterial(const std::string& path) const {
        using namespace rapidjson;
        Document doc;
        doc.SetObject();
        Document::AllocatorType& alloc = doc.GetAllocator();

        // Shader and pipeline properties
        if (m_Pipeline) {
            doc.AddMember("Shader", Value(m_Pipeline->GetShaderUUID().data(), alloc).Move(), alloc);
            auto spec = m_Pipeline->GetSpecification();
            doc.AddMember("DepthTest", spec.EnableDepthTest, alloc);
            doc.AddMember("BlendMode", spec.EnableBlending, alloc);
            doc.AddMember("CullMode", spec.CullMode, alloc);
            doc.AddMember("PolygonMode", spec.PolygonMode, alloc);
        }

        // Uniforms/Properties
        Value uniforms(kObjectType);

        // Int uniforms
        for (const auto& [name, value] : m_IntUniforms) {
            uniforms.AddMember(Value(name.c_str(), alloc).Move(), Value(value).Move(), alloc);
        }
        // Float uniforms
        for (const auto& [name, value] : m_FloatUniforms) {
            uniforms.AddMember(Value(name.c_str(), alloc).Move(), Value(value).Move(), alloc);
        }
        // Vec3 uniforms
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
        // for (const auto& [name, tex] : m_Textures) {
        //     std::string uuid = tex ? tex->GetUUID() : "";
        //     uniforms.AddMember(Value(name.c_str(), alloc).Move(), Value(uuid.c_str(), alloc).Move(), alloc);
        // }

        doc.AddMember("Properties", uniforms, alloc); // or "Uniforms" if that's your file's convention

        // Write to file
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::ofstream out(path);
        if (out.is_open())
            out << buffer.GetString();
    }

    bool Material::LoadFromFile(const std::string& path) {
        using namespace rapidjson;

        std::ifstream in(path);
        if (!in.is_open())
            throw std::runtime_error("Failed to open material file: " + path);

        std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        Document doc;
        doc.Parse(json.c_str());

        if (!doc.IsObject())
            throw std::runtime_error("Invalid material file");

        std::string shaderUUID = doc["Shader"].GetString();

        auto shader = Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLShader>(shaderUUID);
        auto depthTest = doc["DepthTest"].GetBool();
        auto blendMode = doc["BlendMode"].GetBool();
		auto cullMode = doc["CullMode"].GetInt();
		auto polygonMode = doc["PolygonMode"].GetInt();

        Graphics::PipelineSpecification pipelineSpec;
        pipelineSpec.shader = shader;
        pipelineSpec.EnableDepthTest = depthTest;
		pipelineSpec.EnableBlending = blendMode;
        pipelineSpec.CullMode = cullMode;
        pipelineSpec.PolygonMode = polygonMode;
        m_Pipeline = std::make_shared<Graphics::OpenGL::GLPipeline>(pipelineSpec);

        if (doc.HasMember("Properties") && doc["Properties"].IsObject()) {
            const auto& props = doc["Properties"];
            for (auto it = props.MemberBegin(); it != props.MemberEnd(); ++it) {
                std::string uName = it->name.GetString();
                const auto& value = it->value;

                if (value.IsNumber()) {
                    SetUniformFloat(uName, value.GetFloat());
                } else if (value.IsArray()) {
                    if (value.Size() == 3) {
                        Vec3 v{ value[0].GetFloat(), value[1].GetFloat(), value[2].GetFloat() };
                        SetUniformVec3(uName, v);
                    } else if (value.Size() == 16) {
                        Mat4 m{};
                        for (SizeType i = 0; i < 16; ++i)
                            m[i] = value[i].GetFloat();
                        SetUniformMat4(uName, m);
                    }
                } else if (value.IsString()) {
                    // Texture loading not implemented
                    // mat->SetTexture(name, LoadTexture(value.GetString()));
                }
            }
        }

        return true;
    }
}
