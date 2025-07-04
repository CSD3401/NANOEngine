#include "Material.hpp"

namespace NANOEngine::Graphics {

    Material::Material(std::shared_ptr<IPipeline> pipeline)
        : m_Pipeline(std::move(pipeline)) {}

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

        // Bind textures (optional): assumes ITexture has Bind(slot)
        int slot = 0;
        for (const auto& [name, tex] : m_Textures) {
            //tex->Bind(slot);
            shader->SetUniformInt(name, slot);
            ++slot;
        }
    }

}
