#include "UIRenderSystem.hpp"
#include "UITransformSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
#include "../Components/UIText.hpp"
#include "../Components/UIButton.hpp"
#include "../../Graphics/Core/UIDrawCommand.hpp"
#include "../../Graphics/Core/UIRenderer.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Graphics/Core/EditorCamera.hpp"
#include "../../ResourceManagement/ResourceManager.hpp"
#include "../../ResourceManagement/ResourcePaths.hpp"
#include "../../Graphics/Core/Font.hpp"
#include "Core/SpdLogger.hpp"
#include <functional>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace NE::ECS;
using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    //=========================================================================
    // Constants
    //=========================================================================

    static constexpr float PI = 3.14159265358979f;
    static constexpr float ROTATION_EPSILON = 0.001f;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    UIRenderSystem::UIRenderSystem(ComponentManager* cm, UITransformSystem* transformSystem) 
        : m_cm(cm), m_transformSystem(transformSystem) {}

    // Generate font cache key
    static uint32_t GenerateFontCacheKey(const std::string& fontUUID, float fontSize, UIText::FontStyle fontStyle) {
        std::hash<std::string> hasher;
        uint32_t uuidHash = static_cast<uint32_t>(hasher(fontUUID));
        uint32_t sizeKey = static_cast<uint32_t>(fontSize * 100.0f);
        uint32_t styleKey = static_cast<uint32_t>(static_cast<int>(fontStyle));
        return uuidHash ^ (sizeKey << 16) ^ (sizeKey >> 16) ^ (styleKey << 8) ^ (styleKey >> 24);
    }

    // Calculate optimal font size for auto-sizing
    static float CalculateOptimalFontSize(
        UIText& text,
        float containerWidth,
        float containerHeight,
        const std::string& fontPath,
        const std::vector<uint8_t>& fontData
    ) {
        if (text.text.empty() || containerWidth <= 0.0f || containerHeight <= 0.0f) {
            return text.fontSize;
        }

        bool isBold = (text.fontStyle == UIText::FontStyle::BOLD) || 
                      (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);
        bool isItalic = (text.fontStyle == UIText::FontStyle::ITALIC) || 
                        (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);

        bool shouldWrap = (text.horizontalOverflow == UIText::OverflowMode::WRAP);
        float maxWidth = shouldWrap ? containerWidth : 0.0f;

        float minSize = text.minSize;
        float maxSize = text.maxSize;
        float bestSize = minSize;

        // Reduced iterations and increased epsilon for better performance
        // Binary search converges quickly (typically ~5-7 iterations for size range 30)
        const float epsilon = 1.0f;  // Accept 1px precision (was 0.5px)
        int iterations = 0;
        const int maxIterations = 10;  // Reduced from 20 (was more than needed)
        
        // Reuse a single Font object to avoid repeated allocations
        std::shared_ptr<NE::Graphics::Font> testFont = std::make_shared<NE::Graphics::Font>();

        while (maxSize - minSize > epsilon && iterations < maxIterations) {
            float testSize = (minSize + maxSize) * 0.5f;
            
            bool loaded = false;
            
            if (!fontData.empty()) {
                loaded = testFont->LoadFromBinaryData(fontData, testSize, isBold, isItalic);
            } else if (!fontPath.empty()) {
                loaded = testFont->LoadFromFile(fontPath, testSize, isBold, isItalic);
            } else {
                loaded = testFont->LoadFromFile("Assets/Fonts/Roboto-Regular.ttf", testSize, isBold, isItalic);
            }

            if (!loaded) {
                maxSize = testSize;
                iterations++;
                continue;
            }

            float textWidth = testFont->MeasureTextWidth(text.text);
            float textHeight = testFont->MeasureTextHeight(text.text, maxWidth);

            if (textWidth <= containerWidth && textHeight <= containerHeight) {
                bestSize = testSize;
                minSize = testSize;
            } else {
                maxSize = testSize;
            }
            
            iterations++;
        }

        return std::max(text.minSize, std::min(text.maxSize, bestSize));
    }

    void UIRenderSystem::Init() {
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (m_cm->HasComponent<UIImage>(e)) {
                auto& img = m_cm->GetComponent<UIImage>(e);

                if (!img.textureUUID.empty()) {
                    auto texture = NE::Resource::ResourceManager::GetInstance()
                        .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
                    if (texture) {
                        img.cachedTextureWidth = static_cast<float>(texture->GetWidth());
                        img.cachedTextureHeight = static_cast<float>(texture->GetHeight());
                        
                        if (img.bindlessHandle == 0) {
                            img.bindlessHandle = texture->GetBindlessHandle();
                        }
                    }
                }

                if (!img.materialUUID.empty() && !img.material) {
                    img.material = NE::Resource::ResourceManager::GetInstance()
                        .LoadResource<NE::Graphics::Material>(img.materialUUID);
                }
            }
            if (m_cm->HasComponent<UIText>(e)) {
                auto& text = m_cm->GetComponent<UIText>(e);

                // Load material
                if (!text.materialUUID.empty() && !text.material) {
                    text.material = NE::Resource::ResourceManager::GetInstance()
                        .LoadResource<NE::Graphics::Material>(text.materialUUID);
                }

                // Extract font style flags
                bool isBold = (text.fontStyle == UIText::FontStyle::BOLD) || 
                              (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);
                bool isItalic = (text.fontStyle == UIText::FontStyle::ITALIC) || 
                                (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);

                uint32_t currentCacheKey = GenerateFontCacheKey(text.fontUUID, text.fontSize, text.fontStyle);
                
                if (text.fontHandle != 0 && text.fontHandle != currentCacheKey) {
                    text.fontHandle = 0;
                }

                if (text.fontHandle == 0) {
                    auto cacheIt = m_fontCache.find(currentCacheKey);
                    if (cacheIt != m_fontCache.end()) {
                        text.fontHandle = currentCacheKey;
                    } else {
                        std::shared_ptr<NE::Graphics::Font> font;
                        if (!text.fontUUID.empty()) {
                            std::string fontPath = NE::Resource::ComputeFontArtifactPathFromUUID(text.fontUUID);
                            std::ifstream fontFile(fontPath, std::ios::binary | std::ios::ate);
                            if (fontFile) {
                                std::streamsize fileSize = fontFile.tellg();
                                if (fileSize > 0) {
                                    fontFile.seekg(0, std::ios::beg);
                                    std::vector<uint8_t> fontData(static_cast<size_t>(fileSize));
                                    if (fontFile.read(reinterpret_cast<char*>(fontData.data()), fileSize)) {
                                        font = std::make_shared<NE::Graphics::Font>();
                                        if (!font->LoadFromBinaryData(fontData, text.fontSize, isBold, isItalic)) {
                                            font = nullptr;
                                        }
                                    }
                                }
                                fontFile.close();
                            }
                        }
                        
                        if (!font && !text.fontPath.empty()) {
                            font = std::make_shared<NE::Graphics::Font>();
                            if (!font->LoadFromFile(text.fontPath, text.fontSize, isBold, isItalic)) {
                                font = nullptr;
                            }
                        }
                        
                        if (!font) {
                            font = std::make_shared<NE::Graphics::Font>();
                            if (!font->LoadFromFile("Assets/Fonts/Roboto-Regular.ttf", text.fontSize, isBold, isItalic)) {
                                SPD_WARNING("Failed to load font for UIText entity: " << e);
                            } else {
                                m_fontCache[currentCacheKey] = font;
                                text.fontHandle = currentCacheKey;
                            }
                        } else {
                            m_fontCache[currentCacheKey] = font;
                            text.fontHandle = currentCacheKey;
                        }
                    }
                }
            }
        }
    }

    void UIRenderSystem::OnEntityAdded(Entity e) {
        if (m_cm->HasComponent<UIImage>(e)) {
            auto& img = m_cm->GetComponent<UIImage>(e);

            if (!img.textureUUID.empty()) {
                auto texture = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
                if (texture) {
                    img.bindlessHandle = texture->GetBindlessHandle();
                    img.cachedTextureWidth = static_cast<float>(texture->GetWidth());
                    img.cachedTextureHeight = static_cast<float>(texture->GetHeight());
                }
            }

            if (!img.materialUUID.empty()) {
                img.material = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::Material>(img.materialUUID);
            }

            img.isDirty = true;
        }
        if (m_cm->HasComponent<UIText>(e)) {
            auto& text = m_cm->GetComponent<UIText>(e);

            if (!text.materialUUID.empty() && !text.material) {
                text.material = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::Material>(text.materialUUID);
            }

            bool isBold = (text.fontStyle == UIText::FontStyle::BOLD) || 
                          (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);
            bool isItalic = (text.fontStyle == UIText::FontStyle::ITALIC) || 
                            (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);

            uint32_t currentCacheKey = GenerateFontCacheKey(text.fontUUID, text.fontSize, text.fontStyle);
            
            if (text.fontHandle != 0 && text.fontHandle != currentCacheKey) {
                text.fontHandle = 0;
            }
            
            if (text.fontHandle == 0) {
                auto cacheIt = m_fontCache.find(currentCacheKey);
                if (cacheIt != m_fontCache.end()) {
                    text.fontHandle = currentCacheKey;
                } else {
                    std::shared_ptr<NE::Graphics::Font> font;
                    if (!text.fontUUID.empty()) {
                        std::string fontPath = NE::Resource::ComputeFontArtifactPathFromUUID(text.fontUUID);
                        std::ifstream fontFile(fontPath, std::ios::binary | std::ios::ate);
                        if (fontFile) {
                            std::streamsize fileSize = fontFile.tellg();
                            if (fileSize > 0) {
                                fontFile.seekg(0, std::ios::beg);
                                std::vector<uint8_t> fontData(static_cast<size_t>(fileSize));
                                if (fontFile.read(reinterpret_cast<char*>(fontData.data()), fileSize)) {
                                    font = std::make_shared<NE::Graphics::Font>();
                                    if (!font->LoadFromBinaryData(fontData, text.fontSize, isBold, isItalic)) {
                                        font = nullptr;
                                    }
                                }
                            }
                            fontFile.close();
                        }
                    }
                    
                    // If UUID loading failed, fall back to file path (for backward compatibility)
                    if (!font && !text.fontPath.empty()) {
                        font = std::make_shared<NE::Graphics::Font>();
                        if (!font->LoadFromFile(text.fontPath, text.fontSize, isBold, isItalic)) {
                            font = nullptr;
                        }
                    }
                    
                    if (!font) {
                        font = std::make_shared<NE::Graphics::Font>();
                        if (!font->LoadFromFile("Assets/Fonts/Roboto-Regular.ttf", text.fontSize, isBold, isItalic)) {
                            SPD_WARNING("Failed to load font for UIText entity: " << e);
                        } else {
                            m_fontCache[currentCacheKey] = font;
                            text.fontHandle = currentCacheKey;
                        }
                    } else {
                        m_fontCache[currentCacheKey] = font;
                        text.fontHandle = currentCacheKey;
                    }
                }
            }
        }
    }

    void UIRenderSystem::OnEntityRemoved(Entity e) {}

    void UIRenderSystem::Exit() {}

    void UIRenderSystem::SetTransformSystem(UITransformSystem* transformSystem) {
        m_transformSystem = transformSystem;
    }


    //=========================================================================
    // Rendering
    //=========================================================================

    std::vector<Entity> UIRenderSystem::CollectCanvasChildren(Entity canvasEntity) {
        const auto& entities = GetEntities();
        std::vector<Entity> canvasChildren;

        for (Entity e : entities) {
            if (e == canvasEntity) continue;
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;
            if (!m_cm->HasComponent<UIImage>(e) && !m_cm->HasComponent<UIText>(e)) continue;
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

    void UIRenderSystem::SortEntitiesByZOrder(std::vector<Entity>& entities) {
        std::sort(entities.begin(), entities.end(),
            [this](Entity a, Entity b) {
                auto& rectA = m_cm->GetComponent<UIRectTransform>(a);
                auto& rectB = m_cm->GetComponent<UIRectTransform>(b);
                return rectA.z < rectB.z;
            });
    }

    std::vector<NE::Graphics::UIVertex> UIRenderSystem::GenerateScreenSpaceVertices(
        Entity entity,
        const UITransformSystem::WorldTransform& worldTransform,
        const Component::UIImage& img
    ) {
        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        // Calculate top-left from pivot
        float topLeftX = worldTransform.x - worldTransform.width * rect.pivotX;
        float topLeftY = worldTransform.y - worldTransform.height * (1.0f - rect.pivotY);
        
        auto vertices = NE::Graphics::UIImageMeshGenerator::GenerateVertices(
            img,
            topLeftX,
            topLeftY,
            worldTransform.z,
            worldTransform.width,
            worldTransform.height,
            img.color
        );

        if (!vertices.empty() && std::abs(worldTransform.accumulatedRotationZ) > ROTATION_EPSILON) {
            float pivotX = worldTransform.x;
            float pivotY = worldTransform.y;
            RotateVertices2D(vertices, pivotX, pivotY, worldTransform.accumulatedRotationZ);
        }

        return vertices;
    }

    std::vector<NE::Graphics::UIVertex> UIRenderSystem::GenerateWorldSpaceVertices(
        const Component::UIImage& img,
        float width,
        float height
    ) {
        auto vertices = NE::Graphics::UIImageMeshGenerator::GenerateVertices(
            img,
            0.0f, 0.0f, 0.0f,
            width,
            height,
            img.color
        );
        
        // Normalize to unit quad
        for (auto& vertex : vertices) {
            vertex.x /= width;
            vertex.y /= height;
        }
        
        // Flip V for world space
        for (auto& vertex : vertices) {
            vertex.v = 1.0f - vertex.v;
        }
        
        return vertices;
    }


    void UIRenderSystem::SubmitDrawCommand(
        Entity entity,
        Entity canvasEntity,
        const Component::UICanvas& canvas,
        const Component::UIImage& img,
        const Component::UIRectTransform& rect,
        const UITransformSystem::WorldTransform& worldTransform,
        const UITransformSystem::AccumulatedTransform& accumulated,
        std::vector<NE::Graphics::UIVertex>& vertices,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) {
        NE::Graphics::UIDrawCommand cmd;

        float topLeftX = worldTransform.x - worldTransform.width * rect.pivotX;
        float topLeftY = worldTransform.y - worldTransform.height * (1.0f - rect.pivotY);
        
        cmd.x = topLeftX;
        cmd.y = topLeftY;
        cmd.z = worldTransform.z;
        cmd.width = worldTransform.width;
        cmd.height = worldTransform.height;
        
        // Apply button color tint
        NE::Math::Vec4 finalColor = img.color;
        if (m_cm->HasComponent<UIButton>(entity)) {
            const auto& button = m_cm->GetComponent<UIButton>(entity);
            NE::Math::Vec4 buttonColor = button.GetCurrentColor();
            finalColor.x = img.color.x * buttonColor.x;
            finalColor.y = img.color.y * buttonColor.y;
            finalColor.z = img.color.z * buttonColor.z;
            finalColor.w = img.color.w * buttonColor.w;
        }
        cmd.color = finalColor;
        cmd.order = canvas.sortingOrder;
        cmd.entityId = entity;
        cmd.renderMode = static_cast<int>(canvas.renderMode);
        cmd.planeDistance = canvas.planeDistance;

        cmd.material = img.material;
        cmd.bindlessTextureHandle = img.bindlessHandle;

        cmd.vertices = vertices;
        // Use custom vertices for complex images or world space
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            cmd.useCustomVertices = !vertices.empty();
        } else {
            cmd.useCustomVertices = !vertices.empty() &&
                (img.imageType != UIImage::ImageType::SIMPLE ||
                    img.fillAmount < 1.0f ||
                    img.preserveAspect ||
                    std::abs(worldTransform.accumulatedRotationZ) > ROTATION_EPSILON);
        }

        if (viewMatrix) cmd.viewMatrix = *viewMatrix;
        if (projMatrix) cmd.projMatrix = *projMatrix;

        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE && m_transformSystem) {
            cmd.modelMatrix = m_transformSystem->BuildWorldSpaceModelMatrix(entity, canvasEntity, rect, accumulated);
        }

        NE::Graphics::UIRenderer::Submit(cmd);
    }

    void UIRenderSystem::RenderCanvasChildren(
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) {
        std::vector<Entity> canvasChildren = CollectCanvasChildren(canvasEntity);

        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE && canvasChildren.size() > 1) {
            SortEntitiesByZOrder(canvasChildren);
        }

        for (Entity e : canvasChildren) {
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);

            if (!m_transformSystem) continue;

            UITransformSystem::AccumulatedTransform accumulated = m_transformSystem->AccumulateParentTransforms(e, canvasEntity, canvas);

            UITransformSystem::WorldTransform worldTransform = m_transformSystem->CalculateWorldTransform(e, canvasEntity, canvas, viewMatrix, projMatrix);

            std::vector<NE::Graphics::UIVertex> vertices;

            if (m_cm->HasComponent<UIImage>(e)) {
                auto& img = m_cm->GetComponent<UIImage>(e);

                if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                    vertices = GenerateWorldSpaceVertices(img, worldTransform.width, worldTransform.height);
                }
                else {
                    vertices = GenerateScreenSpaceVertices(e, worldTransform, img);
                }

                if (!vertices.empty()) {
                    SubmitDrawCommand(e, canvasEntity, canvas, img, rect, worldTransform, accumulated, vertices, viewMatrix, projMatrix);
                }
            }
            else if (m_cm->HasComponent<UIText>(e)) {
                auto& text = m_cm->GetComponent<UIText>(e);

                // Load material if UUID is set but material is not loaded
                if (!text.materialUUID.empty() && !text.material) {
                    text.material = NE::Resource::ResourceManager::GetInstance()
                        .LoadResource<NE::Graphics::Material>(text.materialUUID);
                }

                // Auto-size calculation
                float containerWidth = (accumulated.calculatedWidth > 0.0f) ? accumulated.calculatedWidth : worldTransform.width;
                float containerHeight = (accumulated.calculatedHeight > 0.0f) ? accumulated.calculatedHeight : worldTransform.height;
                
                if (text.bestFit && containerWidth > 0.0f && containerHeight > 0.0f) {
                    // Check if font changed - invalidate cache if it did
                    bool fontChanged = (text.fontUUID != text.cachedFontUUID) || 
                                      (text.fontPath != text.cachedFontPath);
                    if (fontChanged) {
                        text.cachedFontData.clear();
                        text.cachedFontUUID = text.fontUUID;
                        text.cachedFontPath = text.fontPath;
                        text.cachedContainerWidth = -1.0f; // Force recalculation
                        text.cachedContainerHeight = -1.0f;
                    }
                    
                    // Check if container size changed - only recalculate if it did
                    const float sizeChangeThreshold = 0.5f;
                    bool containerSizeChanged = (std::abs(text.cachedContainerWidth - containerWidth) > sizeChangeThreshold ||
                                                 std::abs(text.cachedContainerHeight - containerHeight) > sizeChangeThreshold);
                    
                    if (containerSizeChanged || text.cachedFontData.empty()) {
                        // Load font data if not cached
                        if (text.cachedFontData.empty()) {
                            if (!text.fontUUID.empty()) {
                                std::string artifactPath = NE::Resource::ComputeFontArtifactPathFromUUID(text.fontUUID);
                                std::ifstream fontFile(artifactPath, std::ios::binary | std::ios::ate);
                                if (fontFile) {
                                    std::streamsize fileSize = fontFile.tellg();
                                    if (fileSize > 0) {
                                        fontFile.seekg(0, std::ios::beg);
                                        text.cachedFontData.resize(static_cast<size_t>(fileSize));
                                        fontFile.read(reinterpret_cast<char*>(text.cachedFontData.data()), fileSize);
                                    }
                                    fontFile.close();
                                }
                            }
                        }
                        
                        std::vector<uint8_t> fontData = text.cachedFontData;
                        std::string fontPath;
                        
                        if (text.cachedFontData.empty()) {
                            if (!text.fontPath.empty()) {
                                fontPath = text.fontPath;
                            } else {
                                fontPath = "Assets/Fonts/Roboto-Regular.ttf";
                            }
                        }

                        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                            const float referencePixelsPerWorldUnit = 36.0f;
                            containerWidth = containerWidth * referencePixelsPerWorldUnit;
                            containerHeight = containerHeight * referencePixelsPerWorldUnit;
                        }

                        float optimalSize = CalculateOptimalFontSize(
                            text,
                            containerWidth,
                            containerHeight,
                            fontPath,
                            fontData
                        );

                        if (std::abs(text.fontSize - optimalSize) > 0.01f) {
                            text.fontSize = optimalSize;
                            text.fontHandle = 0;
                        }
                        
                        // Update cached container size
                        text.cachedContainerWidth = containerWidth;
                        text.cachedContainerHeight = containerHeight;
                    }
                }

                bool isBold = (text.fontStyle == UIText::FontStyle::BOLD) || 
                              (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);
                bool isItalic = (text.fontStyle == UIText::FontStyle::ITALIC) || 
                                (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);

                uint32_t currentCacheKey = GenerateFontCacheKey(text.fontUUID, text.fontSize, text.fontStyle);
                
                std::shared_ptr<NE::Graphics::Font> font;
                
                if (text.fontHandle != 0) {
                    if (text.fontHandle == currentCacheKey) {
                        auto cacheIt = m_fontCache.find(static_cast<uint32_t>(text.fontHandle));
                        if (cacheIt != m_fontCache.end()) {
                            font = cacheIt->second;
                        }
                    } else {
                        text.fontHandle = 0;
                    }
                }
                
                if (!font) {
                    auto cacheIt = m_fontCache.find(currentCacheKey);
                    if (cacheIt != m_fontCache.end()) {
                        font = cacheIt->second;
                        text.fontHandle = currentCacheKey;
                    } else {
                        if (!text.fontUUID.empty()) {
                            std::string fontPath = NE::Resource::ComputeFontArtifactPathFromUUID(text.fontUUID);
                            std::ifstream fontFile(fontPath, std::ios::binary | std::ios::ate);
                            if (fontFile) {
                                std::streamsize fileSize = fontFile.tellg();
                                if (fileSize > 0) {
                                    fontFile.seekg(0, std::ios::beg);
                                    std::vector<uint8_t> fontData(static_cast<size_t>(fileSize));
                                    if (fontFile.read(reinterpret_cast<char*>(fontData.data()), fileSize)) {
                                        font = std::make_shared<NE::Graphics::Font>();
                                        if (!font->LoadFromBinaryData(fontData, text.fontSize, isBold, isItalic)) {
                                            font = nullptr;
                                        }
                                    }
                                }
                                fontFile.close();
                            }
                        }
                        
                        if (!font && !text.fontPath.empty()) {
                            font = std::make_shared<NE::Graphics::Font>();
                            if (!font->LoadFromFile(text.fontPath, text.fontSize, isBold, isItalic)) {
                                font = nullptr;
                            }
                        }
                        
                        if (!font) {
                            font = std::make_shared<NE::Graphics::Font>();
                            if (!font->LoadFromFile("Assets/Fonts/Roboto-Regular.ttf", text.fontSize, isBold, isItalic)) {
                                continue;
                            }
                        }
                        
                        m_fontCache[currentCacheKey] = font;
                        text.fontHandle = currentCacheKey;
                    }
                }

                if (!font) {
                    continue;
                }
                if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                    const float referencePixelsPerWorldUnit = 36.0f;
                    float textGenWidth = worldTransform.width * referencePixelsPerWorldUnit;
                    float textGenHeight = worldTransform.height * referencePixelsPerWorldUnit;
                    
                    vertices = NE::Graphics::UITextMeshGenerator::GenerateVertices(
                        text,
                        *font,
                        0.0f, 0.0f, 0.0f,
                        textGenWidth,
                        textGenHeight,
                        text.color
                    );
                    
                    // Normalize to unit quad space
                    for (auto& vertex : vertices) {
                        vertex.x /= textGenWidth;
                        vertex.y /= textGenHeight;
                    }
                    
                    // Flip Y for world space (Y-up)
                    for (auto& vertex : vertices) {
                        vertex.y = 1.0f - vertex.y;
                    }
                } else {
                    float topLeftX = worldTransform.x - worldTransform.width * rect.pivotX;
                    float topLeftY = worldTransform.y - worldTransform.height * (1.0f - rect.pivotY);

                    vertices = NE::Graphics::UITextMeshGenerator::GenerateVertices(
                        text,
                        *font,
                        topLeftX,
                        topLeftY,
                        worldTransform.z,
                        worldTransform.width,
                        worldTransform.height,
                        text.color
                    );

                    if (!vertices.empty() && std::abs(worldTransform.accumulatedRotationZ) > ROTATION_EPSILON) {
                        float pivotX = worldTransform.x;
                        float pivotY = worldTransform.y;
                        RotateVertices2D(vertices, pivotX, pivotY, worldTransform.accumulatedRotationZ);
                    }
                }

                if (!vertices.empty()) {
                    NE::Graphics::UIDrawCommand cmd;
                    
                    if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                        cmd.x = 0.0f;
                        cmd.y = 0.0f;
                        cmd.z = 0.0f;
                    } else {
                        float topLeftX = worldTransform.x - worldTransform.width * rect.pivotX;
                        float topLeftY = worldTransform.y - worldTransform.height * (1.0f - rect.pivotY);
                        cmd.x = topLeftX;
                        cmd.y = topLeftY;
                        cmd.z = worldTransform.z;
                    }
                    
                    cmd.width = worldTransform.width;
                    cmd.height = worldTransform.height;
                    cmd.color = text.color;
                    cmd.order = canvas.sortingOrder;
                    cmd.entityId = e;
                    cmd.renderMode = static_cast<int>(canvas.renderMode);
                    cmd.planeDistance = canvas.planeDistance;
                    cmd.vertices = vertices;
                    cmd.useCustomVertices = true;

                    cmd.material = text.material;
                    cmd.bindlessTextureHandle = font->GetAtlasTextureHandle();

                    if (viewMatrix) cmd.viewMatrix = *viewMatrix;
                    if (projMatrix) cmd.projMatrix = *projMatrix;

                    if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE && m_transformSystem) {
                        float fontSizeScale = text.fontSize / 36.0f;
                        cmd.modelMatrix = m_transformSystem->BuildWorldSpaceModelMatrix(e, canvasEntity, rect, accumulated, fontSizeScale);
                    }

                    NE::Graphics::UIRenderer::Submit(cmd);
                }
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
    ) {
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

    bool UIRenderSystem::GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj) {
        auto* cam = NE::Graphics::GraphicsManager::GetEditorCamera();
        if (!cam) return false;

        outView = cam->GetViewMatrix();
        outProj = cam->GetProjectionMatrix();

        return true;
    }

    //=========================================================================
    // Main Update Loop
    //=========================================================================

    void UIRenderSystem::Update(double) {
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

            if (m_transformSystem) {
                canvas.scaleFactor = m_transformSystem->CalculateScaleFactor(canvas);
                m_transformSystem->SetupCanvasDefaults(canvasEntity, canvas);
            }
            Math::Mat4 viewMatrix, projMatrix;
            Math::Mat4* pView = nullptr;
            Math::Mat4* pProj = nullptr;

            if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA ||
                canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                if (GetCameraMatrices(viewMatrix, projMatrix)) {
                    pView = &viewMatrix;
                    pProj = &projMatrix;
                }
            }

            RenderCanvasChildren(canvasEntity, canvas, pView, pProj);
        }
    }

} // namespace NE::ECS::Systems