#include "RenderGraph.hpp"
#include "../Interfaces/IFrameBuffer.hpp"
#include "Core/SpdLogger.hpp"
#include "../../Core/Logger.hpp"
#include "RenderGraph/TexturePool.hpp"


#include <glad/glad.h>
#include <queue>
#include <algorithm>
#include <cassert>

namespace NE::Graphics {

    //==========================================================================
    // Helper: Convert TextureFormat to OpenGL internal format
    //==========================================================================
    static GLenum GetGLInternalFormat(TextureFormat format) {
        switch (format) {
            case TextureFormat::R8:             return GL_R8;
            case TextureFormat::RG8:            return GL_RG8;
            case TextureFormat::RGB8:           return GL_RGB8;
            case TextureFormat::RGBA8:          return GL_RGBA8;
            case TextureFormat::R16F:           return GL_R16F;
            case TextureFormat::RG16F:          return GL_RG16F;
            case TextureFormat::RGB16F:         return GL_RGB16F;
            case TextureFormat::RGBA16F:        return GL_RGBA16F;
            case TextureFormat::R32F:           return GL_R32F;
            case TextureFormat::RG32F:          return GL_RG32F;
            case TextureFormat::RGB32F:         return GL_RGB32F;
            case TextureFormat::RGBA32F:        return GL_RGBA32F;
            case TextureFormat::Depth24:        return GL_DEPTH_COMPONENT24;
            case TextureFormat::Depth32F:       return GL_DEPTH_COMPONENT32F;
            case TextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
            default:                            return GL_RGBA8;
        }
    }

    static GLenum GetGLFormat(TextureFormat format) {
        switch (format) {
            case TextureFormat::R8:
            case TextureFormat::R16F:
            case TextureFormat::R32F:
                return GL_RED;
            case TextureFormat::RG8:
            case TextureFormat::RG16F:
            case TextureFormat::RG32F:
                return GL_RG;
            case TextureFormat::RGB8:
            case TextureFormat::RGB16F:
            case TextureFormat::RGB32F:
                return GL_RGB;
            case TextureFormat::RGBA8:
            case TextureFormat::RGBA16F:
            case TextureFormat::RGBA32F:
                return GL_RGBA;
            case TextureFormat::Depth24:
            case TextureFormat::Depth32F:
                return GL_DEPTH_COMPONENT;
            case TextureFormat::Depth24Stencil8:
                return GL_DEPTH_STENCIL;
            default:
                return GL_RGBA;
        }
    }

    static GLenum GetGLType(TextureFormat format) {
        switch (format) {
            case TextureFormat::R8:
            case TextureFormat::RG8:
            case TextureFormat::RGB8:
            case TextureFormat::RGBA8:
                return GL_UNSIGNED_BYTE;
            case TextureFormat::R16F:
            case TextureFormat::RG16F:
            case TextureFormat::RGB16F:
            case TextureFormat::RGBA16F:
            case TextureFormat::R32F:
            case TextureFormat::RG32F:
            case TextureFormat::RGB32F:
            case TextureFormat::RGBA32F:
            case TextureFormat::Depth32F:
                return GL_FLOAT;
            case TextureFormat::Depth24:
                return GL_UNSIGNED_INT;
            case TextureFormat::Depth24Stencil8:
                return GL_UNSIGNED_INT_24_8;
            default:
                return GL_UNSIGNED_BYTE;
        }
    }

    static bool IsDepthFormat(TextureFormat format) {
        return format == TextureFormat::Depth24 ||
               format == TextureFormat::Depth32F ||
               format == TextureFormat::Depth24Stencil8;
    }

    //==========================================================================
    // RenderGraphContext Implementation
    //==========================================================================

    uint32_t RenderGraphContext::GetTexture(RenderGraphResource handle) const {
        return graph->GetTexture(handle);
    }

    IFrameBuffer* RenderGraphContext::GetFramebuffer(RenderGraphResource handle) const {
        return graph->GetFramebuffer(handle);
    }

    //==========================================================================
    // RenderGraphBuilder Implementation
    //==========================================================================

    RenderGraphBuilder::RenderGraphBuilder(RenderGraph* graph, size_t passIndex)
        : m_Graph(graph)
        , m_PassIndex(passIndex)
    {
    }

    RenderGraphBuilder& RenderGraphBuilder::Read(RenderGraphResource resource) {
        if (resource.IsValid()) {
            m_Graph->GetPassData(m_PassIndex).reads.push_back(resource);
        }
        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::Write(RenderGraphResource resource) {
        if (resource.IsValid()) {
            m_Graph->GetPassData(m_PassIndex).writes.push_back(resource);
        }
        return *this;
    }

    RenderGraphBuilder& RenderGraphBuilder::Execute(std::function<void(const RenderGraphContext&)> callback) {
        m_Graph->GetPassData(m_PassIndex).executeCallback = std::move(callback);
        return *this;
    }

    //==========================================================================
    // RenderGraph Implementation
    //==========================================================================

    RenderGraph::~RenderGraph() {
        FreeTransientResources();
    }

    RenderGraphResource RenderGraph::ImportTexture(uint32_t textureId, const std::string& name) {
        ResourceData data;
        data.type = ResourceType::ImportedTexture;
        data.name = name;
        data.textureId = textureId;

        RenderGraphResource handle;
        handle.id = static_cast<uint32_t>(m_Resources.size());
        m_Resources.push_back(std::move(data));

        return handle;
    }

    RenderGraphResource RenderGraph::ImportFramebuffer(IFrameBuffer* fb, const std::string& name) {
        ResourceData data;
        data.type = ResourceType::ImportedFramebuffer;
        data.name = name;
        data.framebuffer = fb;

        RenderGraphResource handle;
        handle.id = static_cast<uint32_t>(m_Resources.size());
        m_Resources.push_back(std::move(data));

        return handle;
    }

    RenderGraphResource RenderGraph::CreateTexture(const TextureDesc& desc) {
        ResourceData data;
        data.type = ResourceType::TransientTexture;
        data.name = desc.name;
        data.desc = desc;
        data.allocated = false;

        RenderGraphResource handle;
        handle.id = static_cast<uint32_t>(m_Resources.size());
        m_Resources.push_back(std::move(data));

        return handle;
    }

    RenderGraphBuilder RenderGraph::AddPass(const std::string& name) {
        m_Compiled = false;

        RenderPassData pass;
        pass.name = name;

        size_t index = m_Passes.size();
        m_Passes.push_back(std::move(pass));

        return RenderGraphBuilder(this, index);
    }

    void RenderGraph::Compile() {
        if (m_Compiled) {
            return;
        }

        // Perform topological sort to determine execution order
        if (!TopologicalSort()) {
           SPD_ERROR("RenderGraph: Cyclic dependency detected!");
            return;
        }

        // Compute resource lifetimes for visualization and future pooling
        ComputeResourceLifetimes();

        // Allocate transient resources
        AllocateTransientResources();

        m_Compiled = true;
    }

    void RenderGraph::Execute() {
        if (!m_Compiled) {
			SPD_WARNING("RenderGraph: Executing uncompiled graph, compiling now...");
            Compile();
        }

        RenderGraphContext context;
        context.graph = this;

        // Execute passes in topological order
        for (size_t passIndex : m_ExecutionOrder) {
            const auto& pass = m_Passes[passIndex];

            if (pass.executeCallback) {
                pass.executeCallback(context);
            }
        }
    }

    void RenderGraph::Reset() {
        m_Passes.clear();
        m_ExecutionOrder.clear();
        m_Compiled = false;
        // Note: Keep resources for reuse
    }

    void RenderGraph::Clear() {
        FreeTransientResources();
        m_Passes.clear();
        m_Resources.clear();
        m_ExecutionOrder.clear();
        m_Compiled = false;
    }

    uint32_t RenderGraph::GetTexture(RenderGraphResource handle) const {
        if (!handle.IsValid() || handle.id >= m_Resources.size()) {
            return 0;
        }

        const auto& resource = m_Resources[handle.id];

        switch (resource.type) {
            case ResourceType::ImportedTexture:
                return resource.textureId;
            case ResourceType::ImportedFramebuffer:
                if (resource.framebuffer) {
                    return resource.framebuffer->GetColorAttachment();
                }
                return 0;
            case ResourceType::TransientTexture:
                return resource.transientTextureId;
            default:
                return 0;
        }
    }

    IFrameBuffer* RenderGraph::GetFramebuffer(RenderGraphResource handle) const {
        if (!handle.IsValid() || handle.id >= m_Resources.size()) {
            return nullptr;
        }

        const auto& resource = m_Resources[handle.id];

        if (resource.type == ResourceType::ImportedFramebuffer) {
            return resource.framebuffer;
        }

        return nullptr;
    }

    RenderPassData& RenderGraph::GetPassData(size_t index) {
        return m_Passes[index];
    }

    bool RenderGraph::TopologicalSort() {
        const size_t numPasses = m_Passes.size();

        if (numPasses == 0) {
            m_ExecutionOrder.clear();
            return true;
        }

        // Build dependency graph
        std::vector<std::vector<size_t>> adjacency(numPasses);
        std::vector<size_t> inDegree(numPasses, 0);

        BuildDependencyGraph(adjacency, inDegree);

        // Kahn's algorithm for topological sort
        std::queue<size_t> zeroInDegree;
        for (size_t i = 0; i < numPasses; ++i) {
            if (inDegree[i] == 0) {
                zeroInDegree.push(i);
            }
        }

        m_ExecutionOrder.clear();
        m_ExecutionOrder.reserve(numPasses);

        while (!zeroInDegree.empty()) {
            size_t current = zeroInDegree.front();
            zeroInDegree.pop();
            m_ExecutionOrder.push_back(current);

            for (size_t neighbor : adjacency[current]) {
                --inDegree[neighbor];
                if (inDegree[neighbor] == 0) {
                    zeroInDegree.push(neighbor);
                }
            }
        }

        // Check for cycles
        if (m_ExecutionOrder.size() != numPasses) {
            return false; // Cycle detected
        }

        return true;
    }

    void RenderGraph::BuildDependencyGraph(
        std::vector<std::vector<size_t>>& adjacency,
        std::vector<size_t>& inDegree) const
    {
        // Build a map: resource -> pass that writes it
        std::unordered_map<uint32_t, size_t> resourceWriters;

        for (size_t i = 0; i < m_Passes.size(); ++i) {
            for (const auto& write : m_Passes[i].writes) {
                resourceWriters[write.id] = i;
            }
        }

        // For each pass that reads a resource, add edge from writer to reader
        for (size_t i = 0; i < m_Passes.size(); ++i) {
            for (const auto& read : m_Passes[i].reads) {
                auto it = resourceWriters.find(read.id);
                if (it != resourceWriters.end() && it->second != i) {
                    // Pass it->second writes this resource, pass i reads it
                    // So it->second must execute before i
                    adjacency[it->second].push_back(i);
                    ++inDegree[i];
                }
            }
        }
    }

    void RenderGraph::ComputeResourceLifetimes() {
        m_ResourceLifetimes.clear();
        m_ResourceLifetimes.resize(m_Resources.size());

        // Initialize lifetimes
        for (size_t i = 0; i < m_Resources.size(); ++i) {
            m_ResourceLifetimes[i].resourceId = static_cast<uint32_t>(i);
            m_ResourceLifetimes[i].firstPassIndex = -1;
            m_ResourceLifetimes[i].lastPassIndex = -1;
            m_ResourceLifetimes[i].isImported = (m_Resources[i].type != ResourceType::TransientTexture);
        }

        // Scan passes in execution order to find first/last usage
        for (size_t execIdx = 0; execIdx < m_ExecutionOrder.size(); ++execIdx) {
            size_t passIdx = m_ExecutionOrder[execIdx];
            const auto& pass = m_Passes[passIdx];

            // Check reads
            for (const auto& res : pass.reads) {
                if (res.IsValid() && res.id < m_ResourceLifetimes.size()) {
                    auto& lifetime = m_ResourceLifetimes[res.id];
                    if (lifetime.firstPassIndex == -1) {
                        lifetime.firstPassIndex = static_cast<int>(execIdx);
                    }
                    lifetime.lastPassIndex = static_cast<int>(execIdx);
                }
            }

            // Check writes
            for (const auto& res : pass.writes) {
                if (res.IsValid() && res.id < m_ResourceLifetimes.size()) {
                    auto& lifetime = m_ResourceLifetimes[res.id];
                    if (lifetime.firstPassIndex == -1) {
                        lifetime.firstPassIndex = static_cast<int>(execIdx);
                    }
                    lifetime.lastPassIndex = static_cast<int>(execIdx);
                }
            }
        }
    }

    void RenderGraph::AllocateTransientResources() {
        const bool usePool = IsPoolingEnabled();

        for (auto& resource : m_Resources) {
            if (resource.type == ResourceType::TransientTexture && !resource.allocated) {
                const auto& desc = resource.desc;

                if (usePool) {
                    // Use pooled allocation
                    PooledTexture* pooled = m_TexturePool->Acquire(desc.width, desc.height, desc.format);
                    if (pooled) {
                        resource.pooledTexture = pooled;
                        resource.transientTextureId = pooled->textureId;
                        resource.transientFboId = pooled->fboId;
                        resource.allocated = true;
                        continue;
                    }
                    // Fall through to direct allocation if pool fails
                }

                // Direct allocation (no pool or pool failed)
                GLuint textureId;
                glGenTextures(1, &textureId);
                glBindTexture(GL_TEXTURE_2D, textureId);

                GLenum internalFormat = GetGLInternalFormat(desc.format);
                GLenum format = GetGLFormat(desc.format);
                GLenum type = GetGLType(desc.format);

                glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                             desc.width, desc.height, 0,
                             format, type, nullptr);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                glBindTexture(GL_TEXTURE_2D, 0);

                resource.transientTextureId = textureId;

                // Create FBO for this texture
                GLuint fboId;
                glGenFramebuffers(1, &fboId);
                glBindFramebuffer(GL_FRAMEBUFFER, fboId);

                if (IsDepthFormat(desc.format)) {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                           GL_TEXTURE_2D, textureId, 0);
                    glDrawBuffer(GL_NONE);
                    glReadBuffer(GL_NONE);
                } else {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D, textureId, 0);
                }

                GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                if (status != GL_FRAMEBUFFER_COMPLETE) {
                   SPD_ERROR(
                        "RenderGraph: Failed to create FBO for transient texture '{}', status: {}",
                        desc.name, status);
                }

                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                resource.transientFboId = fboId;
                resource.pooledTexture = nullptr;
                resource.allocated = true;
            }
        }
    }

    void RenderGraph::FreeTransientResources() {
        for (auto& resource : m_Resources) {
            if (resource.type == ResourceType::TransientTexture && resource.allocated) {
                if (resource.pooledTexture) {
                    // Return to pool (pool manages the GPU resources)
                    if (m_TexturePool) {
                        m_TexturePool->Release(resource.pooledTexture);
                    }
                    resource.pooledTexture = nullptr;
                } else {
                    // Direct deallocation
                    if (resource.transientFboId != 0) {
                        glDeleteFramebuffers(1, &resource.transientFboId);
                    }
                    if (resource.transientTextureId != 0) {
                        glDeleteTextures(1, &resource.transientTextureId);
                    }
                }
                resource.transientFboId = 0;
                resource.transientTextureId = 0;
                resource.allocated = false;
            }
        }
    }

    //==========================================================================
    // Debug/Visualization Helpers
    //==========================================================================

    const std::string& RenderGraph::GetResourceName(RenderGraphResource handle) const {
        static const std::string s_Invalid = "<invalid>";
        if (!handle.IsValid() || handle.id >= m_Resources.size()) {
            return s_Invalid;
        }
        return m_Resources[handle.id].name;
    }

    const char* RenderGraph::GetResourceTypeString(ResourceType type) {
        switch (type) {
            case ResourceType::ImportedTexture:    return "Imported Texture";
            case ResourceType::ImportedFramebuffer: return "Imported Framebuffer";
            case ResourceType::TransientTexture:   return "Transient Texture";
            default:                               return "Unknown";
        }
    }

    const char* RenderGraph::GetTextureFormatString(TextureFormat format) {
        switch (format) {
            case TextureFormat::R8:              return "R8";
            case TextureFormat::RG8:             return "RG8";
            case TextureFormat::RGB8:            return "RGB8";
            case TextureFormat::RGBA8:           return "RGBA8";
            case TextureFormat::R16F:            return "R16F";
            case TextureFormat::RG16F:           return "RG16F";
            case TextureFormat::RGB16F:          return "RGB16F";
            case TextureFormat::RGBA16F:         return "RGBA16F";
            case TextureFormat::R32F:            return "R32F";
            case TextureFormat::RG32F:           return "RG32F";
            case TextureFormat::RGB32F:          return "RGB32F";
            case TextureFormat::RGBA32F:         return "RGBA32F";
            case TextureFormat::Depth24:         return "Depth24";
            case TextureFormat::Depth32F:        return "Depth32F";
            case TextureFormat::Depth24Stencil8: return "Depth24Stencil8";
            default:                             return "Unknown";
        }
    }

} // namespace NE::Graphics
