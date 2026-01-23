#ifndef NANOENGINE_GRAPHICS_ISTATECACHE_HPP
#define NANOENGINE_GRAPHICS_ISTATECACHE_HPP

#include <memory>
#include <string>
#include "IPipeline.hpp"

namespace NE::Graphics {

    class IStateCache {
    public:
        virtual ~IStateCache() = default;
        virtual void InvalidateAll() = 0;
        virtual void Reset() = 0;
        virtual void Bind(const PipelineSpecification& spec) = 0;
		virtual void Bind(const std::shared_ptr<IPipeline>& pipeline) = 0;
    };
}

#endif // !NANOENGINE_GRAPHICS_ISTATECACHE_HPP