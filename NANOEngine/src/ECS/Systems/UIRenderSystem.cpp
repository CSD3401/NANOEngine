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

    // Helper function to generate cache key from fontUUID, fontSize, and fontStyle
    static uint32_t GenerateFontCacheKey(const std::string& fontUUID, float fontSize, UIText::FontStyle fontStyle) {
        std::hash<std::string> hasher;
        uint32_t uuidHash = static_cast<uint32_t>(hasher(fontUUID));
        uint32_t sizeKey = static_cast<uint32_t>(fontSize * 100.0f);
        uint32_t styleKey = static_cast<uint32_t>(static_cast<int>(fontStyle));
        // Combine UUID hash, size key, and style key
        return uuidHash ^ (sizeKey << 16) ^ (sizeKey >> 16) ^ (styleKey << 8) ^ (styleKey >> 24);
    }

    // Calculate optimal font size for auto-sizing (best fit)
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

        // Extract bold and italic flags
        bool isBold = (text.fontStyle == UIText::FontStyle::BOLD) || 
                      (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);
        bool isItalic = (text.fontStyle == UIText::FontStyle::ITALIC) || 
                        (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);

        // Check if wrapping is enabled (horizontalOverflow == WRAP)
        bool shouldWrap = (text.horizontalOverflow == UIText::OverflowMode::WRAP);
        float maxWidth = shouldWrap ? containerWidth : 0.0f;

        // Binary search for optimal font size
        float minSize = text.minSize;
        float maxSize = text.maxSize;
        float bestSize = minSize;

        // Binary search
        const float epsilon = 0.5f;
        int iterations = 0;
        const int maxIterations = 20; // Prevent infinite loops

        while (maxSize - minSize > epsilon && iterations < maxIterations) {
            float testSize = (minSize + maxSize) * 0.5f;
            
            // Create temporary font to measure text
            std::shared_ptr<NE::Graphics::Font> testFont = std::make_shared<NE::Graphics::Font>();
            bool loaded = false;
            
            if (!fontData.empty()) {
                loaded = testFont->LoadFromBinaryData(fontData, testSize, isBold, isItalic);
            } else if (!fontPath.empty()) {
                loaded = testFont->LoadFromFile(fontPath, testSize, isBold, isItalic);
            } else {
                loaded = testFont->LoadFromFile("Assets/Fonts/Roboto-Regular.ttf", testSize, isBold, isItalic);
            }

            if (!loaded) {
                // If font can't be loaded, try smaller size
                maxSize = testSize;
                iterations++;
                continue;
            }

            // Measure text dimensions
            float textWidth = testFont->MeasureTextWidth(text.text);
            float textHeight = testFont->MeasureTextHeight(text.text, maxWidth);

            if (textWidth <= containerWidth && textHeight <= containerHeight) {
                // Text fits, try larger size
                bestSize = testSize;
                minSize = testSize;
            } else {
                // Text doesn't fit, try smaller size
                maxSize = testSize;
            }
            
            iterations++;
        }

        // Clamp to min/max bounds
        return std::max(text.minSize, std::min(text.maxSize, bestSize));
    }

    void UIRenderSystem::Init() {
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            // Load UIImage resources
            if (m_cm->HasComponent<UIImage>(e)) {
                auto& img = m_cm->GetComponent<UIImage>(e);

                if (!img.textureUUID.empty()) {
                    auto texture = NE::Resource::ResourceManager::GetInstance()
                        .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
                    if (texture) {
                        // Always cache texture dimensions (even if already loaded) for preserve aspect ratio
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

            // Load UIText fonts
            if (m_cm->HasComponent<UIText>(e)) {
                auto& text = m_cm->GetComponent<UIText>(e);

                // Extract bold and italic flags from fontStyle
                bool isBold = (text.fontStyle == UIText::FontStyle::BOLD) || 
                              (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);
                bool isItalic = (text.fontStyle == UIText::FontStyle::ITALIC) || 
                                (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);

                // Generate cache key from fontUUID, fontSize, and fontStyle
                uint32_t currentCacheKey = GenerateFontCacheKey(text.fontUUID, text.fontSize, text.fontStyle);
                
                // If fontHandle doesn't match current cache key, invalidate it
                if (text.fontHandle != 0 && text.fontHandle != currentCacheKey) {
                    text.fontHandle = 0;  // Invalidate to force reload
                }

                if (text.fontHandle == 0) {
                    // Check if font is already cached
                    auto cacheIt = m_fontCache.find(currentCacheKey);
                    if (cacheIt != m_fontCache.end()) {
                        text.fontHandle = currentCacheKey;
                    } else {
                        // Try to load font from cooked binary using UUID
                        std::shared_ptr<NE::Graphics::Font> font;
                        if (!text.fontUUID.empty()) {
                            // Load font binary data and create font with correct fontSize and style
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
                        
                        // If still no font, use default Roboto
                        if (!font) {
                            font = std::make_shared<NE::Graphics::Font>();
                            if (!font->LoadFromFile("Assets/Fonts/Roboto-Regular.ttf", text.fontSize, isBold, isItalic)) {
                                SPD_WARNING("Failed to load font for UIText entity: " << e);
                            } else {
                                // Cache the font only if it loaded successfully
                                m_fontCache[currentCacheKey] = font;
                                text.fontHandle = currentCacheKey;
                            }
                        } else {
                            // Cache the font loaded from UUID or file path
                            m_fontCache[currentCacheKey] = font;
                            text.fontHandle = currentCacheKey;
                        }
                    }
                }
            }
        }
    }

    void UIRenderSystem::OnEntityAdded(Entity e) {
        // Handle UIImage
        if (m_cm->HasComponent<UIImage>(e)) {
            auto& img = m_cm->GetComponent<UIImage>(e);

            if (!img.textureUUID.empty()) {
                auto texture = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
                if (texture) {
                    img.bindlessHandle = texture->GetBindlessHandle();
                    // Cache texture dimensions for preserve aspect ratio
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

        // Handle UIText
        if (m_cm->HasComponent<UIText>(e)) {
            auto& text = m_cm->GetComponent<UIText>(e);

            // Extract bold and italic flags from fontStyle
            bool isBold = (text.fontStyle == UIText::FontStyle::BOLD) || 
                          (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);
            bool isItalic = (text.fontStyle == UIText::FontStyle::ITALIC) || 
                            (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);

            // Generate cache key from fontUUID, fontSize, and fontStyle
            uint32_t currentCacheKey = GenerateFontCacheKey(text.fontUUID, text.fontSize, text.fontStyle);
            
            // If fontHandle doesn't match current cache key, invalidate it
            if (text.fontHandle != 0 && text.fontHandle != currentCacheKey) {
                text.fontHandle = 0;  // Invalidate to force reload
            }
            
            if (text.fontHandle == 0) {
                // Check if font is already cached
                auto cacheIt = m_fontCache.find(currentCacheKey);
                if (cacheIt != m_fontCache.end()) {
                    text.fontHandle = currentCacheKey;
                } else {
                    // Try to load font from cooked binary using UUID
                    std::shared_ptr<NE::Graphics::Font> font;
                    if (!text.fontUUID.empty()) {
                        // Load font binary data and create font with correct fontSize and style
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
                    
                    // If still no font, use default Roboto
                    if (!font) {
                        font = std::make_shared<NE::Graphics::Font>();
                        if (!font->LoadFromFile("Assets/Fonts/Roboto-Regular.ttf", text.fontSize, isBold, isItalic)) {
                            SPD_WARNING("Failed to load font for UIText entity: " << e);
                        } else {
                            // Cache the font only if it loaded successfully
                            m_fontCache[currentCacheKey] = font;
                            text.fontHandle = currentCacheKey;
                        }
                    } else {
                        // Cache the font loaded from UUID or file path
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
            // Collect entities with either UIImage OR UIText
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

        // Unity-style: worldTransform is already in top-left origin coordinates (Y-down)
        // worldTransform.x/y is the pivot position (calculated in AccumulateParentTransforms)
        // GenerateVertices expects top-left corner, so we calculate it from pivot position
        //
        // In Unity: pivot (0,0) = bottom-left, (0.5,0.5) = center, (1,1) = top-right
        // Formula: topLeft = pivot - (width * pivotX, height * (1 - pivotY))
        //   - X: pivotX_norm = 0.0 → left, 1.0 → right
        //   - Y: pivotY_norm = 0.0 → bottom, 1.0 → top (in Y-down: bottom Y > top Y)
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
            // Rotate vertices around the pivot point
            // worldTransform.x/y is already the pivot position in top-left origin coordinates
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
        // For world space, generate vertices with actual dimensions (GenerateVertices handles preserve aspect)
        // Then normalize to unit quad space (0,0 to 1,1) so the model matrix can scale correctly
        auto vertices = NE::Graphics::UIImageMeshGenerator::GenerateVertices(
            img,
            0.0f, 0.0f, 0.0f,
            width,
            height,
            img.color
        );
        
        // Normalize vertex positions to unit quad space (0,0 to 1,1)
        // GenerateVertices already applied preserve aspect with offsets, so normalizing preserves those offsets
        for (auto& vertex : vertices) {
            vertex.x /= width;
            vertex.y /= height;
        }
        
        // For world space, flip V coordinates to fix Y-axis
        // The mesh generator sets: y=0 (top) → V=0, y=1 (bottom) → V=1 (for screen space)
        // For world space in Y-down coordinate system, we need: y=0 (top) → V=1, y=1 (bottom) → V=0
        // So we flip V: v_flipped = 1.0f - v_original
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

        // BuildQuadVertices expects top-left corner, not pivot position
        // Calculate top-left from pivot: topLeft = pivot - (width * pivotX, height * (1 - pivotY))
        float topLeftX = worldTransform.x - worldTransform.width * rect.pivotX;
        float topLeftY = worldTransform.y - worldTransform.height * (1.0f - rect.pivotY);
        
        cmd.x = topLeftX;
        cmd.y = topLeftY;
        cmd.z = worldTransform.z;
        cmd.width = worldTransform.width;
        cmd.height = worldTransform.height;
        
        // Apply button state color if this entity has a button component
        // Unity pattern: Final Color = UIImage Color * Button State Color
        // This allows UIImage color to act as a base tint, and button states to modify it
        NE::Math::Vec4 finalColor = img.color;
        if (m_cm->HasComponent<UIButton>(entity)) {
            const auto& button = m_cm->GetComponent<UIButton>(entity);
            if (button.transitionType == UIButton::TransitionType::COLOR_TINT) {
                // Get the button color based on current state
                // GetCurrentColor() handles interactable check and returns disabledColor if not interactable
                NE::Math::Vec4 buttonColor = button.GetCurrentColor();
                
                // Multiply image color by button state color (component-wise)
                // This matches Unity's behavior: base image color is tinted by button state
                // Example: UIImage color (0.8, 0.8, 0.8) * Button hover (0.5, 1.0, 0.5) = (0.4, 0.8, 0.4)
                finalColor.x = img.color.x * buttonColor.x;
                finalColor.y = img.color.y * buttonColor.y;
                finalColor.z = img.color.z * buttonColor.z;
                finalColor.w = img.color.w * buttonColor.w;
            }
        }
        cmd.color = finalColor;
        cmd.order = canvas.sortingOrder;
        cmd.entityId = entity;
        cmd.renderMode = static_cast<int>(canvas.renderMode);
        cmd.planeDistance = canvas.planeDistance;

        cmd.material = img.material;
        cmd.bindlessTextureHandle = img.bindlessHandle;

        cmd.vertices = vertices;
        // For world space, always use custom vertices to ensure Y-axis flip is applied
        // For screen space, use custom vertices for special cases (filled, sliced, tiled, preserve aspect, rotation)
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

            // Handle UIImage
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
            // Handle UIText
            else if (m_cm->HasComponent<UIText>(e)) {
                auto& text = m_cm->GetComponent<UIText>(e);

                // Auto-size (best fit) calculation
                if (text.bestFit && worldTransform.width > 0.0f && worldTransform.height > 0.0f) {
                    // Load font data for measurement
                    std::vector<uint8_t> fontData;
                    std::string fontPath;
                    
                    if (!text.fontUUID.empty()) {
                        std::string artifactPath = NE::Resource::ComputeFontArtifactPathFromUUID(text.fontUUID);
                        std::ifstream fontFile(artifactPath, std::ios::binary | std::ios::ate);
                        if (fontFile) {
                            std::streamsize fileSize = fontFile.tellg();
                            if (fileSize > 0) {
                                fontFile.seekg(0, std::ios::beg);
                                fontData.resize(static_cast<size_t>(fileSize));
                                fontFile.read(reinterpret_cast<char*>(fontData.data()), fileSize);
                            }
                            fontFile.close();
                        }
                    } else if (!text.fontPath.empty()) {
                        fontPath = text.fontPath;
                    } else {
                        fontPath = "Assets/Fonts/Roboto-Regular.ttf";
                    }

                    // Calculate optimal font size
                    float optimalSize = CalculateOptimalFontSize(
                        text,
                        worldTransform.width,
                        worldTransform.height,
                        fontPath,
                        fontData
                    );

                    // Update fontSize if it changed
                    if (std::abs(text.fontSize - optimalSize) > 0.1f) {
                        text.fontSize = optimalSize;
                        text.fontHandle = 0; // Invalidate to force reload with new size
                    }
                }

                // Extract bold and italic flags from fontStyle
                bool isBold = (text.fontStyle == UIText::FontStyle::BOLD) || 
                              (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);
                bool isItalic = (text.fontStyle == UIText::FontStyle::ITALIC) || 
                                (text.fontStyle == UIText::FontStyle::BOLD_AND_ITALIC);

                // Generate cache key from fontUUID, fontSize, and fontStyle
                uint32_t currentCacheKey = GenerateFontCacheKey(text.fontUUID, text.fontSize, text.fontStyle);
                
                // Get font from cache
                std::shared_ptr<NE::Graphics::Font> font;
                
                if (text.fontHandle != 0) {
                    // Check if cached font matches current cache key
                    if (text.fontHandle == currentCacheKey) {
                        auto cacheIt = m_fontCache.find(static_cast<uint32_t>(text.fontHandle));
                        if (cacheIt != m_fontCache.end()) {
                            font = cacheIt->second;
                        }
                    } else {
                        // Font UUID, size, or style changed - invalidate old cache entry
                        text.fontHandle = 0;
                    }
                }
                
                // If not in cache, load new font
                if (!font) {
                    auto cacheIt = m_fontCache.find(currentCacheKey);
                    if (cacheIt != m_fontCache.end()) {
                        // Font already cached
                        font = cacheIt->second;
                        text.fontHandle = currentCacheKey;
                    } else {
                        // Try to load font from cooked binary using UUID
                        if (!text.fontUUID.empty()) {
                            // Load font binary data and create font with correct fontSize and style
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
                        
                        // If still no font, use default Roboto
                        if (!font) {
                            font = std::make_shared<NE::Graphics::Font>();
                            if (!font->LoadFromFile("Assets/Fonts/Roboto-Regular.ttf", text.fontSize, isBold, isItalic)) {
                                continue; // Skip if font can't be loaded
                            }
                        }
                        
                        // Cache the font
                        m_fontCache[currentCacheKey] = font;
                        text.fontHandle = currentCacheKey;
                    }
                }

                // Safety check: ensure font is valid
                if (!font) {
                    continue; // Skip if font is invalid
                }

                // Calculate top-left position from pivot
                float topLeftX = worldTransform.x - worldTransform.width * rect.pivotX;
                float topLeftY = worldTransform.y - worldTransform.height * (1.0f - rect.pivotY);

                // Generate text vertices
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

                if (!vertices.empty()) {
                    // Apply rotation if needed
                    if (std::abs(worldTransform.accumulatedRotationZ) > ROTATION_EPSILON) {
                        float pivotX = worldTransform.x;
                        float pivotY = worldTransform.y;
                        RotateVertices2D(vertices, pivotX, pivotY, worldTransform.accumulatedRotationZ);
                    }

                    // Submit text draw command
                    NE::Graphics::UIDrawCommand cmd;
                    cmd.x = topLeftX;
                    cmd.y = topLeftY;
                    cmd.z = worldTransform.z;
                    cmd.width = worldTransform.width;
                    cmd.height = worldTransform.height;
                    cmd.color = text.color;
                    cmd.order = canvas.sortingOrder;
                    cmd.entityId = e;
                    cmd.renderMode = static_cast<int>(canvas.renderMode);
                    cmd.planeDistance = canvas.planeDistance;
                    cmd.vertices = vertices;
                    cmd.useCustomVertices = true;

                    // Get font atlas texture handle
                    cmd.bindlessTextureHandle = font->GetAtlasTextureHandle();

                    if (viewMatrix) cmd.viewMatrix = *viewMatrix;
                    if (projMatrix) cmd.projMatrix = *projMatrix;

                    if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE && m_transformSystem) {
                        cmd.modelMatrix = m_transformSystem->BuildWorldSpaceModelMatrix(e, canvasEntity, rect, accumulated);
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

            // Calculate scale factor based on scale mode
            if (m_transformSystem) {
                canvas.scaleFactor = m_transformSystem->CalculateScaleFactor(canvas);

                // Setup canvas defaults based on render mode
                m_transformSystem->SetupCanvasDefaults(canvasEntity, canvas);
            }

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
            }

            RenderCanvasChildren(canvasEntity, canvas, pView, pProj);
        }
    }

} // namespace NE::ECS::Systems