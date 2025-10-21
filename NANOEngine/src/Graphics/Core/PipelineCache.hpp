#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "../OpenGL/GLPipeline.hpp" // for IPipeline, PipelineSpecification

namespace NE::Graphics {

    struct PipelineKey {
        std::string shaderUUID;
        bool depthTest{};
        bool blending{};
        int  cullMode{};
        int  polygonMode{};
        bool operator==(const PipelineKey& o) const = default;
    };

    struct PipelineKeyHash {
        size_t operator()(const PipelineKey& k) const noexcept {
            size_t h = std::hash<std::string>{}(k.shaderUUID);
            h ^= std::hash<bool>{}(k.depthTest) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= std::hash<bool>{}(k.blending) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(k.cullMode) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(k.polygonMode) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            return h;
        }
    };

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
