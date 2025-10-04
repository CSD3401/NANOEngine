#include "PipelineCache.hpp"
#include "../OpenGL/GLPipeline.hpp"
//#include "../OpenGL/GLShader.hpp"
//#include "../../AssetManager.hpp"

namespace NE::Graphics {

    static PipelineKey MakeKey(const PipelineSpecification& s) {
        PipelineKey k;
        // GLShader exposes GetUUID() (used by GLPipeline::GetShaderUUID), so we use it here too.
        //k.shaderUUID = std::string(s.shader ? s.shader->GetUUID() : std::string_view{});
        k.shaderUUID = s.shaderName;
        k.depthTest = s.EnableDepthTest;
        k.blending = s.EnableBlending;
        k.cullMode = s.CullMode;
        k.polygonMode = s.PolygonMode;
        return k;
    }

    std::shared_ptr<IPipeline> PipelineCache::GetOrCreate(const PipelineSpecification& spec) {
        auto key = MakeKey(spec);
        if (auto it = m_cache.find(key); it != m_cache.end()) {
            if (auto sp = it->second.lock()) return sp;
        }
        auto created = std::make_shared<OpenGL::GLPipeline>(spec);
        m_cache[key] = created;
        return created;
    }

    void PipelineCache::InvalidateShader(std::string_view shaderUUID) {
        for (auto it = m_cache.begin(); it != m_cache.end(); ) {
            if (it->first.shaderUUID == shaderUUID) it = m_cache.erase(it);
            else ++it;
        }
    }

    PipelineCache& GetPipelineCache() {
        static PipelineCache s_cache;
        return s_cache;
    }

} // namespace NE::Graphics
