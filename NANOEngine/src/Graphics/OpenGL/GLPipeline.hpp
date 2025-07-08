#pragma once
#include "../Interfaces/IPipeline.hpp"

namespace NANOEngine::Graphics::OpenGL {

    class GLPipeline final : public IPipeline {
    public:
        GLPipeline(const PipelineSpecification& spec, std::string name);
        ~GLPipeline() = default;

        void Bind() const override;
        const PipelineSpecification& GetSpecification() const override { return m_Spec; }

        const std::string& GetName() const override { return m_name; }

    private:
        std::string m_name;
        PipelineSpecification m_Spec;
    };

}
