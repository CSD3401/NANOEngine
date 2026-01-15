#ifndef NANOENGINE_GRAPHICS_MATERIAL_HPP
#define NANOENGINE_GRAPHICS_MATERIAL_HPP

#include <memory>
#include <unordered_map>
#include <string>
#include <algorithm>
#include "../Interfaces/IPipeline.hpp"
#include "../OpenGL/GLTexture.hpp"
#include "../../../src/Math/Vec3.hpp"
#include "../../../src/Math/Mat4.hpp"
#include "../../NANOEngineAPI.hpp"
#include "ResourceManagement/IResource.hpp"
#include "RenderQueue.hpp"
#include "PipelineData.hpp"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace NE::Graphics {

	class NANOENGINE_API Material : public Resource::IResource {
	public:
        Material(std::shared_ptr<IPipeline> pipeline);
		Material() = default;
        ~Material() override;

		void SetPipeline(std::shared_ptr<IPipeline> pipeline) { m_Pipeline = std::move(pipeline); }

        void SetUniformInt(const std::string& name, int value);
        void SetUniformFloat(const std::string& name, float value);
        void SetUniformVec3(const std::string& name, const Vec3& value);
        void SetUniformMat4(const std::string& name, const Mat4& value);
        void SetTexture(const std::string& name, const std::string& uuid);
        void SetQueueBase(RenderQueue queue);
        void SetQueueOffset(int32_t offset);

        void Bind() const;

        //void SaveMaterial(const std::string& path) const;
        void SetShader(const std::string& shaderUUID);
        bool Preload(Resource::BinaryView blob) override;
        void Finalize() override;

        void ApplyPipelineSpec(const PipelineSpecification& requested);

        std::shared_ptr<IPipeline> GetPipeline() const { return m_Pipeline; }

        //const std::unordered_map<std::string, int>& GetIntUniforms() const { return m_IntUniforms; }
        //const std::unordered_map<std::string, float>& GetFloatUniforms() const { return m_FloatUniforms; }
        std::unordered_map<std::string, int>& GetIntUniforms() { return m_IntUniforms; }
        std::unordered_map<std::string, float>& GetFloatUniforms() { return m_FloatUniforms; }
        const std::unordered_map<std::string, Vec3>& GetVec3Uniforms() const { return m_Vec3Uniforms; }
        const std::unordered_map<std::string, Mat4>& GetMat4Uniforms() const { return m_Mat4Uniforms; }
        const RenderQueue& GetQueueBase() const { return m_BaseRQ; }
		const int32_t& GetQueueOffset() const { return m_OffsetRQ; }
        const uint32_t GetQueueOrder() const { return static_cast<uint32_t>(std::max<int64_t>(static_cast<int64_t>(m_BaseRQ) + m_OffsetRQ, 0)); }
        const std::unordered_map<std::string, std::shared_ptr<OpenGL::GLTexture>>& GetTextures() const { return m_Textures; }

        static constexpr Resource::ResourceType GetStaticType() { return Resource::ResourceType::Material; }
        Resource::ResourceType GetType() const override { return GetStaticType(); }


        //void SetUniformMat4Array(const std::string& name, const std::vector<NE::Math::Mat4>& values); // warning: definition not found

    public:
        std::shared_ptr<IPipeline> m_Pipeline;

        std::unordered_map<std::string, int>  m_IntUniforms;
        std::unordered_map<std::string, float> m_FloatUniforms;
        std::unordered_map<std::string, Vec3> m_Vec3Uniforms;
        std::unordered_map<std::string, Mat4> m_Mat4Uniforms;

        std::unordered_map<std::string, std::shared_ptr<OpenGL::GLTexture>> m_Textures;

        // Render queue
        RenderQueue m_BaseRQ = RenderQueue::GEOMETRY;
        int32_t m_OffsetRQ = 0;

    private:

        struct MatStage {
            std::string shaderName;
            bool  depthTest = true;
            bool  blend = false;
            uint32_t cullMode = 0;
            uint32_t polygonMode = 0;

			NE::Graphics::RenderQueue rqBase = NE::Graphics::RenderQueue::GEOMETRY;
			int32_t rqOffset = 0;

            struct Prop {
                std::string name;
                uint8_t type = 0;     // see NanoMatHeader for type ref
                std::vector<uint8_t> bytes; // raw payload
            };
            std::vector<Prop> props;
            bool has = false; // parsed ok
        } m_stage;

        
    };

}

#pragma warning(pop)

#endif // !NANOENGINE_GRAPHICS_MATERIAL_HPP
