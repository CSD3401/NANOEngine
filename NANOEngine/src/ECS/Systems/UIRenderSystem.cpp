#include "UIRenderSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
#include "../Components/UIText.hpp"
#include "../../Graphics/Core/UIDrawCommand.hpp"
#include "../../Graphics/Core/UIRenderer.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Graphics/Core/EditorCamera.hpp"
#include "../../Graphics/Core/UITextMeshGenerator.hpp"
#include "../../Graphics/OpenGL/GLVertexBuffer.hpp"
#include "../../Graphics/OpenGL/GLIndexBuffer.hpp"
#include "../../Graphics/OpenGL/GLGeometryBuffer.hpp"
#include "../../Graphics/OpenGL/GLPipeline.hpp"
#include "../../Graphics/OpenGL/GLShader.hpp"
#include "../../Graphics/Core/Material.hpp"
#include "../../Graphics/Core/DrawCommand.hpp"
#include "../../Graphics/Core/Vertex.hpp"
#include "../../Graphics/Core/PipelineCache.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include <glad/glad.h>
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace NE::ECS;
using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    //=========================================================================
    // Constants
    //=========================================================================

    static constexpr float PI = 3.14159265358979f;
    static constexpr float ROTATION_EPSILON = 0.001f;

    // Default anchor at center for Screen Space modes
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
    }

    void UIRenderSystem::Init()
    {
        // Create default UI material programmatically
        // NOTE: UI shaders are not in the resource system, so we create them from hardcoded strings
        // This matches the old UIRenderer approach
        try {
            // Create UI shader from hardcoded source (same as UIRenderer)
            const char* uiVertexShaderSource = R"(#version 460 core
                // Per-vertex attributes
                layout(location = 0) in vec3 aPos;
                layout(location = 1) in vec3 aNormal;
                layout(location = 2) in vec2 aTexCoord;

                // Per-instance attributes (required for instanced rendering, even if unused)
                layout(location = 5) in vec4 aInstanceModel0;
                layout(location = 6) in vec4 aInstanceModel1;
                layout(location = 7) in vec4 aInstanceModel2;
                layout(location = 8) in vec4 aInstanceModel3;
                layout(location = 9) in vec3 aInstanceIdRGB;

                out vec2 vUV;

                uniform vec2 uScreenSize;

                void main() {
                    // Convert pixel coordinates to NDC (-1 to 1)
                    // Note: We ignore the instance model matrix since UI is already in screen space
                    float ndcX = (aPos.x / uScreenSize.x) * 2.0 - 1.0;
                    float ndcY = 1.0 - (aPos.y / uScreenSize.y) * 2.0;

                    gl_Position = vec4(ndcX, ndcY, aPos.z, 1.0);
                    vUV = aTexCoord;
                }
            )";

            const char* uiFragmentShaderSource = R"(#version 460 core
                in vec2 vUV;
                out vec4 FragColor;

                uniform vec4 uColor;
                uniform int uHasTexture;
                uniform sampler2D uTexture;

                void main() {
                    if (uHasTexture == 1) {
                        FragColor = texture(uTexture, vUV) * uColor;
                    } else {
                        FragColor = uColor;
                    }
                }
            )";

            // Manually compile and link the shader
            GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertexShader, 1, &uiVertexShaderSource, nullptr);
            glCompileShader(vertexShader);

            // Check vertex shader compilation
            GLint vertexSuccess = 0;
            glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexSuccess);
            if (!vertexSuccess) {
                char infoLog[512];
                glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
                std::cerr << "[UIRenderSystem] ERROR: Vertex shader compilation failed:\n" << infoLog << std::endl;
                glDeleteShader(vertexShader);
                return;
            }

            GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragmentShader, 1, &uiFragmentShaderSource, nullptr);
            glCompileShader(fragmentShader);

            // Check fragment shader compilation
            GLint fragmentSuccess = 0;
            glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentSuccess);
            if (!fragmentSuccess) {
                char infoLog[512];
                glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
                std::cerr << "[UIRenderSystem] ERROR: Fragment shader compilation failed:\n" << infoLog << std::endl;
                glDeleteShader(vertexShader);
                glDeleteShader(fragmentShader);
                return;
            }

            // Link shaders
            GLuint shaderProgram = glCreateProgram();
            glAttachShader(shaderProgram, vertexShader);
            glAttachShader(shaderProgram, fragmentShader);
            glLinkProgram(shaderProgram);

            // Check linking
            GLint linkSuccess = 0;
            glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkSuccess);
            if (!linkSuccess) {
                char infoLog[512];
                glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
                std::cerr << "[UIRenderSystem] ERROR: Shader program linking failed:\n" << infoLog << std::endl;
                glDeleteShader(vertexShader);
                glDeleteShader(fragmentShader);
                glDeleteProgram(shaderProgram);
                return;
            }

            // Clean up shader objects (no longer needed after linking)
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            std::cout << "[UIRenderSystem] UI shader compiled and linked successfully (program ID: " << shaderProgram << ")" << std::endl;

            // Create GLShader with the compiled program
            auto uiShader = std::make_shared<NE::Graphics::OpenGL::GLShader>(shaderProgram);

            NE::Graphics::PipelineSpecification spec;
            spec.shader = uiShader;
            spec.EnableBlending = true;
            spec.EnableDepthTest = false;
            spec.DepthWrite = false;
            spec.CullMode = GL_NONE;
            spec.PolygonMode = GL_FILL;

            auto pipeline = std::make_shared<NE::Graphics::OpenGL::GLPipeline>(spec, "UI_Programmatic");
            m_defaultUIMaterial = std::make_shared<NE::Graphics::Material>(pipeline);
            m_defaultUIMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);

            std::cout << "[UIRenderSystem] Created programmatic UI material with OVERLAY queue" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[UIRenderSystem] Error creating programmatic UI material: " << e.what() << std::endl;
        }

        // Use same material for text
        m_defaultTextMaterial = m_defaultUIMaterial;

        // Load per-entity textures and materials
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIImage>(e)) continue;

            auto& img = m_cm->GetComponent<UIImage>(e);

            if (!img.textureUUID.empty() && img.bindlessHandle == 0) {
                auto texture = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
                if (texture) {
                    img.bindlessHandle = texture->GetBindlessHandle();
                }
            }

            if (!img.materialUUID.empty() && !img.material) {
                img.material = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::Material>(img.materialUUID);
            }
        }
    }

    void UIRenderSystem::Update(double) 
    {
        const auto& entities = GetEntities();

        std::vector<std::pair<int, Entity>> canvases;

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UICanvas>(e)) continue;

            auto& canvas = m_cm->GetComponent<UICanvas>(e);
            if (canvas.isActive) {
                canvases.push_back({ canvas.sortingOrder, e });
            }
        }

        std::sort(canvases.begin(), canvases.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& [order, canvasEntity] : canvases) {
            auto& canvas = m_cm->GetComponent<UICanvas>(canvasEntity);

            // Calculate scale factor based on scale mode
            canvas.scaleFactor = CalculateScaleFactor(canvas);

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

            // Render text entities
            std::vector<Entity> textChildren = CollectTextChildren(canvasEntity);
            for (Entity e : textChildren) {
                RenderTextEntity(e, canvasEntity, canvas, pView, pProj);
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
                float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
                float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();
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
            float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
            float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

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

    //=========================================================================
    // Transform Hierarchy Functions
    //=========================================================================

    std::vector<Entity> UIRenderSystem::BuildParentChain(Entity entity, Entity canvasEntity, UICanvas::RenderMode renderMode) 
    {
        std::vector<Entity> chain;
        Entity current = entity;

        while (current != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(current)) {
            if (current == canvasEntity && renderMode != UICanvas::RenderMode::WORLD_SPACE) {
                break;
            }
            chain.push_back(current);
            current = m_cm->GetComponent<UIRectTransform>(current).parent;
        }

        std::reverse(chain.begin(), chain.end());
        return chain;
    }

    UIRenderSystem::AccumulatedTransform UIRenderSystem::AccumulateParentTransforms(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas
    ) 
    {
        AccumulatedTransform result;

        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return result;
        }

        //=====================================================================
        // WORLD SPACE: Original logic - NO center anchor offset
        // Canvas transform IS included (position, rotation, scale all apply)
        //=====================================================================
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            std::vector<Entity> chain = BuildParentChain(entity, canvasEntity, canvas.renderMode);

            for (size_t i = 0; i < chain.size(); ++i) {
                Entity current = chain[i];
                auto& rect = m_cm->GetComponent<UIRectTransform>(current);

                // accumulate scale
                result.scaleX *= rect.scaleX;
                result.scaleY *= rect.scaleY;
                result.scaleZ *= rect.scaleZ;

                // accumulate rotation
                result.rotationX += rect.rotationX;
                result.rotationY += rect.rotationY;
                result.rotationZ += rect.rotationZ;

                // accumulate position
                result.posX += rect.x;
                result.posY += rect.y;
                result.posZ += rect.z;
            }

            return result;
        }

        //=====================================================================
        // SCREEN SPACE: Apply center anchor and scaleFactor
        // Canvas transform is IGNORED (locked like Unity)
        //=====================================================================
        result.scaleX = canvas.scaleFactor;
        result.scaleY = canvas.scaleFactor;

        std::vector<Entity> chain = BuildParentChain(entity, canvasEntity, canvas.renderMode);

        for (size_t i = 0; i < chain.size(); ++i) {
            Entity current = chain[i];
            auto& rect = m_cm->GetComponent<UIRectTransform>(current);
            bool isTarget = (current == entity);

            result.scaleX *= rect.scaleX;
            result.scaleY *= rect.scaleY;
            result.scaleZ *= rect.scaleZ;
            result.rotationZ += rect.rotationZ;

            // Get parent dimensions for anchor calculation
            float parentWidth = 0.f;
            float parentHeight = 0.f;

            if (i > 0) {
                Entity parentEntity = chain[i - 1];
                auto& parentRect = m_cm->GetComponent<UIRectTransform>(parentEntity);
                parentWidth = parentRect.width;
                parentHeight = parentRect.height;
            }
            else {
                // Direct child of canvas - use screen dimensions in reference coordinates
                parentWidth = NE::Graphics::GraphicsManager::GetScreenWidth() / canvas.scaleFactor;
                parentHeight = NE::Graphics::GraphicsManager::GetScreenHeight() / canvas.scaleFactor;
            }

            // Center anchor offset
            float anchorX = parentWidth * DEFAULT_ANCHOR_X;
            float anchorY = parentHeight * DEFAULT_ANCHOR_Y;

            if (isTarget) {
                // Use scaled dimensions for pivot offset calculation
                float scaledWidth = rect.width * result.scaleX;
                float scaledHeight = rect.height * result.scaleY;

                float localX = anchorX + rect.x - scaledWidth * rect.pivotX;
                float localY = anchorY + rect.y - scaledHeight * rect.pivotY;

                float parentRotation = result.rotationZ - rect.rotationZ;
                if (std::abs(parentRotation) > ROTATION_EPSILON) {
                    float rad = parentRotation * PI / 180.0f;
                    float cosR = std::cos(rad);
                    float sinR = std::sin(rad);
                    float rotatedX = localX * cosR - localY * sinR;
                    float rotatedY = localX * sinR + localY * cosR;
                    localX = rotatedX;
                    localY = rotatedY;
                }

                float parentScaleX = result.scaleX / rect.scaleX;
                float parentScaleY = result.scaleY / rect.scaleY;

                result.posX += localX * parentScaleX;
                result.posY += localY * parentScaleY;
                result.posZ += rect.z;
            }
            else {
                result.posX += anchorX + rect.x;
                result.posY += anchorY + rect.y;
                result.posZ += rect.z;
            }
        }

        return result;
    }

    //=========================================================================
    // World Transform Calculation
    //=========================================================================

    UIRenderSystem::WorldTransform UIRenderSystem::CalculateWorldTransform(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) 
    {
        WorldTransform result;

        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return result;
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);
        AccumulatedTransform accumulated = AccumulateParentTransforms(entity, canvasEntity, canvas);

        result.x = accumulated.posX;
        result.y = accumulated.posY;
        result.z = accumulated.posZ;
        result.width = rect.width * accumulated.scaleX;
        result.height = rect.height * accumulated.scaleY;
        result.accumulatedRotationZ = accumulated.rotationZ;
        result.accumulatedScaleX = accumulated.scaleX;
        result.accumulatedScaleY = accumulated.scaleY;

        // Pixel-perfect snapping for screen space only
        if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
            canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
            if (canvas.pixelPerfect) {
                ApplyPixelPerfectSnapping(result);
            }
        }

        return result;
    }

    void UIRenderSystem::ApplyPixelPerfectSnapping(WorldTransform& transform)
    {
        transform.x = std::round(transform.x);
        transform.y = std::round(transform.y);
        transform.width = std::round(transform.width);
        transform.height = std::round(transform.height);
    }

    void UIRenderSystem::UpdateWorldMatrix(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas
    )
    {
        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return;
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);
        if (!rect.worldMatrixDirty) return;

        // Get accumulated transforms (existing accumulation logic)
        AccumulatedTransform acc = AccumulateParentTransforms(entity, canvasEntity, canvas);

        // Build TRS matrices
        Math::Mat4 T = Math::Mat4::BuildTranslation(Math::Vec3(acc.posX, acc.posY, acc.posZ));
        Math::Mat4 R = rect.GetRotationMatrix();
        Math::Mat4 S = Math::Mat4::BuildScaling(acc.scaleX, acc.scaleY, acc.scaleZ);

        // Handle pivot offset
        float pivotOffsetX = rect.width * rect.pivotX;
        float pivotOffsetY = rect.height * rect.pivotY;
        Math::Mat4 pivotTrans = Math::Mat4::BuildTranslation(Math::Vec3(pivotOffsetX, pivotOffsetY, 0.0f));
        Math::Mat4 pivotTransInv = Math::Mat4::BuildTranslation(Math::Vec3(-pivotOffsetX, -pivotOffsetY, 0.0f));

        // Build final world matrix: T * pivot * R * S * pivot^-1
        rect.worldMatrix = T * pivotTrans * R * S * pivotTransInv;
        rect.worldMatrixDirty = false;
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

        // NOTE: We can't use GLGeometryBuffer directly because it's hardcoded for Vertex struct
        // For now, convert UIVertex2 to standard Vertex format as a workaround
        // TODO Phase 3: Create dedicated UIGeometryBuffer class

        std::vector<NE::Graphics::Vertex> standardVertices;
        standardVertices.reserve(vertices.size());

        for (const auto& uiVert : vertices) {
            NE::Graphics::Vertex v;
            v.Position = uiVert.Position;
            v.Normal = Math::Vec3(0, 0, 1); // Default normal for UI
            v.TexCoord = uiVert.TexCoord;
            // BoneIDs and Weights default to 0 (already initialized in struct)
            standardVertices.push_back(v);
        }

        // Create buffers with standard Vertex format
        auto vb = std::make_shared<NE::Graphics::OpenGL::GLVertexBuffer>(
            standardVertices.data(),
            static_cast<uint32_t>(standardVertices.size() * sizeof(NE::Graphics::Vertex)),
            sizeof(NE::Graphics::Vertex)
        );

        auto ib = std::make_shared<NE::Graphics::OpenGL::GLIndexBuffer>(
            indices.data(),
            static_cast<uint32_t>(indices.size())
        );

        return std::make_shared<NE::Graphics::OpenGL::GLGeometryBuffer>(vb, ib);
    }

    void UIRenderSystem::SubmitUIElement(
        Entity entity,
        const UICanvas& canvas,
        const UIImage& img,
        const UIRectTransform& rect,
        const std::vector<NE::Graphics::UIVertex2>& vertices
    )
    {
        // Create dynamic geometry buffer
        auto geometryBuffer = CreateDynamicUIGeometry(vertices);
        if (!geometryBuffer) {
            std::cerr << "[UIRenderSystem::SubmitUIElement] ERROR: Failed to create geometry buffer!" << std::endl;
            return;
        }

        // Get material (use component material or fallback to default)
        std::shared_ptr<NE::Graphics::Material> material = img.material;

        if (!material) {
            // Use default UI material
            material = m_defaultUIMaterial;

            if (!material) {
                // No material available, skip rendering
                // This happens if shader loading failed in Init()
                std::cerr << "[UIRenderSystem::SubmitUIElement] ERROR: No material available (shader loading failed)" << std::endl;
                return;
            }
        }

        // Clone material to set per-instance uniforms
        auto instanceMaterial = std::make_shared<NE::Graphics::Material>(*material);

        // Set per-instance uniforms
        instanceMaterial->SetUniformVec4("uColor", img.color);

        // Get screen size from GraphicsManager
        uint32_t screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
        uint32_t screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

        // Set screen size as Vec2 (shader expects vec2)
        instanceMaterial->SetUniformVec2("uScreenSize",
            Math::Vec2(static_cast<float>(screenWidth), static_cast<float>(screenHeight)));

        // Set texture if available
        if (img.bindlessHandle != 0) {
            instanceMaterial->SetUniformInt("uHasTexture", 1);
            // TODO Phase 3: Set bindless texture handle
            // Material::Bind() expects GLTexture objects in m_Textures map
            // We have a raw bindless handle (uint64_t) which doesn't fit the current API
            // Need to either:
            //   1. Add Material::SetUniformHandle(name, handle) method
            //   2. Create a GLTexture wrapper for existing handle
            //   3. Set uniform directly on shader after material bind (requires pipeline access)
            // For now, textures won't render - only solid colors work
        } else {
            instanceMaterial->SetUniformInt("uHasTexture", 0);
        }

        // Set render queue - UI uses OVERLAY (4000) as base priority
        instanceMaterial->SetQueueBase(NE::Graphics::RenderQueue::OVERLAY);
        instanceMaterial->SetQueueOffset(canvas.sortingOrder);

        // Build DrawCommand
        NE::Graphics::DrawCommand cmd;

        // For screen-space UI, vertices are already in pixel coordinates
        // The shader converts them to NDC, so use identity transform
        cmd.transform = Math::Mat4(); // Identity matrix
        cmd.mesh = geometryBuffer;
        cmd.material = instanceMaterial;
        cmd.castsShadow = false;
        cmd.receivesShadow = false;

        // CRITICAL: Set bounds to ensure UI passes frustum culling
        // Screen-space UI should always be considered visible
        // Set bounds at origin with very large radius to always pass frustum test
        cmd.boundsCenterWS = Math::Vec3(0.0f, 0.0f, 0.0f);
        cmd.boundsRadiusWs = 999999.0f;  // Effectively infinite - always visible

        // Submit through standard pipeline
        NE::Graphics::GraphicsManager::Submit(cmd);
    }

    //=========================================================================
    // Canvas & Scaling
    //=========================================================================

    float UIRenderSystem::CalculateScaleFactor(const UICanvas& canvas) 
    {
        float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
        float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

        switch (canvas.scaleMode) {
        case UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE: {
            float widthScale = screenWidth / canvas.referenceWidth;
            float heightScale = screenHeight / canvas.referenceHeight;
            return std::min(widthScale, heightScale);
        }

        case UICanvas::ScaleMode::CONSTANT_PIXEL_SIZE: {
            return 1.0f;
        }

        case UICanvas::ScaleMode::CONSTANT_PHYSICAL_SIZE: {
            float referenceDPI = 96.0f;
            float currentDPI = 96.0f;
            return currentDPI / referenceDPI;
        }
        }

        return 1.0f;
    }

    //=========================================================================
    // Rendering
    //=========================================================================

    std::vector<Entity> UIRenderSystem::CollectCanvasChildren(Entity canvasEntity) 
    {
        const auto& entities = GetEntities();
        std::vector<Entity> canvasChildren;

        for (Entity e : entities) {
            if (e == canvasEntity) continue;
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;
            if (!m_cm->HasComponent<UIImage>(e)) continue;
            if (m_cm->HasComponent<UICanvas>(e)) continue;

            auto& rect = m_cm->GetComponent<UIRectTransform>(e);

            Entity root = e;
            Entity current = rect.parent;

            while (current != NO_ENTITY) {
                root = current;
                if (!m_cm->HasComponent<UIRectTransform>(current)) break;
                current = m_cm->GetComponent<UIRectTransform>(current).parent;
            }

            if (root == canvasEntity || rect.parent == canvasEntity) {
                canvasChildren.push_back(e);
            }
        }

        return canvasChildren;
    }

    void UIRenderSystem::SortEntitiesByZOrder(std::vector<Entity>& entities) 
    {
        std::sort(entities.begin(), entities.end(),
            [this](Entity a, Entity b) {
                auto& rectA = m_cm->GetComponent<UIRectTransform>(a);
                auto& rectB = m_cm->GetComponent<UIRectTransform>(b);
                return rectA.z < rectB.z;
            });
    }

    std::vector<NE::Graphics::UIVertex> UIRenderSystem::GenerateScreenSpaceVertices(
        Entity entity,
        const WorldTransform& worldTransform,
        const UIImage& img
    ) 
    {
        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        auto vertices = NE::Graphics::UIImageMeshGenerator::GenerateVertices(
            img,
            worldTransform.x,
            worldTransform.y,
            worldTransform.z,
            worldTransform.width,
            worldTransform.height,
            img.color
        );

        if (!vertices.empty() && std::abs(worldTransform.accumulatedRotationZ) > ROTATION_EPSILON) {
            float pivotX = worldTransform.x + worldTransform.width * rect.pivotX;
            float pivotY = worldTransform.y + worldTransform.height * rect.pivotY;
            RotateVertices2D(vertices, pivotX, pivotY, worldTransform.accumulatedRotationZ);
        }

        return vertices;
    }

    std::vector<NE::Graphics::UIVertex> UIRenderSystem::GenerateWorldSpaceVertices(const UIImage& img) 
    {
        return NE::Graphics::UIImageMeshGenerator::GenerateVertices(
            img,
            0.0f, 0.0f, 0.0f,
            1.0f, 1.0f,
            img.color
        );
    }

    Math::Mat4 UIRenderSystem::BuildWorldSpaceModelMatrix(
        Entity entity,
        Entity canvasEntity,
        const UIRectTransform& rect,
        const AccumulatedTransform& accumulated
    ) 
    {
        // compute pivot offset
        Math::Vec2 pivot = rect.GetPivot();

        float pivotOffsetX = -rect.width * pivot.x * accumulated.scaleX;
        float pivotOffsetY = -rect.height * pivot.y * accumulated.scaleY;

        // Scale matrix using accumulated scale
        Math::Mat4 scaleMatrix = Math::Mat4::BuildScaling(
            rect.width * accumulated.scaleX,
            rect.height * accumulated.scaleY,
            accumulated.scaleZ
        );

        // pivot offset in local space
        Math::Mat4 pivotMatrix = Math::Mat4::BuildTranslation(
            pivotOffsetX,
            pivotOffsetY,
            0.0f
        );

        // rotation using accumulated rotations
        Math::Mat4 rotationX = Math::Mat4::BuildXRotation(accumulated.rotationX * PI / 180.0f);
        Math::Mat4 rotationY = Math::Mat4::BuildYRotation(accumulated.rotationY * PI / 180.0f);
        Math::Mat4 rotationZ = Math::Mat4::BuildZRotation(accumulated.rotationZ * PI / 180.0f);
        Math::Mat4 rotationMatrix = rotationZ * rotationY * rotationX;

        // translation using accumulated position
        Math::Mat4 translationMatrix = Math::Mat4::BuildTranslation(
            accumulated.posX,
            accumulated.posY,
            accumulated.posZ
        );

        return translationMatrix * rotationMatrix * pivotMatrix * scaleMatrix;
    }

    void UIRenderSystem::SubmitDrawCommand(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas,
        const UIImage& img,
        const UIRectTransform& rect,
        const WorldTransform& worldTransform,
        const AccumulatedTransform& accumulated,
        std::vector<NE::Graphics::UIVertex>& vertices,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) 
    {
        NE::Graphics::UIDrawCommand cmd;

        cmd.x = worldTransform.x;
        cmd.y = worldTransform.y;
        cmd.z = worldTransform.z;
        cmd.width = worldTransform.width;
        cmd.height = worldTransform.height;
        cmd.color = img.color;
        cmd.order = canvas.sortingOrder;
        cmd.entityId = entity;
        cmd.renderMode = static_cast<int>(canvas.renderMode);
        cmd.planeDistance = canvas.planeDistance;

        cmd.material = img.material;
        cmd.bindlessTextureHandle = img.bindlessHandle;

        cmd.vertices = vertices;
        cmd.useCustomVertices = !vertices.empty() &&
            (img.imageType != UIImage::ImageType::SIMPLE ||
                img.fillAmount < 1.0f ||
                std::abs(worldTransform.accumulatedRotationZ) > ROTATION_EPSILON);

        if (viewMatrix) cmd.viewMatrix = *viewMatrix;
        if (projMatrix) cmd.projMatrix = *projMatrix;

        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            cmd.modelMatrix = BuildWorldSpaceModelMatrix(entity, canvasEntity, rect, accumulated);
        }

        NE::Graphics::UIRenderer::Submit(cmd);
    }

    void UIRenderSystem::RenderCanvasChildren(
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) 
    {
        std::vector<Entity> canvasChildren = CollectCanvasChildren(canvasEntity);

        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE && canvasChildren.size() > 1) 
        {
            SortEntitiesByZOrder(canvasChildren);
        }

        for (Entity e : canvasChildren) {
            auto& img = m_cm->GetComponent<UIImage>(e);
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);

            if (m_useIntegratedPipeline) {
                // NEW PATH: Submit through GraphicsManager
                UpdateWorldMatrix(e, canvasEntity, canvas);

                // Generate vertices using UIVertex2
                std::vector<NE::Graphics::UIVertex2> verticesV2;
                WorldTransform worldTransform = CalculateWorldTransform(e, canvasEntity, canvas, viewMatrix, projMatrix);

                // Use white color for vertices (color is applied via uniform)
                // The old shader only uses uColor uniform, not vertex colors
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
                    SubmitUIElement(e, canvas, img, rect, verticesV2);
                }
            }
            else {
                // OLD PATH: Submit through UIRenderer
                AccumulatedTransform accumulated = AccumulateParentTransforms(e, canvasEntity, canvas);

                WorldTransform worldTransform = CalculateWorldTransform(e, canvasEntity, canvas, viewMatrix, projMatrix);

                std::vector<NE::Graphics::UIVertex> vertices;

                if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                    vertices = GenerateWorldSpaceVertices(img);
                }
                else {
                    vertices = GenerateScreenSpaceVertices(e, worldTransform, img);
                }

                if (vertices.empty()) continue;

                SubmitDrawCommand(e, canvasEntity, canvas, img, rect, worldTransform, accumulated, vertices, viewMatrix, projMatrix);
            }
        }
    }

    //=========================================================================
    // Vertex Manipulation
    //=========================================================================

    void UIRenderSystem::RotateVertices2D(
        std::vector<NE::Graphics::UIVertex>& vertices,
        float pivotX,
        float pivotY,
        float rotationDegrees
    ) 
    {
        if (std::abs(rotationDegrees) < ROTATION_EPSILON) return;

        float radians = rotationDegrees * PI / 180.0f;
        float cosR = std::cos(radians);
        float sinR = std::sin(radians);

        for (auto& v : vertices) {
            float localX = v.x - pivotX;
            float localY = v.y - pivotY;

            v.x = pivotX + localX * cosR - localY * sinR;
            v.y = pivotY + localX * sinR + localY * cosR;
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

    std::vector<Entity> UIRenderSystem::CollectTextChildren(Entity canvasEntity) 
    {
        const auto& entities = GetEntities();
        std::vector<Entity> textChildren;

        for (Entity e : entities) {
            if (e == canvasEntity) continue;
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;
            if (!m_cm->HasComponent<UIText>(e)) continue;
            if (m_cm->HasComponent<UICanvas>(e)) continue;

            auto& rect = m_cm->GetComponent<UIRectTransform>(e);

            Entity root = e;
            Entity current = rect.parent;

            while (current != NO_ENTITY) {
                root = current;
                if (!m_cm->HasComponent<UIRectTransform>(current)) break;
                current = m_cm->GetComponent<UIRectTransform>(current).parent;
            }

            if (root == canvasEntity || rect.parent == canvasEntity) {
                textChildren.push_back(e);
            }
        }

        return textChildren;
    }

    void UIRenderSystem::RenderTextEntity(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) 
    {
        if (!m_cm->HasComponent<UIText>(entity) || !m_cm->HasComponent<UIRectTransform>(entity)) return;

        auto& text = m_cm->GetComponent<UIText>(entity);
        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        // Skip if no text or font
        if (text.text.empty() || text.fontUUID.empty()) return;

        // Calculate accumulated scale for font scaling
        AccumulatedTransform accumulated = AccumulateParentTransforms(entity, canvasEntity, canvas);

        // Use the average scale for uniform font scaling (take the smaller to ensure text fits)
        float scaleFactorForFont = std::min(accumulated.scaleX, accumulated.scaleY);

        // Calculate effective font size based on accumulated scale
        float effectiveFontSize = text.fontSize * scaleFactorForFont;

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
            return;
        }

        WorldTransform worldTransform =
            CalculateWorldTransform(entity, canvasEntity, canvas, viewMatrix, projMatrix);

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
            WorldTransform worldTransform = CalculateWorldTransform(entity, canvasEntity, canvas, viewMatrix, projMatrix);

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

            text.cachedVertices.clear();
            text.cachedVertices.reserve(result.vertices.size());
            for (const auto& v : result.vertices) {
                UITextVertex tv;
                tv.x = v.x;
                tv.y = v.y;
                tv.z = v.z;
                tv.u = v.u;
                tv.v = v.v;
                tv.r = v.r;
                tv.g = v.g;
                tv.b = v.b;
                tv.a = v.a;
                text.cachedVertices.push_back(tv);
            }

            // Apply rotation
            float rot = worldTransform.accumulatedRotationZ * (3.1415926535f / 180.0f);
            if (std::abs(rot) > 0.0001f) {
                // Rotate around rect pivot (preferred). If you don't have pivot, use 0.5f, 0.5f.
                const float pivotX = worldTransform.x + worldTransform.width * rect.pivotX;
                const float pivotY = worldTransform.y + worldTransform.height * rect.pivotY;

                const float c = std::cos(rot);
                const float s = std::sin(rot);

                for (auto& v : text.cachedVertices) {
                    float lx = v.x - pivotX;
                    float ly = v.y - pivotY;
                    float rx = lx * c - ly * s;
                    float ry = lx * s + ly * c;
                    v.x = rx + pivotX;
                    v.y = ry + pivotY;
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
        SubmitTextDrawCommand(entity, canvasEntity, canvas, text, rect, worldTransform, fontAtlas, viewMatrix, projMatrix);
    }

    void UIRenderSystem::SubmitTextDrawCommand(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas,
        UIText& text,
        const UIRectTransform& rect,
        const WorldTransform& worldTransform,
        std::shared_ptr<NE::Graphics::FontAtlas> fontAtlas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) 
    {
        if (text.cachedVertices.empty()) return;

        NE::Graphics::UIDrawCommand cmd;

        cmd.x = worldTransform.x;
        cmd.y = worldTransform.y;
        cmd.z = worldTransform.z;
        cmd.width = worldTransform.width;
        cmd.height = worldTransform.height;
        cmd.color = text.color;
        cmd.order = canvas.sortingOrder;
        cmd.entityId = entity;
        cmd.renderMode = static_cast<int>(canvas.renderMode);
        cmd.planeDistance = canvas.planeDistance;

        cmd.bindlessTextureHandle = fontAtlas->GetBindlessHandle();
        cmd.isTextCommand = true;

        // Convert cached vertices to UIVertex
        cmd.vertices.reserve(text.cachedVertices.size());
        for (const auto& tv : text.cachedVertices) {
            NE::Graphics::UIVertex v;
            v.x = tv.x;
            v.y = tv.y;
            v.z = tv.z;
            v.u = tv.u;
            v.v = tv.v;
            v.r = tv.r;
            v.g = tv.g;
            v.b = tv.b;
            v.a = tv.a;
            cmd.vertices.push_back(v);
        }

        cmd.useCustomVertices = true;

        if (viewMatrix) cmd.viewMatrix = *viewMatrix;
        if (projMatrix) cmd.projMatrix = *projMatrix;

        NE::Graphics::UIRenderer::Submit(cmd);
    }

} // namespace NE::ECS::Systems