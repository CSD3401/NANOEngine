#pragma once

namespace NANOEngine::Graphics {

    enum class PipelineType {
        Graphics,
        Compute
    };

    struct GraphicsPipelineDesc {
        // need to expand for vulkan
        const char* vertexShaderPath;
        const char* fragmentShaderPath;
    };

    class IPipeline {
    public:
        virtual ~IPipeline() = default;
        virtual void Bind() = 0;
    };

}