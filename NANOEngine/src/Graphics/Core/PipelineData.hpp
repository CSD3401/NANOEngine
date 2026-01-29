#pragma once
#include <string>
#include <memory>
#include "../Interfaces/IShader.hpp"

namespace NE::Graphics {

    struct PipelineSpecification {
        std::shared_ptr<IShader> shader;
        std::string shaderName;
        bool EnableBlending = false;
        bool EnableDepthTest = true;
		bool DepthWrite = true;

        int CullMode = 0;
		int PolygonMode = 0x1B02; // GL_FILL
    };

    struct PipelineKey {
        std::string shaderUUID;
        bool blending{};
        bool depthTest{};
		bool depthWrite{};
        int  cullMode{};
        int  polygonMode{};

        inline bool operator==(const PipelineKey& o) const {
            return shaderUUID == o.shaderUUID &&
                blending == o.blending &&
                depthTest == o.depthTest &&
				depthWrite == o.depthWrite &&
                cullMode == o.cullMode &&
                polygonMode == o.polygonMode;
        }
        inline bool operator!=(const PipelineKey& o) const {
			return !(*this == o);
        }
        inline bool operator<(const PipelineKey& o) const {
			if (shaderUUID != o.shaderUUID)
				return shaderUUID < o.shaderUUID;
			if (blending != o.blending)
				return blending < o.blending;
			if (depthTest != o.depthTest)
				return depthTest < o.depthTest;
            if (depthWrite != o.depthWrite)
				return depthWrite < o.depthWrite;
			if (cullMode != o.cullMode)
				return cullMode < o.cullMode;
			return polygonMode < o.polygonMode;
        }
        inline uint64_t Pack() const {
            uint64_t h = std::hash<std::string>{}(shaderUUID) & ((1ULL << 48) - 1);
            return  (h << 15)
                | ((uint64_t(blending) & 1ULL) << 14)
                | ((uint64_t(depthTest) & 1ULL) << 13)
                | ((uint64_t(depthWrite) & 1ULL) << 12)
                | ((uint64_t(cullMode) & 0x3FULL) << 6)
                | ((uint64_t(polygonMode) & 0x3FULL) << 0);
        }
        static PipelineKey MakeKey(const PipelineSpecification& s) {
            PipelineKey k;
            // GLShader exposes GetUUID() (used by GLPipeline::GetShaderUUID), so we use it here too.
            //k.shaderUUID = std::string(s.shader ? s.shader->GetUUID() : std::string_view{});
            k.shaderUUID = s.shaderName;
            k.blending = s.EnableBlending;
            k.depthTest = s.EnableDepthTest;
			k.depthWrite = s.DepthWrite;
            k.cullMode = s.CullMode;
            k.polygonMode = s.PolygonMode;
            return k;
        }
    };

    struct PipelineKeyHash {
        inline size_t operator()(const PipelineKey& k) const noexcept {
            size_t h = std::hash<std::string>{}(k.shaderUUID);
            h ^= std::hash<bool>{}(k.depthTest) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= std::hash<bool>{}(k.depthWrite) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= std::hash<bool>{}(k.blending) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(k.cullMode) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(k.polygonMode) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            return h;
        }
    };
}