#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include "../../NANOEngineAPI.hpp"

namespace NE::Graphics {

    class IFrameBuffer;
    class RenderGraph;
    class RenderGraphBuilder;

    //==========================================================================
    // RenderGraphResource - Handle-based resource reference
    //==========================================================================
    struct RenderGraphResource {
        static constexpr uint32_t InvalidId = UINT32_MAX;

        uint32_t id = InvalidId;

        bool IsValid() const { return id != InvalidId; }
        bool operator==(const RenderGraphResource& other) const { return id == other.id; }
        bool operator!=(const RenderGraphResource& other) const { return id != other.id; }
    };

    //==========================================================================
    // TextureFormat - Supported texture formats for transient resources
    //==========================================================================
    enum class TextureFormat {
        R8,
        RG8,
        RGB8,
        RGBA8,
        R16F,
        RG16F,
        RGB16F,
        RGBA16F,
        R32F,
        RG32F,
        RGB32F,
        RGBA32F,
        Depth24,
        Depth32F,
        Depth24Stencil8
    };

    //==========================================================================
    // TextureDesc - Description for creating transient textures
    //==========================================================================
    struct TextureDesc {
        uint32_t width = 0;
        uint32_t height = 0;
        TextureFormat format = TextureFormat::RGBA8;
        std::string name;
    };

    //==========================================================================
    // ResourceType - Internal resource type tracking
    //==========================================================================
    enum class ResourceType {
        ImportedTexture,
        ImportedFramebuffer,
        TransientTexture
    };

    //==========================================================================
    // ResourceData - Internal resource data storage
    //==========================================================================
    struct ResourceData {
        ResourceType type = ResourceType::ImportedTexture;
        std::string name;

        // For imported textures
        uint32_t textureId = 0;

        // For imported framebuffers
        IFrameBuffer* framebuffer = nullptr;

        // For transient textures
        TextureDesc desc;
        uint32_t transientTextureId = 0;
        uint32_t transientFboId = 0;
        bool allocated = false;
    };

    //==========================================================================
    // RenderGraphContext - Passed to pass execute callbacks
    //==========================================================================
    struct RenderGraphContext {
        RenderGraph* graph = nullptr;

        uint32_t GetTexture(RenderGraphResource handle) const;
        IFrameBuffer* GetFramebuffer(RenderGraphResource handle) const;
    };

    //==========================================================================
    // RenderPassData - Internal pass data storage
    //==========================================================================
    struct RenderPassData {
        std::string name;
        std::vector<RenderGraphResource> reads;
        std::vector<RenderGraphResource> writes;
        std::function<void(const RenderGraphContext&)> executeCallback;
    };

    //==========================================================================
    // RenderGraphBuilder - Fluent API for pass construction
    //==========================================================================
    class RenderGraphBuilder {
    public:
        RenderGraphBuilder(RenderGraph* graph, size_t passIndex);

        // Declare that this pass reads from a resource
        RenderGraphBuilder& Read(RenderGraphResource resource);

        // Declare that this pass writes to a resource
        RenderGraphBuilder& Write(RenderGraphResource resource);

        // Set the execute callback for this pass
        RenderGraphBuilder& Execute(std::function<void(const RenderGraphContext&)> callback);

    private:
        RenderGraph* m_Graph;
        size_t m_PassIndex;
    };

    //==========================================================================
    // RenderGraph - Main graph structure
    //==========================================================================
    class NANOENGINE_API RenderGraph {
    public:
        RenderGraph() = default;
        ~RenderGraph();

        // Non-copyable
        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;

        // Moveable
        RenderGraph(RenderGraph&&) = default;
        RenderGraph& operator=(RenderGraph&&) = default;

        //----------------------------------------------------------------------
        // Resource Import/Creation
        //----------------------------------------------------------------------

        // Import an existing texture (e.g., from a framebuffer attachment)
        RenderGraphResource ImportTexture(uint32_t textureId, const std::string& name);

        // Import an existing framebuffer
        RenderGraphResource ImportFramebuffer(IFrameBuffer* fb, const std::string& name);

        // Create a transient texture (lifetime managed by the graph)
        RenderGraphResource CreateTexture(const TextureDesc& desc);

        //----------------------------------------------------------------------
        // Pass Registration
        //----------------------------------------------------------------------

        // Add a new render pass, returns builder for fluent configuration
        RenderGraphBuilder AddPass(const std::string& name);

        //----------------------------------------------------------------------
        // Compilation & Execution
        //----------------------------------------------------------------------

        // Compile the graph: validate, compute execution order, allocate resources
        void Compile();

        // Execute all passes in dependency order
        void Execute();

        // Reset the graph for reuse (clears passes but keeps imported resources)
        void Reset();

        // Clear everything (passes and resources)
        void Clear();

        //----------------------------------------------------------------------
        // Resource Access (for use during pass execution)
        //----------------------------------------------------------------------

        // Get the OpenGL texture ID from a resource handle
        uint32_t GetTexture(RenderGraphResource handle) const;

        // Get the framebuffer pointer from a resource handle
        IFrameBuffer* GetFramebuffer(RenderGraphResource handle) const;

        //----------------------------------------------------------------------
        // Query
        //----------------------------------------------------------------------

        bool IsCompiled() const { return m_Compiled; }
        size_t GetPassCount() const { return m_Passes.size(); }
        size_t GetResourceCount() const { return m_Resources.size(); }

        //----------------------------------------------------------------------
        // Debug/Visualization Access
        //----------------------------------------------------------------------

        const std::vector<RenderPassData>& GetPasses() const { return m_Passes; }
        const std::vector<ResourceData>& GetResources() const { return m_Resources; }
        const std::vector<size_t>& GetExecutionOrder() const { return m_ExecutionOrder; }

        // Get resource name by handle
        const std::string& GetResourceName(RenderGraphResource handle) const;

        // Get resource type string for display
        static const char* GetResourceTypeString(ResourceType type);
        static const char* GetTextureFormatString(TextureFormat format);

    private:
        friend class RenderGraphBuilder;

        // Internal: Get pass data for builder
        RenderPassData& GetPassData(size_t index);

        // Internal: Topological sort using Kahn's algorithm
        bool TopologicalSort();

        // Internal: Allocate transient resources
        void AllocateTransientResources();

        // Internal: Free transient resources
        void FreeTransientResources();

        // Internal: Build adjacency list for dependency graph
        void BuildDependencyGraph(
            std::vector<std::vector<size_t>>& adjacency,
            std::vector<size_t>& inDegree) const;

        std::vector<RenderPassData> m_Passes;
        std::vector<ResourceData> m_Resources;
        std::vector<size_t> m_ExecutionOrder;
        bool m_Compiled = false;
    };

} // namespace NE::Graphics
