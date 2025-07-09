#ifndef NANOENGINE_GRAPHICS_MATERIAL_HPP
#define NANOENGINE_GRAPHICS_MATERIAL_HPP

#include <memory>
#include <unordered_map>
#include <string>
#include "../Interfaces/IPipeline.hpp"
#include "../Interfaces/ITexture.hpp"
#include "../../../src/Math/Vec3.hpp"
#include "../../../src/Math/Mat4.hpp"
#include "../../../NANOEngineAPI.hpp"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace NANOEngine::Graphics {
	class NANOENGINE_API Material {
	public:
        Material(std::shared_ptr<IPipeline> pipeline);

        void SetUniformInt(const std::string& name, int value);
        void SetUniformFloat(const std::string& name, float value);
        void SetUniformVec3(const std::string& name, const Vec3& value);
        void SetUniformMat4(const std::string& name, const Mat4& value);
        void SetTexture(const std::string& name, std::shared_ptr<ITexture> texture);

        void Bind() const;

        std::shared_ptr<IPipeline> GetPipeline() const { return m_Pipeline; }

        const std::unordered_map<std::string, int>& GetIntUniforms() const { return m_IntUniforms; }
        const std::unordered_map<std::string, float>& GetFloatUniforms() const { return m_FloatUniforms; }
        const std::unordered_map<std::string, Vec3>& GetVec3Uniforms() const { return m_Vec3Uniforms; }
        const std::unordered_map<std::string, Mat4>& GetMat4Uniforms() const { return m_Mat4Uniforms; }

        void SaveMaterial(const std::string& path) const;
        static std::shared_ptr<Material> LoadMaterial(std::string path);

    private:
        std::shared_ptr<IPipeline> m_Pipeline;

        // Uniforms to be uploaded before draw
        std::unordered_map<std::string, int>  m_IntUniforms;
        std::unordered_map<std::string, float> m_FloatUniforms;
        std::unordered_map<std::string, Vec3> m_Vec3Uniforms;
        std::unordered_map<std::string, Mat4> m_Mat4Uniforms;

        // Texture units (assume 1 per name for now)
        std::unordered_map<std::string, std::shared_ptr<ITexture>> m_Textures;
	};

    void RegisterPipeline(std::shared_ptr<IPipeline> pipeline);
    std::shared_ptr<IPipeline> GetPipelineByName(const std::string& name);
}

#pragma warning(pop)

#endif // !NANOENGINE_GRAPHICS_MATERIAL_HPP
