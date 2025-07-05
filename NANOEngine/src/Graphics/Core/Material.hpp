#ifndef NANOENGINE_GRAPHICS_MATERIAL_HPP
#define NANOENGINE_GRAPHICS_MATERIAL_HPP

#include <memory>
#include <unordered_map>
#include "../Interfaces/IPipeline.hpp"
#include "../Interfaces/ITexture.hpp"
#include "../../../src/Math/Vec3.hpp"
#include "../../../src/Math/Mat4.hpp"

namespace NANOEngine::Graphics {
	class Material {
	public:
        Material(std::shared_ptr<IPipeline> pipeline);

        void SetUniformFloat(const std::string& name, float value);
        void SetUniformVec3(const std::string& name, const Vec3& value);
        void SetUniformMat4(const std::string& name, const Mat4& value);
        void SetTexture(const std::string& name, std::shared_ptr<ITexture> texture);

        void Bind() const;

        std::shared_ptr<IPipeline> GetPipeline() const { return m_Pipeline; }

    private:
        std::shared_ptr<IPipeline> m_Pipeline;

        // Uniforms to be uploaded before draw
        std::unordered_map<std::string, float> m_FloatUniforms;
        std::unordered_map<std::string, Vec3> m_Vec3Uniforms;
        std::unordered_map<std::string, Mat4> m_Mat4Uniforms;

        // Texture units (assume 1 per name for now)
        std::unordered_map<std::string, std::shared_ptr<ITexture>> m_Textures;
	};
}


#endif // !NANOENGINE_GRAPHICS_MATERIAL_HPP
