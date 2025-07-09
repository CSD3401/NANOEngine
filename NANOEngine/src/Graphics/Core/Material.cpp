#include "Material.hpp"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <fstream>

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

    void Material::SetUniformInt(const std::string& name, int value) {
        m_IntUniforms[name] = value;
    }

    void Material::SetUniformFloat(const std::string& name, float value) {
        m_FloatUniforms[name] = value;
    }

    void Material::SetUniformVec3(const std::string& name, const Vec3& value) {
        m_Vec3Uniforms[name] = value;
    }

    void Material::SetUniformMat4(const std::string& name, const Mat4& value) {
        m_Mat4Uniforms[name] = value;
    }

    void Material::SetTexture(const std::string& name, std::shared_ptr<ITexture> texture) {
        m_Textures[name] = std::move(texture);
    }

    void Material::Bind() const {
        m_Pipeline->Bind();

        auto* shader = m_Pipeline->GetSpecification().shader.get();
        for (const auto& [name, val] : m_FloatUniforms)
            shader->SetUniformFloat(name, val);
        for (const auto& [name, val] : m_Vec3Uniforms)
            shader->SetUniformVec3(name, val);
        for (const auto& [name, val] : m_Mat4Uniforms)
            shader->SetUniformMat4(name, val);
        for (const auto& [name, val] : m_IntUniforms)
            shader->SetUniformInt(name, val);

        // Bind textures (optional): assumes ITexture has Bind(slot)
        int slot = 0;
        for (const auto& [name, tex] : m_Textures) {
            //tex->Bind(slot);
            shader->SetUniformInt(name, slot);
            ++slot;
        }
    }

    void Material::SaveMaterial(const std::string& path) const {
        using namespace rapidjson;

        // Create JSON document
        Document doc;
        doc.SetObject();

        // 1. Add shader name
        Document::AllocatorType& alloc = doc.GetAllocator();
        std::string shaderName = m_Pipeline->GetName(); // or whatever your pipeline uses
        doc.AddMember("shader", Value(shaderName.c_str(), alloc), alloc);

        // 2. Add properties object
        Value props(kObjectType);

        // Floats
        for (const auto& [name, value] : m_FloatUniforms) {
            props.AddMember(Value(name.c_str(), alloc), Value(value), alloc);
        }
        // Vec3s
        for (const auto& [name, v] : m_Vec3Uniforms) {
            Value arr(kArrayType);
            arr.PushBack(v.x, alloc).PushBack(v.y, alloc).PushBack(v.z, alloc);
            props.AddMember(Value(name.c_str(), alloc), arr, alloc);
        }
        // Mat4s
        for (const auto& [name, m] : m_Mat4Uniforms) {
            Value arr(kArrayType);
            for (int i = 0; i < 16; ++i) arr.PushBack(m[i], alloc);
            props.AddMember(Value(name.c_str(), alloc), arr, alloc);
        }
        // Textures (just save their path or ID)
        for (const auto& [name, tex] : m_Textures) {
            // This assumes you can get the texture path from the texture object
            //std::string texPath = tex->GetPath(); // adjust if needed
            //props.AddMember(Value(name.c_str(), alloc), Value(texPath.c_str(), alloc), alloc);
        }

        doc.AddMember("Properties", props, alloc);

        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        doc.Accept(writer);
        std::ofstream out(path);
        if (out.is_open())
            out << buffer.GetString();
    }

    std::shared_ptr<Material> Material::LoadMaterial(std::string path) {
        using namespace rapidjson;

        std::ifstream in(path);
        if (!in.is_open())
            throw std::runtime_error("Failed to open material file: " + path);

        std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        Document doc;
        doc.Parse(json.c_str());

        if (!doc.IsObject())
            throw std::runtime_error("Invalid material file");

        if (!doc.HasMember("shader") || !doc["shader"].IsString())
            throw std::runtime_error("No 'shader' in material file");

        std::string shaderName = doc["shader"].GetString();
        auto pipeline = GetPipelineByName(shaderName);
        if (!pipeline)
            throw std::runtime_error("Pipeline not found: " + shaderName);

        auto mat = std::make_shared<Material>(pipeline);

        if (doc.HasMember("Properties") && doc["Properties"].IsObject()) {
            const auto& props = doc["Properties"];
            for (auto it = props.MemberBegin(); it != props.MemberEnd(); ++it) {
                std::string name = it->name.GetString();
                const auto& value = it->value;

                if (value.IsNumber()) {
                    mat->SetUniformFloat(name, value.GetFloat());
                } else if (value.IsArray()) {
                    if (value.Size() == 3) {
                        Vec3 v{ value[0].GetFloat(), value[1].GetFloat(), value[2].GetFloat() };
                        mat->SetUniformVec3(name, v);
                    } else if (value.Size() == 16) {
                        Mat4 m{};
                        for (SizeType i = 0; i < 16; ++i)
                            m[i] = value[i].GetFloat();
                        mat->SetUniformMat4(name, m);
                    }
                } else if (value.IsString()) {
                    // Texture loading not implemented
                    // mat->SetTexture(name, LoadTexture(value.GetString()));
                }
            }
        }

        return mat;
    }
}
