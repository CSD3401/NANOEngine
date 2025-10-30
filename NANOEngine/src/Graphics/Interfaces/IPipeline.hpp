#ifndef NANOENGINE_GRAPHICS_IPIPELINE_HPP
#define NANOENGINE_GRAPHICS_IPIPELINE_HPP

#include <memory>
#include <string>
#include "IShader.hpp"

namespace NE::Graphics {

    struct PipelineKey;
    struct PipelineSpecification;

    class IPipeline {
    public:
        virtual ~IPipeline() = default;
        virtual void Bind() const = 0;
        virtual const PipelineSpecification& GetSpecification() const = 0;
		virtual const PipelineKey GetKey() const = 0;
        virtual const std::string_view GetShaderUUID() const = 0;

        virtual const std::string& GetName() const = 0;
    };
}

#endif // !NANOENGINE_GRAPHICS_IPIPELINE_HPP

/*
struct PipelineKey {
        uint64_t shaderID : 48;
        uint64_t blending : 1;
        uint64_t depthTest : 1;
        uint64_t cullMode : 6;
        uint64_t polygonMode : 6;

        static PipelineKey FromSpecification(const PipelineSpecification& spec) {
            PipelineKey key = {};
            key.shaderID = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(spec.shader.get()));
            key.blending = spec.EnableBlending ? 1 : 0;
            key.depthTest = spec.EnableDepthTest ? 1 : 0;
            key.cullMode = spec.CullMode & 0x3F; // 6 bits
            key.polygonMode = spec.PolygonMode & 0x3F; // 6 bits
            return key;
        }

        uint64_t Pack() const {
            return  ((shaderID & ((1ULL << 48) - 1)) << 14)
                | ((blending & 1ULL) << 13)
                | ((depthTest & 1ULL) << 12)
                | ((cullMode & 0x3FULL) << 6)
                | ((polygonMode & 0x3FULL) << 0);
        }

        bool operator<(const PipelineKey& other) const {
            if (shaderID != other.shaderID)
                return shaderID < other.shaderID;
            if (blending != other.blending)
                return blending < other.blending;
            if (depthTest != other.depthTest)
                return depthTest < other.depthTest;
            if (cullMode != other.cullMode)
                return cullMode < other.cullMode;
            return polygonMode < other.polygonMode;
        }

        bool operator==(const PipelineKey& other) const {
            return shaderID == other.shaderID &&
                depthTest == other.depthTest &&
                blending == other.blending &&
                cullMode == other.cullMode &&
                polygonMode == other.polygonMode;
        }
    };
*/