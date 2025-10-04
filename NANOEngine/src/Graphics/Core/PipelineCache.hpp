#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "PipelineData.hpp" // for PipelineKey, PipelineSpecification
#include "../OpenGL/GLPipeline.hpp"

namespace NE::Graphics {

    class PipelineCache {
    public:
        std::shared_ptr<NE::Graphics::IPipeline> GetOrCreate(const NE::Graphics::PipelineSpecification& spec);

        void InvalidateShader(std::string_view shaderUUID);
        void Clear() { m_cache.clear(); }

    private:
        std::unordered_map<PipelineKey, std::weak_ptr<NE::Graphics::IPipeline>, PipelineKeyHash> m_cache;
    };

    // Global (renderer-owned) accessor. Implemented in .cpp as a function-local static.
    PipelineCache& GetPipelineCache();

} // namespace NE::Graphics
