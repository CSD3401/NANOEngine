#include "UIRenderSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
#include "../Components/UIText.hpp"
#include "../Components/Hierarchy.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Graphics/Core/EditorCamera.hpp"
#include "../../Graphics/Core/UITextMeshGenerator.hpp"
#include "../../Graphics/OpenGL/GLVertexBuffer.hpp"
#include "../../Graphics/OpenGL/GLIndexBuffer.hpp"
#include "../../Graphics/OpenGL/GLGeometryBuffer.hpp"
#include "../../Graphics/OpenGL/GLPipeline.hpp"
#include "../../Graphics/OpenGL/GLShader.hpp"
#include "../../Graphics/Core/Material.hpp"
#include "../../Graphics/Core/UIGeometryBuffer.hpp"
#include "../../Graphics/Core/DrawCommand.hpp"
#include "../../Graphics/Core/Vertex.hpp"
#include "../../Graphics/Core/InstanceData.hpp"
#include "../../Graphics/Core/PipelineCache.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include "../Components/EntityMeta.hpp"
// UIRectMask2D folded into UIRectTransform (enableMask + maskPadding fields)
#include "UITransformUtilities.hpp"
using namespace NE::ECS;
using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    //=========================================================================
    // Constants
    //=========================================================================

    static constexpr float DEFAULT_ANCHOR_X = 0.5f;
    static constexpr float DEFAULT_ANCHOR_Y = 0.5f;

    // Default world space canvas scale (so 100x100 UI = 10x10 world units)
    static constexpr float DEFAULT_WORLD_SPACE_SCALE = 0.1f;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    UIRenderSystem::UIRenderSystem(ComponentManager* cm) : m_cm(cm)
    {
    }
    bool UIRenderSystem::IsActiveForUI(Entity entity, Entity canvasEntity) const
    {
        return UIUtil::IsActiveForUI(m_cm, entity, canvasEntity);
    }

    void UIRenderSystem::OnEntityAdded(Entity e)
    {
        if (!m_cm->HasComponent<UIImage>(e)) return;

        auto& img = m_cm->GetComponent<UIImage>(e);

        if (!img.textureUUID.empty()) {
            auto texture = NE::Resource::ResourceManager::GetInstance()
                .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
            if (texture) {
                img.bindlessHandle = texture->GetBindlessHandle();
            }
        }

        if (!img.materialUUID.empty()) {
            img.material = NE::Resource::ResourceManager::GetInstance()
                .LoadResource<NE::Graphics::Material>(img.materialUUID);
        }

        img.isDirty = true;
    }

    void UIRenderSystem::OnEntityRemoved(Entity e)
    {
        (void)e; // Unused parameter
    }

    void UIRenderSystem::Init()
    {
        // Load UI sprite shader from .nanoshader asset file
        try {
            auto uiShader = NE::Resource::ResourceManager::GetInstance()
                .LoadResource<NE::Graphics::OpenGL::GLShader>("neuisprite");
            if (!uiShader) {
                std::cerr << "[UIRenderSystem] ERROR: Failed to load UI sprite shader (neuisprite)" << std::endl;
                return;
            }

            NE::Graphics::PipelineSpecification spec;
            spec.shader = uiShader;
            spec.EnableBlending = true;
            spec.EnableDepthTest = false;
            spec.DepthWrite = false;
            spec.CullMode = 0;
            spec.PolygonMode = 0x1B02;

            auto pipeline = std::make_shared<NE::Graphics::OpenGL::GLPipeline>(spec, "UI_Sprite");
            m_defaultUIMaterial = std::make_shared<NE::Graphics::Material>(pipeline);
            m_defaultUIMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);

        } catch (const std::exception& e) {
            std::cerr << "[UIRenderSystem] Error loading UI sprite material: " << e.what() << std::endl;
        }

        // Load UI text shader from .nanoshader asset file
        try {
            auto textShader = NE::Resource::ResourceManager::GetInstance()
                .LoadResource<NE::Graphics::OpenGL::GLShader>("neuitext");
            if (!textShader) {
                std::cerr << "[UIRenderSystem] ERROR: Failed to load UI text shader (neuitext)" << std::endl;
                return;
            }

            NE::Graphics::PipelineSpecification textSpec;
            textSpec.shader = textShader;
            textSpec.EnableBlending = true;
            textSpec.EnableDepthTest = false;
            textSpec.DepthWrite = false;
            textSpec.CullMode = 0;
            textSpec.PolygonMode = 0x1B02;

            auto textPipeline = std::make_shared<NE::Graphics::OpenGL::GLPipeline>(textSpec, "UI_Text");
            m_defaultTextMaterial = std::make_shared<NE::Graphics::Material>(textPipeline);
            m_defaultTextMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);

        } catch (const std::exception& e) {
            std::cerr << "[UIRenderSystem] Error loading UI text material: " << e.what() << std::endl;
        }

        // Load per-entity textures and materials
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIImage>(e)) continue;

            auto& img = m_cm->GetComponent<UIImage>(e);

            if (!img.textureUUID.empty() && img.bindlessHandle == 0) {
                auto texture = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
                if (texture) {
                    texture->MakeResident();  // CRITICAL: Make texture resident for bindless access
                    img.bindlessHandle = texture->GetBindlessHandle();
                }
            }

            if (!img.materialUUID.empty() && !img.material) {
                img.material = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::Material>(img.materialUUID);
            }
        }
    }

    std::shared_ptr<NE::Graphics::Material> UIRenderSystem::AcquireSpriteMaterial()
    {
        if (m_spriteMaterialIndex >= m_spriteMaterialPool.size()) {
            m_spriteMaterialPool.push_back(
                std::make_shared<NE::Graphics::Material>(m_defaultUIMaterial->GetPipeline())
            );
        }
        return m_spriteMaterialPool[m_spriteMaterialIndex++];
    }

    std::shared_ptr<NE::Graphics::Material> UIRenderSystem::AcquireTextMaterial()
    {
        if (m_textMaterialIndex >= m_textMaterialPool.size()) {
            m_textMaterialPool.push_back(
                std::make_shared<NE::Graphics::Material>(m_defaultTextMaterial->GetPipeline())
            );
        }
        return m_textMaterialPool[m_textMaterialIndex++];
    }

    std::shared_ptr<NE::Graphics::IGeometryBuffer> UIRenderSystem::AcquireGeometryBuffer(
        const std::vector<NE::Graphics::UIVertex2>& vertices,
        const std::vector<uint32_t>& indices
    )
    {
        if (m_geometryIndex >= m_geometryPool.size()) {
            m_geometryPool.push_back(std::make_shared<NE::Graphics::UIGeometryBuffer>());
        }
        auto buf = std::static_pointer_cast<NE::Graphics::UIGeometryBuffer>(m_geometryPool[m_geometryIndex++]);
        buf->Upload(vertices, indices);
        return buf;
    }

    void UIRenderSystem::Update(double)
    {
        // Reset pool indices for this frame
        m_spriteMaterialIndex = 0;
        m_textMaterialIndex = 0;
        m_geometryIndex = 0;

        const auto& entities = GetEntities();

        // Runtime texture loading - handle textures dragged at runtime
        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIImage>(e)) continue;

            auto& img = m_cm->GetComponent<UIImage>(e);

            // Check if texture UUID changed but handle not loaded
            if (!img.textureUUID.empty() && img.bindlessHandle == 0) {
                auto texture = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
                if (texture) {
                    texture->MakeResident();  // CRITICAL: Make texture resident for bindless access
                    img.bindlessHandle = texture->GetBindlessHandle();
                }
            }
            // Check if texture was removed (UUID cleared)
            else if (img.textureUUID.empty() && img.bindlessHandle != 0) {
                img.bindlessHandle = 0;
            }
        }

        // Single O(N) pass: bucket all UI entities by their owning canvas
        BuildCanvasChildrenMap();

        std::vector<std::pair<int, Entity>> canvases;

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UICanvas>(e)) continue;

            auto& canvas = m_cm->GetComponent<UICanvas>(e);

            bool metaActive = true;
            if (m_cm->HasComponent<NE::ECS::Component::EntityMeta>(e)) {
                metaActive = m_cm->GetComponent<NE::ECS::Component::EntityMeta>(e).isActive;
            }

            if (canvas.isActive && metaActive) {
                canvases.push_back({ canvas.sortingOrder, e });
            }

        }

        std::sort(canvases.begin(), canvases.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& [order, canvasEntity] : canvases) {
            auto& canvas = m_cm->GetComponent<UICanvas>(canvasEntity);

            // Calculate scale factor based on scale mode
            canvas.scaleFactor = m_layoutEngine->CalculateScaleFactor(canvas);

            // Setup canvas defaults based on render mode
            SetupCanvasDefaults(canvasEntity, canvas);

            // Get camera matrices if needed
            Math::Mat4 viewMatrix, projMatrix;
            Math::Mat4* pView = nullptr;
            Math::Mat4* pProj = nullptr;

            if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA ||
                canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                if (GetCameraMatrices(viewMatrix, projMatrix)) {
                    pView = &viewMatrix;
                    pProj = &projMatrix;
                }
                else {
                    std::cerr << "[UIRenderSystem] Warning: Canvas requires camera but none found!" << std::endl;
                }
            }

            RenderCanvasChildren(canvasEntity, canvas, pView, pProj);

            // Render text entities from pre-built map
            auto textIt = m_canvasChildrenMap.find(canvasEntity);
            if (textIt != m_canvasChildrenMap.end()) {
                for (Entity e : textIt->second.texts) {
                    RenderTextEntity(e, canvasEntity, canvas, pView, pProj);
                }
            }
        }
    }

    void UIRenderSystem::Exit()
    {
    }

    //=========================================================================
    // Canvas Setup Helpers
    //=========================================================================

    void UIRenderSystem::SetupCanvasDefaults(Entity canvasEntity, UICanvas& canvas)
    {
        if (!m_cm->HasComponent<UIRectTransform>(canvasEntity)) {
            return;
        }

        // Check if render mode changed or first time setup
        bool renderModeChanged = canvas.hasBeenInitialized &&
            (canvas.renderMode != canvas.lastInitializedMode);

        if (canvas.hasBeenInitialized && !renderModeChanged) {
            // Already initialized and mode hasn't changed - only update screen space position
            if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
                canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA)
            {
                auto& canvasRect = m_cm->GetComponent<UIRectTransform>(canvasEntity);
                float screenWidth = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenWidth());
                float screenHeight = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenHeight());
                canvasRect.x = screenWidth * DEFAULT_ANCHOR_X;
                canvasRect.y = screenHeight * DEFAULT_ANCHOR_Y;
                canvasRect.width = canvas.referenceWidth;
                canvasRect.height = canvas.referenceHeight;
            }
            return;
        }

        // First time OR render mode changed - apply full defaults
        auto& canvasRect = m_cm->GetComponent<UIRectTransform>(canvasEntity);

        switch (canvas.renderMode) {
        case UICanvas::RenderMode::SCREEN_SPACE_OVERLAY:
        case UICanvas::RenderMode::SCREEN_SPACE_CAMERA:
        {
            float screenWidth = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenWidth());
            float screenHeight = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenHeight());

            canvasRect.x = screenWidth * DEFAULT_ANCHOR_X;
            canvasRect.y = screenHeight * DEFAULT_ANCHOR_Y;
            canvasRect.z = 0.f;

            // Reset scale to 1 (scaleFactor handles scaling)
            canvasRect.scaleX = 1.f;
            canvasRect.scaleY = 1.f;
            canvasRect.scaleZ = 1.f;

            canvasRect.width = canvas.referenceWidth;
            canvasRect.height = canvas.referenceHeight;

            // Reset rotation
            canvasRect.rotationX = 0.f;
            canvasRect.rotationY = 0.f;
            canvasRect.rotationZ = 0.f;
            break;
        }
        case UICanvas::RenderMode::WORLD_SPACE:
        {
            // Apply default world space scale
            canvasRect.scaleX = DEFAULT_WORLD_SPACE_SCALE;
            canvasRect.scaleY = DEFAULT_WORLD_SPACE_SCALE;
            canvasRect.scaleZ = 1.f;

            // Reset position to origin (user can move it)
            canvasRect.x = 0.f;
            canvasRect.y = 0.f;
            canvasRect.z = 0.f;

            // Keep rotation as is or reset
            canvasRect.rotationX = 0.f;
            canvasRect.rotationY = 0.f;
            canvasRect.rotationZ = 0.f;
            break;
        }
        }
        // Mark as initialized with current mode
        canvas.hasBeenInitialized = true;
        canvas.lastInitializedMode = canvas.renderMode;
    }

    std::shared_ptr<NE::Graphics::IGeometryBuffer> UIRenderSystem::CreateDynamicUIGeometry(
        const std::vector<NE::Graphics::UIVertex2>& vertices
    )
    {
        if (vertices.empty()) {
            return nullptr;
        }

        // Generate quad indices (0,1,2, 2,3,0 pattern)
        std::vector<uint32_t> indices;
        size_t numQuads = vertices.size() / 4;
        indices.reserve(numQuads * 6);

        for (size_t i = 0; i < numQuads; ++i) {
            uint32_t base = static_cast<uint32_t>(i * 4);
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
            indices.push_back(base + 0);
        }

        return AcquireGeometryBuffer(vertices, indices);
    }

    std::shared_ptr<NE::Graphics::IGeometryBuffer> UIRenderSystem::CreateDynamicTextGeometry(
        const std::vector<NE::Graphics::UIVertex2>& vertices
    )
    {
        if (vertices.empty()) {
            return nullptr;
        }

        // Text vertices come as triangles (6 vertices per glyph), not quads
        std::vector<uint32_t> indices;
        indices.reserve(vertices.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(vertices.size()); ++i) {
            indices.push_back(i);
        }

        return AcquireGeometryBuffer(vertices, indices);
    }

    void UIRenderSystem::SubmitUIElement(
        Entity entity,
        const UICanvas& canvas,
        const UIImage& img,
        const UIRectTransform& rect,
        const std::vector<NE::Graphics::UIVertex2>& vertices,
        const std::optional<NE::Graphics::ScissorRect>& scissor
    )
    {
        (void)entity; // Unused parameter

        // Create dynamic geometry buffer
        auto geometryBuffer = CreateDynamicUIGeometry(vertices);
        if (!geometryBuffer) {
            std::cerr << "[UIRenderSystem::SubmitUIElement] ERROR: Failed to create geometry buffer!" << std::endl;
            return;
        }

        if (!m_defaultUIMaterial) {
            std::cerr << "[UIRenderSystem::SubmitUIElement] ERROR: No material available (shader loading failed)" << std::endl;
            return;
        }

        // Reuse pooled material instance instead of allocating per frame
        auto instanceMaterial = AcquireSpriteMaterial();

        // Set per-instance uniforms
        instanceMaterial->SetUniformVec4("uColor", img.color);

        // Get screen size from GraphicsManager
        uint32_t screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
        uint32_t screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

        // Set screen size as Vec2 (shader expects vec2)
        instanceMaterial->SetUniformVec2("uScreenSize",
            Math::Vec2(static_cast<float>(screenWidth), static_cast<float>(screenHeight)));

        // Bindless texture support
        if (img.bindlessHandle != 0) {
            // Texture is loaded, enable it and pass bindless handle
            instanceMaterial->SetUniformInt("uHasTexture", 1);
            instanceMaterial->SetUniformHandle("uTexture", img.bindlessHandle);
        } else {
            // No texture, render solid color only
            instanceMaterial->SetUniformInt("uHasTexture", 0);
        }

        // Set render queue - UI uses OVERLAY (4000) as base priority
        instanceMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
        instanceMaterial->SetQueueOffset(canvas.sortingOrder);

        // Build DrawCommand
        NE::Graphics::DrawCommand cmd;

        // Screen-space: vertices are already in pixel coordinates, shader converts to NDC
        // World-space: vertices are unit quads, worldMatrix positions them in 3D
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            cmd.transform = rect.worldMatrix;
        } else {
            cmd.transform = Math::Mat4(); // Identity — vertices pre-positioned in pixels
        }
        cmd.mesh = geometryBuffer;
        cmd.material = instanceMaterial;
        cmd.castsShadow = false;
        cmd.receivesShadow = false;

        cmd.boundsCenterWS = Math::Vec3(0.0f, 0.0f, 0.0f);
        cmd.boundsRadiusWs = 999999.0f;  // Always visible
        cmd.scissorRect = scissor;

        // Submit through standard pipeline
        NE::Graphics::GraphicsManager::Submit(cmd);
    }

    void UIRenderSystem::SubmitTextElement(
        Entity entity,
        const UICanvas& canvas,
        const UIText& text,
        const UIRectTransform& rect,
        const std::vector<NE::Graphics::UIVertex2>& vertices,
        std::shared_ptr<NE::Graphics::FontAtlas> fontAtlas,
        const std::optional<NE::Graphics::ScissorRect>& scissor
    )
    {
        (void)entity; // Unused parameter

        if (vertices.empty()) {
            return;
        }

        // Create dynamic geometry buffer
        auto geometryBuffer = CreateDynamicTextGeometry(vertices);
        if (!geometryBuffer) {
            std::cerr << "[UIRenderSystem::SubmitTextElement] ERROR: Failed to create text geometry buffer!" << std::endl;
            return;
        }

        if (!m_defaultTextMaterial) {
            std::cerr << "[UIRenderSystem::SubmitTextElement] ERROR: No default text material!" << std::endl;
            return;
        }

        // Reuse pooled material instance instead of allocating per frame
        auto instanceMaterial = AcquireTextMaterial();

        // Set text-specific uniforms
        instanceMaterial->SetUniformVec4("uColor", text.color);
        instanceMaterial->SetUniformVec2("uScreenSize",
            Math::Vec2(
                static_cast<float>(NE::Graphics::GraphicsManager::GetScreenWidth()),
                static_cast<float>(NE::Graphics::GraphicsManager::GetScreenHeight())
            )
        );

        // Set font atlas bindless handle
        if (fontAtlas) {
            instanceMaterial->SetUniformHandle("uFontAtlas", fontAtlas->GetBindlessHandle());
        }

        // Set render queue (OVERLAY + canvas sorting)
        instanceMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
        instanceMaterial->SetQueueOffset(canvas.sortingOrder);

        // Build DrawCommand
        NE::Graphics::DrawCommand cmd;

        // Screen-space: vertices are already in pixel coordinates, shader converts to NDC
        // World-space: vertices are unit quads, worldMatrix positions them in 3D
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            cmd.transform = rect.worldMatrix;
        } else {
            cmd.transform = Math::Mat4(); // Identity — vertices pre-positioned in pixels
        }
        cmd.mesh = geometryBuffer;
        cmd.material = instanceMaterial;
        cmd.castsShadow = false;
        cmd.receivesShadow = false;
        cmd.boundsRadiusWs = 999999.0f;  // Always visible
        cmd.scissorRect = scissor;

        // Submit through integrated pipeline
        NE::Graphics::GraphicsManager::Submit(cmd);
    }

    //=========================================================================
    // Canvas Children Collection (single-pass)
    //=========================================================================

    void UIRenderSystem::BuildCanvasChildrenMap()
    {
        m_canvasChildrenMap.clear();

        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;
            if (m_cm->HasComponent<UICanvas>(e)) continue; // Skip canvas entities themselves

            bool hasImage = m_cm->HasComponent<UIImage>(e);
            bool hasText = m_cm->HasComponent<UIText>(e);
            if (!hasImage && !hasText) continue;

            Entity canvasEntity = m_layoutEngine->FindOwningCanvas(e);
            if (canvasEntity == NO_ENTITY) continue;

            // Check active state once
            if (!IsActiveForUI(e, canvasEntity)) continue;

            auto& children = m_canvasChildrenMap[canvasEntity];
            if (hasImage) children.images.push_back(e);
            if (hasText)  children.texts.push_back(e);
        }
    }

    //=========================================================================
    // Rendering
    //=========================================================================


    void UIRenderSystem::SortEntitiesByZOrder(std::vector<Entity>& entities)
    {
        std::sort(entities.begin(), entities.end(),
            [this](Entity a, Entity b) {
                auto& rectA = m_cm->GetComponent<UIRectTransform>(a);
                auto& rectB = m_cm->GetComponent<UIRectTransform>(b);
                return rectA.z < rectB.z;
            });
    }

    void UIRenderSystem::RenderCanvasChildren(
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    )
    {
        (void)viewMatrix;
        (void)projMatrix;

        auto it = m_canvasChildrenMap.find(canvasEntity);
        if (it == m_canvasChildrenMap.end()) return;

        std::vector<Entity>& canvasChildren = it->second.images;

        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE && canvasChildren.size() > 1)
        {
            SortEntitiesByZOrder(canvasChildren);
        }

        for (Entity e : canvasChildren) {
            auto& img = m_cm->GetComponent<UIImage>(e);
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);

            // Accumulate parent transforms ONCE per entity (delegated to UILayoutEngine)
            AccumulatedTransform accumulated = m_layoutEngine->AccumulateParentTransforms(e, canvasEntity, canvas);

            // Use the pre-computed accumulated transform for both world matrix and world transform
            m_layoutEngine->UpdateWorldMatrixFromAccumulated(e, accumulated);
            WorldTransform worldTransform = m_layoutEngine->CalculateWorldTransformFromAccumulated(e, canvas, accumulated);

            // Cache the world rect for UIEventSystem to reuse
            rect.cachedWorldX = worldTransform.x;
            rect.cachedWorldY = worldTransform.y;
            rect.cachedWorldWidth = worldTransform.width;
            rect.cachedWorldHeight = worldTransform.height;
            rect.cachedWorldRotZ = worldTransform.accumulatedRotationZ;
            rect.cachedWorldScaleX = worldTransform.accumulatedScaleX;
            rect.cachedWorldScaleY = worldTransform.accumulatedScaleY;
            rect.worldRectCached = true;

            // Generate vertices using UIVertex2
            std::vector<NE::Graphics::UIVertex2> verticesV2;

            // Use white color for vertices (color is applied via uniform)
            Math::Vec4 whiteColor(1.0f, 1.0f, 1.0f, 1.0f);

            if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                // For world space, generate simple unit quad
                verticesV2 = NE::Graphics::UIImageMeshGenerator::GenerateVertices2(
                    img, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, whiteColor
                );
            }
            else {
                // For screen space, use world transform
                verticesV2 = NE::Graphics::UIImageMeshGenerator::GenerateVertices2(
                    img,
                    worldTransform.x, worldTransform.y, worldTransform.z,
                    worldTransform.width, worldTransform.height,
                    whiteColor
                );
            }

            if (!verticesV2.empty()) {
                // Compute scissor rect from parent RectMask2D chain
                auto scissor = ComputeScissorRect(e, canvasEntity, canvas);
                SubmitUIElement(e, canvas, img, rect, verticesV2, scissor);
            }
        }
    }

    //=========================================================================
    // Camera Utilities
    //=========================================================================

    bool UIRenderSystem::GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj)
    {
        auto* cam = NE::Graphics::GraphicsManager::GetEditorCamera();
        if (!cam) return false;

        outView = cam->GetViewMatrix();
        outProj = cam->GetProjectionMatrix();

        return true;
    }

    //=========================================================================
    // Text Rendering
    //=========================================================================



    void UIRenderSystem::RenderTextEntity(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    )
    {
        (void)viewMatrix;
        (void)projMatrix;

        if (!m_cm->HasComponent<UIText>(entity) || !m_cm->HasComponent<UIRectTransform>(entity)) return;

        auto& text = m_cm->GetComponent<UIText>(entity);
        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        // Skip if no text or font
        if (text.text.empty() || text.fontUUID.empty()) return;

        // Accumulate parent transforms ONCE for this text entity (delegated to UILayoutEngine)
        AccumulatedTransform accumulated = m_layoutEngine->AccumulateParentTransforms(entity, canvasEntity, canvas);

        // Update world matrix from accumulated (needed for world-space text submission)
        m_layoutEngine->UpdateWorldMatrixFromAccumulated(entity, accumulated);

        // Use the average scale for uniform font scaling (take the smaller to ensure text fits)
        float scaleFactorForFont = std::min(accumulated.scaleX, accumulated.scaleY);

        // Calculate effective font size based on accumulated scale
        float effectiveFontSize = text.fontSize * scaleFactorForFont;

        // Auto-scale: Calculate font size to fit within bounds if enabled
        if (text.autoScale) {
            // First get a font atlas at base size to calculate with
            auto tempAtlas = NE::Graphics::FontAtlasCache::GetInstance().GetOrCreate(
                text.fontUUID, text.fontSize
            );

            if (tempAtlas) {
                // Calculate what font size fits in the rect bounds
                effectiveFontSize = NE::Graphics::UITextMeshGenerator::CalculateFitFontSize(
                    text.text,
                    *tempAtlas,
                    rect.width * scaleFactorForFont,   // Available width
                    rect.height * scaleFactorForFont,  // Available height
                    text.fontSize * scaleFactorForFont,
                    text.minFontSize * scaleFactorForFont,
                    text.maxFontSize * scaleFactorForFont,
                    text.wordWrap
                );
            }
        }

        // Clamp to reasonable range to avoid tiny or huge font atlases
        effectiveFontSize = std::max(8.0f, std::min(effectiveFontSize, 256.0f));

        // Get or create font atlas at the effective scaled size
        if (text.fontUUID.empty()) {
            return;
        }

        auto fontAtlas = NE::Graphics::FontAtlasCache::GetInstance().GetOrCreate(
            text.fontUUID, effectiveFontSize
        );

        if (!fontAtlas) {
            std::cerr << "[UIRenderSystem::RenderTextEntity] ERROR: Failed to create font atlas!" << std::endl;
            return;
        }

        // Reuse the pre-computed accumulated transform (no second traversal)
        WorldTransform worldTransform =
            m_layoutEngine->CalculateWorldTransformFromAccumulated(entity, canvas, accumulated);

        // Cache the world rect for UIEventSystem to reuse
        rect.cachedWorldX = worldTransform.x;
        rect.cachedWorldY = worldTransform.y;
        rect.cachedWorldWidth = worldTransform.width;
        rect.cachedWorldHeight = worldTransform.height;
        rect.cachedWorldRotZ = worldTransform.accumulatedRotationZ;
        rect.cachedWorldScaleX = worldTransform.accumulatedScaleX;
        rect.cachedWorldScaleY = worldTransform.accumulatedScaleY;
        rect.worldRectCached = true;

        NE::Math::Vec3 curPos{ worldTransform.x, worldTransform.y, worldTransform.z };
        NE::Math::Vec2 curSize{ worldTransform.width, worldTransform.height };

        const float POS_EPS = 0.01f;
        const float SIZE_EPS = 0.01f;
        const float ROT_EPS = 0.001f;

        auto absf = [](float v) { return v < 0.0f ? -v : v; };

        bool transformChanged = !text.hasCachedTransform ||
            absf(text.cachedPos.x - curPos.x) > POS_EPS ||
            absf(text.cachedPos.y - curPos.y) > POS_EPS ||
            absf(text.cachedPos.z - curPos.z) > POS_EPS ||
            absf(text.cachedSize.x - curSize.x) > SIZE_EPS ||
            absf(text.cachedSize.y - curSize.y) > SIZE_EPS ||
            absf(text.cachedRotZ - worldTransform.accumulatedRotationZ) > ROT_EPS;

        // Check if text needs to be regenerated
        // Include effective font size in the check since scale can change
        bool needsRegen = text.isDirty ||
            text.cachedText != text.text ||
            std::abs(text.cachedFontSize - effectiveFontSize) > 0.1f ||
            text.fontAtlasHandle != fontAtlas->GetBindlessHandle() ||
            transformChanged;

        if (needsRegen) {
            // worldTransform already computed from accumulated above — no re-traversal needed

            auto result = NE::Graphics::UITextMeshGenerator::GenerateVertices(
                text.text,
                *fontAtlas,
                worldTransform.x,
                worldTransform.y,
                worldTransform.z,
                worldTransform.width,
                worldTransform.height,
                text.color,
                text.horizontalAlign,
                text.verticalAlign,
                text.wordWrap,
                effectiveFontSize  // Pass desired size for bucket scaling
            );

            // Directly copy vertices (same type now - UITextVertex with Vec3/Vec2/Vec4)
            text.cachedVertices = result.vertices;

            // Apply rotation
            float rot = worldTransform.accumulatedRotationZ * (3.1415926535f / 180.0f);
            if (std::abs(rot) > 0.0001f) {
                // Rotate around rect pivot (preferred). If you don't have pivot, use 0.5f, 0.5f.
                const float pivotX = worldTransform.x + worldTransform.width * rect.pivotX;
                const float pivotY = worldTransform.y + worldTransform.height * rect.pivotY;

                const float c = std::cos(rot);
                const float s = std::sin(rot);

                for (auto& v : text.cachedVertices) {
                    float lx = v.Position.x - pivotX;
                    float ly = v.Position.y - pivotY;
                    float rx = lx * c - ly * s;
                    float ry = lx * s + ly * c;
                    v.Position.x = rx + pivotX;
                    v.Position.y = ry + pivotY;
                }
            }

            text.cachedText = text.text;
            text.cachedFontSize = effectiveFontSize;
            text.fontAtlasHandle = fontAtlas->GetBindlessHandle();
            text.isDirty = false;
            text.cachedPos = curPos;
            text.cachedSize = curSize;
            text.cachedRotZ = worldTransform.accumulatedRotationZ;
            text.hasCachedTransform = true;
        }

        auto scissor = ComputeScissorRect(entity, canvasEntity, canvas);
        SubmitTextElement(entity, canvas, text, rect, text.cachedVertices, fontAtlas, scissor);
    }

    //=========================================================================
    // Scissor Clipping (RectMask2D)
    //=========================================================================

    std::optional<NE::Graphics::ScissorRect> UIRenderSystem::ComputeScissorRect(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas
    )
    {
        // Walk parent chain looking for UIRectTransform with enableMask
        Entity current = m_cm->HasComponent<Hierarchy>(entity)
            ? m_cm->GetComponent<Hierarchy>(entity).parent
            : NO_ENTITY;

        std::optional<NE::Graphics::ScissorRect> result;

        while (current != NO_ENTITY && current != canvasEntity) {
            if (m_cm->HasComponent<UIRectTransform>(current)) {
                auto& maskRect = m_cm->GetComponent<UIRectTransform>(current);
                if (maskRect.enableMask) {
                    auto wr = m_layoutEngine->GetWorldRect(current, canvasEntity, canvas);

                    // Apply mask padding (shrinks inward)
                    float maskX = wr.x + maskRect.maskPaddingLeft;
                    float maskY = wr.y + maskRect.maskPaddingTop;
                    float maskW = wr.width - maskRect.maskPaddingLeft - maskRect.maskPaddingRight;
                    float maskH = wr.height - maskRect.maskPaddingTop - maskRect.maskPaddingBottom;

                    if (maskW < 0.f) maskW = 0.f;
                    if (maskH < 0.f) maskH = 0.f;

                    // Convert from UI coords (top-left origin) to GL scissor coords (bottom-left origin)
                    int screenHeight = static_cast<int>(NE::Graphics::GraphicsManager::GetScreenHeight());
                    NE::Graphics::ScissorRect sr;
                    sr.x = static_cast<int>(maskX);
                    sr.y = screenHeight - static_cast<int>(maskY + maskH);
                    sr.width = static_cast<int>(maskW);
                    sr.height = static_cast<int>(maskH);

                    // Intersect with existing result (nested masks)
                    if (result.has_value()) {
                        int x1 = std::max(result->x, sr.x);
                        int y1 = std::max(result->y, sr.y);
                        int x2 = std::min(result->x + result->width, sr.x + sr.width);
                        int y2 = std::min(result->y + result->height, sr.y + sr.height);
                        result->x = x1;
                        result->y = y1;
                        result->width = std::max(0, x2 - x1);
                        result->height = std::max(0, y2 - y1);
                    } else {
                        result = sr;
                    }
                }
            }

            if (!m_cm->HasComponent<Hierarchy>(current)) break;
            current = m_cm->GetComponent<Hierarchy>(current).parent;
        }

        return result;
    }

} // namespace NE::ECS::Systems
