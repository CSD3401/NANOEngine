#include "Material.hpp"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <fstream>
#include "../../AssetManager.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../OpenGL/GLPipeline.hpp"

namespace {
    using namespace NANOEngine::Graphics;
    std::unordered_map<std::string, std::shared_ptr<IPipeline>> s_PipelineRegistry;
}

namespace NANOEngine::Graphics {

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

    void Material::SaveMaterial(const std::string& path) const {
        //using namespace rapidjson;
        path;
        //Document doc;
        //doc.SetObject();
        //Document::AllocatorType& alloc = doc.GetAllocator();

        //// 1. Shader UUID
        //// Assuming you have a way to retrieve it:
        //// If you only have a shared_ptr to the shader, you should store the UUID when loading/creating the material.
        //doc.AddMember("Shader", Value(shaderUUID.c_str(), alloc), alloc);

        //// 2. Pipeline state
        //doc.AddMember("DepthTest", m_Pipeline->GetSpecification().EnableDepthTest, alloc);
        //doc.AddMember("BlendMode", m_Pipeline->GetSpecification().EnableBlending, alloc);
        //doc.AddMember("CullMode", m_Pipeline->GetSpecification().CullMode, alloc);
        //doc.AddMember("PolygonMode", m_Pipeline->GetSpecification().PolygonMode, alloc);

        //// 3. Properties (uniforms/textures)
        //Value props(kObjectType);
        //for (const auto& prop : m_Properties) {
        //    const std::string& name = prop.first;
        //    const UniformValue& value = prop.second;

        //    // Handle various property types
        //    if (value.type == UniformValue::Type::Float) {
        //        props.AddMember(Value(name.c_str(), alloc), value.f, alloc);
        //    } else if (value.type == UniformValue::Type::Vec3) {
        //        Value arr(kArrayType);
        //        arr.PushBack(value.v[0], alloc).PushBack(value.v[1], alloc).PushBack(value.v[2], alloc);
        //        props.AddMember(Value(name.c_str(), alloc), arr, alloc);
        //    } else if (value.type == UniformValue::Type::Mat4) {
        //        Value arr(kArrayType);
        //        for (int i = 0; i < 16; ++i)
        //            arr.PushBack(value.m[i], alloc);
        //        props.AddMember(Value(name.c_str(), alloc), arr, alloc);
        //    } else if (value.type == UniformValue::Type::Texture) {
        //        // Save the texture's UUID or path as string
        //        props.AddMember(Value(name.c_str(), alloc), Value(value.textureUUID.c_str(), alloc), alloc);
        //    }
        //    // Extend with other property types as needed
        //}
        //doc.AddMember("Properties", props, alloc);

        //// 4. Write to file
        //StringBuffer buffer;
        //PrettyWriter<StringBuffer> writer(buffer);
        //doc.Accept(writer);

        //std::ofstream out(path);
        //if (out.is_open())
        //    out << buffer.GetString();

        //doc.AddMember("Properties", props, alloc);

        //StringBuffer buffer;
        //PrettyWriter<StringBuffer> writer(buffer);
        //doc.Accept(writer);
        //std::ofstream out(path);
        //if (out.is_open())
        //    out << buffer.GetString();
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

        //if (!doc.HasMember("shader") || !doc["shader"].IsString())
        //    throw std::runtime_error("No 'shader' in material file");

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
