#pragma once
#include "../Interfaces/IPipeline.hpp"

namespace NANOEngine::Graphics::OpenGL {

    class GLPipeline final : public IPipeline {
    public:
		GLPipeline(const PipelineSpecification& spec, std::string name); // deprecated, use the one without name
        GLPipeline(const PipelineSpecification& spec);
        ~GLPipeline() = default;

        void Bind() const override;
        const PipelineSpecification& GetSpecification() const override { return m_Spec; }

        const std::string& GetName() const override { return m_name; }
        const std::string_view GetShaderUUID() const override;

    private:
		std::string m_name; // to be removed later  
        PipelineSpecification m_Spec;
    };

}
