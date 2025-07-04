#pragma once

#include "../Interfaces/IPipeline.hpp"

namespace NANOEngine::Graphics::OpenGL {

    class GLPipeline final : public IPipeline {
    public:
        GLPipeline(const PipelineSpecification& spec);
        ~GLPipeline() = default;

        void Bind() const override;
        const PipelineSpecification& GetSpecification() const override { return m_Spec; }

    private:
        PipelineSpecification m_Spec;
    };

}
