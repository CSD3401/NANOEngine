#include "pch.h"
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
#include <map>
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
        m_currentView.SetToIdentity();
        m_currentProj.SetToIdentity();
    }
    bool UIRenderSystem::IsActiveForUI(Entity entity, Entity canvasEntity) const
    {
        return UIUtil::IsActiveForUI(m_cm, entity, canvasEntity);
    }

    void UIRenderSystem::OnEntityAdded(Entity e)
    {
        if (m_cm->HasComponent<UIText>(e))
            m_textCache.emplace(e, UITextCache{});

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
        m_textCache.erase(e);
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
            m_sharedSpriteMaterial = std::make_shared<NE::Graphics::Material>(pipeline);
            m_sharedSpriteMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);

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
            m_sharedTextMaterial = std::make_shared<NE::Graphics::Material>(textPipeline);
            m_sharedTextMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);

        } catch (const std::exception& e) {
            std::cerr << "[UIRenderSystem] Error loading UI text material: " << e.what() << std::endl;
        }

        // Load world-space sprite shader
        try {
            auto worldShader = NE::Resource::ResourceManager::GetInstance()
                .LoadResource<NE::Graphics::OpenGL::GLShader>("neuiworld");
            if (worldShader) {
                NE::Graphics::PipelineSpecification worldSpec;
                worldSpec.shader = worldShader;
                worldSpec.EnableBlending = true;
                worldSpec.EnableDepthTest = true;   // WorldSpace participates in depth
                worldSpec.DepthWrite = false;        // Don't occlude 3D objects behind UI
                worldSpec.CullMode = 0;
                worldSpec.PolygonMode = 0x1B02;

                auto worldPipeline = std::make_shared<NE::Graphics::OpenGL::GLPipeline>(worldSpec, "UI_World");
                m_sharedWorldSpriteMaterial = std::make_shared<NE::Graphics::Material>(worldPipeline);
                m_sharedWorldSpriteMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
            } else {
                std::cerr << "[UIRenderSystem] Warning: Failed to load world sprite shader (neuiworld)" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[UIRenderSystem] Error loading world sprite material: " << e.what() << std::endl;
        }

        // Load world-space text shader
        try {
            auto worldTextShader = NE::Resource::ResourceManager::GetInstance()
                .LoadResource<NE::Graphics::OpenGL::GLShader>("neuiworldtext");
            if (worldTextShader) {
                NE::Graphics::PipelineSpecification worldTextSpec;
                worldTextSpec.shader = worldTextShader;
                worldTextSpec.EnableBlending = true;
                worldTextSpec.EnableDepthTest = true;
                worldTextSpec.DepthWrite = false;
                worldTextSpec.CullMode = 0;
                worldTextSpec.PolygonMode = 0x1B02;

                auto worldTextPipeline = std::make_shared<NE::Graphics::OpenGL::GLPipeline>(worldTextSpec, "UI_World_Text");
                m_sharedWorldTextMaterial = std::make_shared<NE::Graphics::Material>(worldTextPipeline);
                m_sharedWorldTextMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
            } else {
                std::cerr << "[UIRenderSystem] Warning: Failed to load world text shader (neuiworldtext)" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[UIRenderSystem] Error loading world text material: " << e.what() << std::endl;
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

    void UIRenderSystem::SubmitBatch(
        const UIBatch& batch,
        std::shared_ptr<NE::Graphics::Material> material,
        const std::optional<NE::Graphics::ScissorRect>& scissor,
        bool enableDepthTest
    )
    {
        if (batch.vertices.empty()) return;

        auto geomBuffer = AcquireGeometryBuffer(batch.vertices, batch.indices);

        NE::Graphics::DrawCommand cmd;
        cmd.mesh = geomBuffer;
        cmd.material = material;
        cmd.scissorRect = scissor;
        cmd.enableDepthTest = enableDepthTest;

        NE::Graphics::GraphicsManager::Submit(cmd);
    }

    void UIRenderSystem::Update(double)
    {
        // Reset geometry pool index for this frame
        m_geometryIndex = 0;

        // Reset batching diagnostics
        m_frameDrawCalls = 0;
        m_frameSpriteBatches = 0;
        m_frameTextBatches = 0;
        m_frameUIElements = 0;

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

        // Reset worldRectCached for all UI entities to ensure fresh computation each frame.
        // Without this, stale cached rects persist across frames and for inactive entities.
        {
            const auto& allEntities = GetEntities();
            for (Entity e : allEntities) {
                if (m_cm->HasComponent<UIRectTransform>(e)) {
                    auto& rect = m_cm->GetComponent<UIRectTransform>(e);
                    rect.worldRectCached = false;
                }
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

            // Cache the canvas entity's own world rect (used by editor gizmos)
            {
                auto& canvasRect = m_cm->GetComponent<UIRectTransform>(canvasEntity);
                if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                    // WorldSpace: canvas position/scale are in world coordinates
                    canvasRect.cachedWorldX = canvasRect.x;
                    canvasRect.cachedWorldY = canvasRect.y;
                    canvasRect.cachedWorldWidth = canvasRect.width * canvasRect.scaleX;
                    canvasRect.cachedWorldHeight = canvasRect.height * canvasRect.scaleY;
                } else {
                    // Screen-space: canvas covers the scaled reference area
                    float sw = canvasRect.width * canvas.scaleFactor;
                    float sh = canvasRect.height * canvas.scaleFactor;
                    canvasRect.cachedWorldX = canvasRect.x - sw * canvasRect.pivotX;
                    canvasRect.cachedWorldY = canvasRect.y - sh * canvasRect.pivotY;
                    canvasRect.cachedWorldWidth = sw;
                    canvasRect.cachedWorldHeight = sh;
                }
                canvasRect.cachedWorldScaleX = canvasRect.scaleX;
                canvasRect.cachedWorldScaleY = canvasRect.scaleY;
                canvasRect.cachedWorldRotZ = canvasRect.rotationZ;
                canvasRect.worldRectCached = true;
            }

            // Get camera matrices if needed
            Math::Mat4 viewMatrix, projMatrix;
            Math::Mat4* pView = nullptr;
            Math::Mat4* pProj = nullptr;
            bool hasValidCamera = true;

            if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA ||
                canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                if (GetCameraMatrices(viewMatrix, projMatrix)) {
                    pView = &viewMatrix;
                    pProj = &projMatrix;
                    // Store for WorldSpace element submission
                    m_currentView = viewMatrix;
                    m_currentProj = projMatrix;
                }
                else {
                    std::cerr << "[UIRenderSystem] WARNING: Canvas requires camera but none found!" << std::endl;
                    hasValidCamera = false;
                }
            }

            // Skip WorldSpace rendering if no camera found
            if (!hasValidCamera && canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                continue;
            }

            RenderCanvasChildren(canvasEntity, canvas, pView, pProj);

            // Render text entities with batching
            RenderCanvasTextChildren(canvasEntity, canvas);
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
            // Only reset when switching TO world space from another mode
            // On first init (deserialized), preserve serialized values
            if (renderModeChanged) {
                canvasRect.scaleX = DEFAULT_WORLD_SPACE_SCALE;
                canvasRect.scaleY = DEFAULT_WORLD_SPACE_SCALE;
                canvasRect.scaleZ = DEFAULT_WORLD_SPACE_SCALE;
                canvasRect.x = 0.f;
                canvasRect.y = 0.f;
                canvasRect.z = 0.f;
                canvasRect.rotationX = 0.f;
                canvasRect.rotationY = 0.f;
                canvasRect.rotationZ = 0.f;
            }
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

        // Shared material based on render mode (batching: one material per type, reused for all elements)
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            if (!m_sharedWorldSpriteMaterial) {
                std::cerr << "[UIRenderSystem::SubmitUIElement] ERROR: No world material available" << std::endl;
                return;
            }

            // Set view/proj matrices once per batch (same for all world elements in frame)
            m_sharedWorldSpriteMaterial->SetUniformMat4("uView", m_currentView);
            m_sharedWorldSpriteMaterial->SetUniformMat4("uProj", m_currentProj);
            m_sharedWorldSpriteMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
            m_sharedWorldSpriteMaterial->SetQueueOffset(canvas.sortingOrder);

            NE::Graphics::DrawCommand cmd;
            cmd.transform = rect.worldMatrix;
            cmd.mesh = geometryBuffer;
            cmd.material = m_sharedWorldSpriteMaterial;
            cmd.castsShadow = false;
            cmd.receivesShadow = false;
            cmd.boundsCenterWS = Math::Vec3(0.0f, 0.0f, 0.0f);
            cmd.boundsRadiusWs = 999999.0f;
            cmd.scissorRect = scissor;
            cmd.enableDepthTest = true;

            NE::Graphics::GraphicsManager::Submit(cmd);
        } else {
            if (!m_sharedSpriteMaterial) {
                std::cerr << "[UIRenderSystem::SubmitUIElement] ERROR: No material available (shader loading failed)" << std::endl;
                return;
            }

            // Set screen size uniform once per batch
            uint32_t screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
            uint32_t screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();
            m_sharedSpriteMaterial->SetUniformVec2("uScreenSize",
                Math::Vec2(static_cast<float>(screenWidth), static_cast<float>(screenHeight)));

            m_sharedSpriteMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
            m_sharedSpriteMaterial->SetQueueOffset(canvas.sortingOrder);

            NE::Graphics::DrawCommand cmd;
            cmd.transform = Math::Mat4(); // Identity — vertices pre-positioned in pixels
            cmd.mesh = geometryBuffer;
            cmd.material = m_sharedSpriteMaterial;
            cmd.castsShadow = false;
            cmd.receivesShadow = false;
            cmd.boundsCenterWS = Math::Vec3(0.0f, 0.0f, 0.0f);
            cmd.boundsRadiusWs = 999999.0f;
            cmd.scissorRect = scissor;

            NE::Graphics::GraphicsManager::Submit(cmd);
        }
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

        // WorldSpace uses MVP world text shader; screen-space uses pixel-to-NDC shader
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            if (!m_sharedWorldTextMaterial) {
                std::cerr << "[UIRenderSystem::SubmitTextElement] ERROR: No world text material!" << std::endl;
                return;
            }

            // Set view/proj matrices once per batch
            m_sharedWorldTextMaterial->SetUniformMat4("uView", m_currentView);
            m_sharedWorldTextMaterial->SetUniformMat4("uProj", m_currentProj);
            m_sharedWorldTextMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
            m_sharedWorldTextMaterial->SetQueueOffset(canvas.sortingOrder);

            NE::Graphics::DrawCommand cmd;
            cmd.transform = rect.worldMatrix;
            cmd.mesh = geometryBuffer;
            cmd.material = m_sharedWorldTextMaterial;
            cmd.castsShadow = false;
            cmd.receivesShadow = false;
            cmd.boundsRadiusWs = 999999.0f;
            cmd.scissorRect = scissor;
            cmd.enableDepthTest = true;

            NE::Graphics::GraphicsManager::Submit(cmd);
        } else {
            if (!m_sharedTextMaterial) {
                std::cerr << "[UIRenderSystem::SubmitTextElement] ERROR: No default text material!" << std::endl;
                return;
            }

            // Set screen size uniform once per batch
            m_sharedTextMaterial->SetUniformVec2("uScreenSize",
                Math::Vec2(
                    static_cast<float>(NE::Graphics::GraphicsManager::GetScreenWidth()),
                    static_cast<float>(NE::Graphics::GraphicsManager::GetScreenHeight())
                )
            );

            m_sharedTextMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
            m_sharedTextMaterial->SetQueueOffset(canvas.sortingOrder);

            NE::Graphics::DrawCommand cmd;
            cmd.transform = Math::Mat4(); // Identity — vertices pre-positioned in pixels
            cmd.mesh = geometryBuffer;
            cmd.material = m_sharedTextMaterial;
            cmd.castsShadow = false;
            cmd.receivesShadow = false;
            cmd.boundsRadiusWs = 999999.0f;
            cmd.scissorRect = scissor;

            NE::Graphics::GraphicsManager::Submit(cmd);
        }
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
            if (hasImage) {
                children.images.push_back(e);
                m_frameUIElements++;
            }
            if (hasText) {
                children.texts.push_back(e);
                m_frameUIElements++;
            }
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

        bool isWorldSpace = (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE);

        // WorldSpace: batch sprites by identical world matrix to reduce draw calls
        if (isWorldSpace) {
            std::map<WorldSpriteBatchKey, UIBatch> worldBatchMap;

            for (Entity e : canvasChildren) {
                auto& img = m_cm->GetComponent<UIImage>(e);
                auto& rect = m_cm->GetComponent<UIRectTransform>(e);

                AccumulatedTransform accumulated = m_layoutEngine->AccumulateParentTransforms(e, canvasEntity, canvas);
                rect.worldMatrixDirty = true;
                m_layoutEngine->UpdateWorldMatrixFromAccumulated(e, accumulated, true);
                WorldTransform worldTransform = m_layoutEngine->CalculateWorldTransformFromAccumulated(e, canvas, accumulated);

                rect.cachedWorldX = worldTransform.x;
                rect.cachedWorldY = worldTransform.y;
                rect.cachedWorldWidth = worldTransform.width;
                rect.cachedWorldHeight = worldTransform.height;
                rect.cachedWorldRotZ = worldTransform.accumulatedRotationZ;
                rect.cachedWorldScaleX = worldTransform.accumulatedScaleX;
                rect.cachedWorldScaleY = worldTransform.accumulatedScaleY;
                rect.worldRectCached = true;

                // Generate unit quad for world space
                std::vector<NE::Graphics::UIVertex2> verts =
                    NE::Graphics::UIImageMeshGenerator::GenerateVertices2(
                        img, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, img.color, img.bindlessHandle
                    );

                if (verts.empty()) continue;

                // Group sprites by identical world matrix
                WorldSpriteBatchKey key;
                std::memcpy(key.matBytes, rect.worldMatrix.a, sizeof(key.matBytes));

                UIBatch& batch = worldBatchMap[key];
                uint32_t base = static_cast<uint32_t>(batch.vertices.size());
                for (const auto& v : verts) batch.vertices.push_back(v);

                // verts.size() is always 4 (simple quad)
                uint32_t quadCount = static_cast<uint32_t>(verts.size()) / 4;
                for (uint32_t q = 0; q < quadCount; ++q) {
                    uint32_t b = base + q * 4;
                    batch.indices.push_back(b + 0);
                    batch.indices.push_back(b + 1);
                    batch.indices.push_back(b + 2);
                    batch.indices.push_back(b + 2);
                    batch.indices.push_back(b + 3);
                    batch.indices.push_back(b + 0);
                }
            }

            // Submit one draw call per unique world matrix
            for (auto& [key, batch] : worldBatchMap) {
                if (batch.vertices.empty()) continue;
                m_frameDrawCalls++;

                Math::Mat4 worldMat;
                std::memcpy(worldMat.a, key.matBytes, sizeof(worldMat.a));

                auto geomBuffer = AcquireGeometryBuffer(batch.vertices, batch.indices);
                if (!geomBuffer) continue;
                if (!m_sharedWorldSpriteMaterial) continue;

                m_sharedWorldSpriteMaterial->SetUniformMat4("uView", m_currentView);
                m_sharedWorldSpriteMaterial->SetUniformMat4("uProj", m_currentProj);
                m_sharedWorldSpriteMaterial->SetUniformMat4("uModel", worldMat);
                m_sharedWorldSpriteMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
                m_sharedWorldSpriteMaterial->SetQueueOffset(canvas.sortingOrder);

                NE::Graphics::DrawCommand cmd;
                cmd.transform = worldMat;
                cmd.mesh = geomBuffer;
                cmd.material = m_sharedWorldSpriteMaterial;
                cmd.castsShadow = false;
                cmd.receivesShadow = false;
                cmd.boundsCenterWS = Math::Vec3(0.0f, 0.0f, 0.0f);
                cmd.boundsRadiusWs = 999999.0f;
                cmd.scissorRect = std::nullopt;
                cmd.enableDepthTest = true;

                NE::Graphics::GraphicsManager::Submit(cmd);
            }
            return;
        }

        // ScreenSpace: Batch accumulation: group elements by batch key to reduce draw calls
        std::map<UIBatchKey, UIBatch> batchMap;

        for (Entity e : canvasChildren) {
            auto& img = m_cm->GetComponent<UIImage>(e);
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);

            AccumulatedTransform accumulated = m_layoutEngine->AccumulateParentTransforms(e, canvasEntity, canvas);
            rect.worldMatrixDirty = true;
            m_layoutEngine->UpdateWorldMatrixFromAccumulated(e, accumulated, false);
            WorldTransform worldTransform = m_layoutEngine->CalculateWorldTransformFromAccumulated(e, canvas, accumulated);

            rect.cachedWorldX = worldTransform.x;
            rect.cachedWorldY = worldTransform.y;
            rect.cachedWorldWidth = worldTransform.width;
            rect.cachedWorldHeight = worldTransform.height;
            rect.cachedWorldRotZ = worldTransform.accumulatedRotationZ;
            rect.cachedWorldScaleX = worldTransform.accumulatedScaleX;
            rect.cachedWorldScaleY = worldTransform.accumulatedScaleY;
            rect.worldRectCached = true;

            // Generate screen-space vertices
            std::vector<NE::Graphics::UIVertex2> verticesV2 =
                NE::Graphics::UIImageMeshGenerator::GenerateVertices2(
                    img,
                    worldTransform.x, worldTransform.y, worldTransform.z,
                    worldTransform.width, worldTransform.height,
                    img.color, img.bindlessHandle
                );

            if (!verticesV2.empty()) {
                std::optional<NE::Graphics::ScissorRect> scissor = ComputeScissorRect(e, canvasEntity, canvas);

                UIBatchKey key;
                key.isText = false;
                key.isWorldSpace = false;
                key.enableDepthTest = false;
                key.scissorRect = scissor;
                key.sortingOrder = rect.z + canvas.sortingOrder * 1000.0f;

                UIBatch& batch = batchMap[key];
                uint32_t baseVertex = static_cast<uint32_t>(batch.vertices.size());

                for (const auto& v : verticesV2) {
                    batch.vertices.push_back(v);
                }

                uint32_t quadCount = static_cast<uint32_t>(verticesV2.size()) / 4;
                for (uint32_t i = 0; i < quadCount; ++i) {
                    uint32_t base = baseVertex + i * 4;
                    batch.indices.push_back(base + 0);
                    batch.indices.push_back(base + 1);
                    batch.indices.push_back(base + 2);
                    batch.indices.push_back(base + 2);
                    batch.indices.push_back(base + 3);
                    batch.indices.push_back(base + 0);
                }
            }
        }

        // Submit screen-space batches
        m_frameSpriteBatches += batchMap.size();
        for (auto& [key, batch] : batchMap) {
            if (batch.vertices.empty()) continue;
            m_frameDrawCalls++;

            auto geometryBuffer = AcquireGeometryBuffer(batch.vertices, batch.indices);
            if (!geometryBuffer) continue;

            if (!m_sharedSpriteMaterial) continue;
            uint32_t screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
            uint32_t screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();
            m_sharedSpriteMaterial->SetUniformVec2("uScreenSize",
                Math::Vec2(static_cast<float>(screenWidth), static_cast<float>(screenHeight)));
            m_sharedSpriteMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
            m_sharedSpriteMaterial->SetQueueOffset(canvas.sortingOrder);

            NE::Graphics::DrawCommand cmd;
            cmd.transform = Math::Mat4();  // Identity — vertices pre-positioned in pixels
            cmd.mesh = geometryBuffer;
            cmd.material = m_sharedSpriteMaterial;
            cmd.scissorRect = key.scissorRect;
            cmd.castsShadow = false;
            cmd.receivesShadow = false;
            cmd.boundsRadiusWs = 999999.0f;

            NE::Graphics::GraphicsManager::Submit(cmd);
        }
    }

    void UIRenderSystem::RenderCanvasTextChildren(
        Entity canvasEntity,
        const UICanvas& canvas
    )
    {
        auto it = m_canvasChildrenMap.find(canvasEntity);
        if (it == m_canvasChildrenMap.end()) return;

        std::vector<Entity>& textChildren = it->second.texts;
        bool isWorldSpace = (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE);

        // Sort text by Z order for world space
        if (isWorldSpace && textChildren.size() > 1) {
            SortEntitiesByZOrder(textChildren);
        }

        // Helper lambda for vertex generation and caching logic
        auto GenerateTextVertices = [&](Entity entity) -> std::vector<NE::Graphics::UIVertex2> {
            auto& text = m_cm->GetComponent<UIText>(entity);
            auto& rect = m_cm->GetComponent<UIRectTransform>(entity);
            auto& cache = m_textCache[entity];

            AccumulatedTransform accumulated = m_layoutEngine->AccumulateParentTransforms(entity, canvasEntity, canvas);
            rect.worldMatrixDirty = true;
            m_layoutEngine->UpdateWorldMatrixFromAccumulated(entity, accumulated, isWorldSpace);
            WorldTransform worldTransform = m_layoutEngine->CalculateWorldTransformFromAccumulated(entity, canvas, accumulated);

            rect.cachedWorldX = worldTransform.x;
            rect.cachedWorldY = worldTransform.y;
            rect.cachedWorldWidth = worldTransform.width;
            rect.cachedWorldHeight = worldTransform.height;
            rect.cachedWorldRotZ = worldTransform.accumulatedRotationZ;
            rect.cachedWorldScaleX = worldTransform.accumulatedScaleX;
            rect.cachedWorldScaleY = worldTransform.accumulatedScaleY;
            rect.worldRectCached = true;

            float scaleFactorForFont = isWorldSpace ? 1.0f : std::min(accumulated.scaleX, accumulated.scaleY);
            float effectiveFontSize = text.fontSize * scaleFactorForFont;

            if (text.autoScale) {
                auto tempAtlas = NE::Graphics::FontAtlasCache::GetInstance().GetOrCreate(
                    text.fontUUID, text.fontSize
                );
                if (tempAtlas) {
                    float fitWidth = isWorldSpace ? rect.width : rect.width * scaleFactorForFont;
                    float fitHeight = isWorldSpace ? rect.height : rect.height * scaleFactorForFont;
                    effectiveFontSize = NE::Graphics::UITextMeshGenerator::CalculateFitFontSize(
                        text.text, *tempAtlas,
                        fitWidth, fitHeight,
                        text.fontSize * scaleFactorForFont,
                        text.minFontSize * scaleFactorForFont,
                        text.maxFontSize * scaleFactorForFont,
                        text.wordWrap
                    );
                }
            }

            effectiveFontSize = std::max(8.0f, std::min(effectiveFontSize, 256.0f));

            auto fontAtlas = NE::Graphics::FontAtlasCache::GetInstance().GetOrCreate(
                text.fontUUID, effectiveFontSize
            );
            if (!fontAtlas) return {};

            const float POS_EPS = 0.01f;
            const float SIZE_EPS = 0.01f;
            const float ROT_EPS = 0.001f;
            auto absf = [](float v) { return v < 0.0f ? -v : v; };

            NE::Math::Vec3 curPos{
                isWorldSpace ? 0.0f : worldTransform.x,
                isWorldSpace ? 0.0f : worldTransform.y,
                isWorldSpace ? 0.0f : worldTransform.z
            };
            NE::Math::Vec2 curSize{
                isWorldSpace ? rect.width : worldTransform.width,
                isWorldSpace ? rect.height : worldTransform.height
            };

            bool transformChanged = !cache.hasCachedTransform ||
                absf(cache.cachedPos.x - curPos.x) > POS_EPS ||
                absf(cache.cachedPos.y - curPos.y) > POS_EPS ||
                absf(cache.cachedPos.z - curPos.z) > POS_EPS ||
                absf(cache.cachedSize.x - curSize.x) > SIZE_EPS ||
                absf(cache.cachedSize.y - curSize.y) > SIZE_EPS ||
                absf(cache.cachedRotZ - worldTransform.accumulatedRotationZ) > ROT_EPS;

            bool needsRegen = text.isDirty ||
                cache.cachedText != text.text ||
                std::abs(cache.cachedFontSize - effectiveFontSize) > 0.1f ||
                cache.fontAtlasHandle != fontAtlas->GetBindlessHandle() ||
                transformChanged;

            std::vector<NE::Graphics::UIVertex2> verticesV2;
            if (needsRegen) {
                float textX = isWorldSpace ? 0.0f : worldTransform.x;
                float textY = isWorldSpace ? 0.0f : worldTransform.y;
                float textZ = isWorldSpace ? 0.0f : worldTransform.z;
                float textW = isWorldSpace ? rect.width : worldTransform.width;
                float textH = isWorldSpace ? rect.height : worldTransform.height;

                auto result = NE::Graphics::UITextMeshGenerator::GenerateVertices(
                    text.text, *fontAtlas,
                    textX, textY, textZ, textW, textH,
                    text.color,
                    text.horizontalAlign,
                    text.verticalAlign,
                    text.wordWrap,
                    effectiveFontSize,
                    fontAtlas->GetBindlessHandle()
                );
                verticesV2 = result.vertices;

                if (!isWorldSpace && std::abs(worldTransform.accumulatedRotationZ) > 0.0001f) {
                    float rot = worldTransform.accumulatedRotationZ * (3.1415926535f / 180.0f);
                    const float pivotX = worldTransform.x + worldTransform.width * rect.pivotX;
                    const float pivotY = worldTransform.y + worldTransform.height * rect.pivotY;
                    const float c = std::cos(rot);
                    const float s = std::sin(rot);

                    for (auto& v : verticesV2) {
                        float rx = v.Position.x - pivotX;
                        float ry = v.Position.y - pivotY;
                        v.Position.x = pivotX + (rx * c - ry * s);
                        v.Position.y = pivotY + (rx * s + ry * c);
                    }
                }

                cache.cachedVertices = verticesV2;
                cache.cachedText = text.text;
                cache.cachedFontSize = effectiveFontSize;
                cache.fontAtlasHandle = fontAtlas->GetBindlessHandle();
                text.isDirty = false;
                cache.cachedPos = curPos;
                cache.cachedSize = curSize;
                cache.cachedRotZ = worldTransform.accumulatedRotationZ;
                cache.hasCachedTransform = true;
                text.cachedSize = curSize;
            } else {
                verticesV2 = cache.cachedVertices;
            }

            return verticesV2;
        };

        // WorldSpace: submit each text individually with its own transform
        if (isWorldSpace) {
            for (Entity entity : textChildren) {
                if (!m_cm->HasComponent<UIText>(entity) || !m_cm->HasComponent<UIRectTransform>(entity)) continue;

                auto& text = m_cm->GetComponent<UIText>(entity);
                auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

                if (text.text.empty() || text.fontUUID.empty()) continue;

                std::vector<NE::Graphics::UIVertex2> verticesV2 = GenerateTextVertices(entity);
                if (verticesV2.empty()) continue;

                m_frameDrawCalls++;
                auto geometryBuffer = CreateDynamicTextGeometry(verticesV2);
                if (!geometryBuffer) continue;

                if (!m_sharedWorldTextMaterial) continue;
                float smoothing = 0.07f;
                m_sharedWorldTextMaterial->SetUniformMat4("uView", m_currentView);
                m_sharedWorldTextMaterial->SetUniformMat4("uProj", m_currentProj);
                m_sharedWorldTextMaterial->SetUniformMat4("uModel", rect.worldMatrix);
                m_sharedWorldTextMaterial->SetUniformFloat("uFontSmoothing", smoothing);
                m_sharedWorldTextMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
                m_sharedWorldTextMaterial->SetQueueOffset(canvas.sortingOrder);

                NE::Graphics::DrawCommand cmd;
                cmd.transform = rect.worldMatrix;  // Apply element's world matrix
                cmd.mesh = geometryBuffer;
                cmd.material = m_sharedWorldTextMaterial;
                cmd.enableDepthTest = true;
                cmd.castsShadow = false;
                cmd.receivesShadow = false;
                cmd.boundsRadiusWs = 999999.0f;

                NE::Graphics::GraphicsManager::Submit(cmd);
            }
            return;
        }

        // ScreenSpace: batch text by font atlas
        std::map<UIBatchKey, UIBatch> batchMap;

        for (Entity entity : textChildren) {
            if (!m_cm->HasComponent<UIText>(entity) || !m_cm->HasComponent<UIRectTransform>(entity)) continue;

            auto& text = m_cm->GetComponent<UIText>(entity);
            auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

            if (text.text.empty() || text.fontUUID.empty()) continue;

            std::vector<NE::Graphics::UIVertex2> verticesV2 = GenerateTextVertices(entity);
            if (verticesV2.empty()) continue;

            std::optional<NE::Graphics::ScissorRect> scissor = ComputeScissorRect(entity, canvasEntity, canvas);

            UIBatchKey key;
            key.isText = true;
            key.isWorldSpace = false;
            key.enableDepthTest = false;
            key.scissorRect = scissor;
            key.sortingOrder = rect.z + canvas.sortingOrder * 1000.0f;

            UIBatch& batch = batchMap[key];
            uint32_t baseVertex = static_cast<uint32_t>(batch.vertices.size());

            for (const auto& v : verticesV2) {
                batch.vertices.push_back(v);
            }

            for (uint32_t i = 0; i < static_cast<uint32_t>(verticesV2.size()); ++i) {
                batch.indices.push_back(baseVertex + i);
            }
        }

        // Submit screen-space text batches
        m_frameTextBatches += batchMap.size();
        for (auto& [key, batch] : batchMap) {
            if (batch.vertices.empty()) continue;
            m_frameDrawCalls++;

            auto geometryBuffer = AcquireGeometryBuffer(batch.vertices, batch.indices);
            if (!geometryBuffer) continue;

            if (!m_sharedTextMaterial) continue;
            float smoothing = 0.07f;
            uint32_t screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
            uint32_t screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();
            m_sharedTextMaterial->SetUniformVec2("uScreenSize",
                Math::Vec2(static_cast<float>(screenWidth), static_cast<float>(screenHeight)));
            m_sharedTextMaterial->SetUniformFloat("uFontSmoothing", smoothing);
            m_sharedTextMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
            m_sharedTextMaterial->SetQueueOffset(canvas.sortingOrder);

            NE::Graphics::DrawCommand cmd;
            cmd.transform = Math::Mat4();
            cmd.mesh = geometryBuffer;
            cmd.material = m_sharedTextMaterial;
            cmd.scissorRect = key.scissorRect;
            cmd.castsShadow = false;
            cmd.receivesShadow = false;
            cmd.boundsRadiusWs = 999999.0f;

            NE::Graphics::GraphicsManager::Submit(cmd);
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
        auto& cache = m_textCache[entity];

        // Skip if no text or font
        if (text.text.empty() || text.fontUUID.empty()) return;

        bool isWorldSpace = (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE);

        // Accumulate parent transforms ONCE for this text entity (delegated to UILayoutEngine)
        AccumulatedTransform accumulated = m_layoutEngine->AccumulateParentTransforms(entity, canvasEntity, canvas);

        // Force dirty so the world matrix is always rebuilt from fresh accumulated data.
        rect.worldMatrixDirty = true;

        // Update world matrix from accumulated (needed for world-space text submission)
        m_layoutEngine->UpdateWorldMatrixFromAccumulated(entity, accumulated, isWorldSpace);

        // For WorldSpace, use fontSize directly — world matrix handles scaling
        // For screen-space, scale font by accumulated parent scale
        float scaleFactorForFont = isWorldSpace ? 1.0f : std::min(accumulated.scaleX, accumulated.scaleY);

        // Calculate effective font size based on accumulated scale
        float effectiveFontSize = text.fontSize * scaleFactorForFont;

        // Auto-scale: Calculate font size to fit within bounds if enabled
        if (text.autoScale) {
            auto tempAtlas = NE::Graphics::FontAtlasCache::GetInstance().GetOrCreate(
                text.fontUUID, text.fontSize
            );

            if (tempAtlas) {
                float fitWidth = isWorldSpace ? rect.width : rect.width * scaleFactorForFont;
                float fitHeight = isWorldSpace ? rect.height : rect.height * scaleFactorForFont;
                effectiveFontSize = NE::Graphics::UITextMeshGenerator::CalculateFitFontSize(
                    text.text,
                    *tempAtlas,
                    fitWidth,
                    fitHeight,
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

        // For WorldSpace, generate text at local coordinates (world matrix handles positioning)
        // For screen-space, generate text at world transform coordinates
        float textX, textY, textZ, textW, textH;
        if (isWorldSpace) {
            textX = 0.0f;
            textY = 0.0f;
            textZ = 0.0f;
            textW = rect.width;
            textH = rect.height;
        } else {
            textX = worldTransform.x;
            textY = worldTransform.y;
            textZ = worldTransform.z;
            textW = worldTransform.width;
            textH = worldTransform.height;
        }

        NE::Math::Vec3 curPos{ textX, textY, textZ };
        NE::Math::Vec2 curSize{ textW, textH };

        const float POS_EPS = 0.01f;
        const float SIZE_EPS = 0.01f;
        const float ROT_EPS = 0.001f;

        auto absf = [](float v) { return v < 0.0f ? -v : v; };

        bool transformChanged = !cache.hasCachedTransform ||
            absf(cache.cachedPos.x - curPos.x) > POS_EPS ||
            absf(cache.cachedPos.y - curPos.y) > POS_EPS ||
            absf(cache.cachedPos.z - curPos.z) > POS_EPS ||
            absf(cache.cachedSize.x - curSize.x) > SIZE_EPS ||
            absf(cache.cachedSize.y - curSize.y) > SIZE_EPS ||
            absf(cache.cachedRotZ - worldTransform.accumulatedRotationZ) > ROT_EPS;

        // Check if text needs to be regenerated
        bool needsRegen = text.isDirty ||
            cache.cachedText != text.text ||
            std::abs(cache.cachedFontSize - effectiveFontSize) > 0.1f ||
            cache.fontAtlasHandle != fontAtlas->GetBindlessHandle() ||
            transformChanged;

        if (needsRegen) {
            auto result = NE::Graphics::UITextMeshGenerator::GenerateVertices(
                text.text,
                *fontAtlas,
                textX,
                textY,
                textZ,
                textW,
                textH,
                text.color,
                text.horizontalAlign,
                text.verticalAlign,
                text.wordWrap,
                effectiveFontSize,
                fontAtlas->GetBindlessHandle()  // Embed font atlas handle in vertices
            );

            cache.cachedVertices = result.vertices;

            // For screen-space, apply manual rotation; for WorldSpace, world matrix handles it
            if (!isWorldSpace) {
                float rot = worldTransform.accumulatedRotationZ * (3.1415926535f / 180.0f);
                if (std::abs(rot) > 0.0001f) {
                    const float pivotX = worldTransform.x + worldTransform.width * rect.pivotX;
                    const float pivotY = worldTransform.y + worldTransform.height * rect.pivotY;

                    const float c = std::cos(rot);
                    const float s = std::sin(rot);

                    for (auto& v : cache.cachedVertices) {
                        float lx = v.Position.x - pivotX;
                        float ly = v.Position.y - pivotY;
                        float rx = lx * c - ly * s;
                        float ry = lx * s + ly * c;
                        v.Position.x = rx + pivotX;
                        v.Position.y = ry + pivotY;
                    }
                }
            }

            cache.cachedText = text.text;
            cache.cachedFontSize = effectiveFontSize;
            cache.fontAtlasHandle = fontAtlas->GetBindlessHandle();
            text.isDirty = false;
            cache.cachedPos = curPos;
            cache.cachedSize = curSize;
            cache.cachedRotZ = worldTransform.accumulatedRotationZ;
            cache.hasCachedTransform = true;
            text.cachedSize = curSize;
        }

        // Skip scissor for WorldSpace (screen-space operation)
        std::optional<NE::Graphics::ScissorRect> scissor;
        if (!isWorldSpace) {
            scissor = ComputeScissorRect(entity, canvasEntity, canvas);
        }
        SubmitTextElement(entity, canvas, text, rect, cache.cachedVertices, fontAtlas, scissor);
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
