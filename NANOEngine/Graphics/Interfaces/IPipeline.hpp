#ifndef NANOENGINE_GRAPHICS_IPIPELINE_HPP
#define NANOENGINE_GRAPHICS_IPIPELINE_HPP

#include <memory>
#include "IShader.hpp"

namespace NANOEngine::Graphics {

    struct PipelineSpecification {
        std::shared_ptr<IShader> shader;
        bool EnableDepthTest = true;
        bool EnableBlending = false;
        unsigned int CullMode = 0; // e.g., GL_BACK etc
        unsigned int PolygonMode = 0; // e.g., GL_FILL etc
    };

    class IPipeline {
    public:
        virtual ~IPipeline() = default;
        virtual void Bind() const = 0;
        virtual const PipelineSpecification& GetSpecification() const = 0;
    };
}

#endif // !NANOENGINE_GRAPHICS_IPIPELINE_HPP