#pragma once
#include "EngineAPI.hpp"

/**
 * UI_TestImage - Self-contained UI texture swapping demo
 *
 * USAGE:
 * 1. Attach this script to any entity
 * 2. Set texture UUIDs in the inspector (textureState0 through textureState5)
 * 3. Press Play
 * 4. Press number keys 0-5 to swap between textures
 *
 * The script automatically creates its own canvas and image.
 */
class UI_TestImage : public IScript {
public:
    // Texture UUIDs for each time state (0-5)
    // Set these in the inspector to your actual texture asset UUIDs
    std::string textureState0 = "";
    std::string textureState1 = "";
    std::string textureState2 = "";
    std::string textureState3 = "";
    std::string textureState4 = "";
    std::string textureState5 = "";

    // Current time state (0-5)
    int currentTimeState = 0;

    // Image size and position (configurable in inspector)
    float imageWidth = 200.0f;
    float imageHeight = 200.0f;
    float imagePosX = 0.0f;  // 0 = center
    float imagePosY = 0.0f;  // 0 = center

    UI_TestImage() {
        // Register fields for editor exposure
        SCRIPT_FIELD(textureState0, String);
        SCRIPT_FIELD(textureState1, String);
        SCRIPT_FIELD(textureState2, String);
        SCRIPT_FIELD(textureState3, String);
        SCRIPT_FIELD(textureState4, String);
        SCRIPT_FIELD(textureState5, String);
        SCRIPT_FIELD(currentTimeState, Int);
        SCRIPT_FIELD(imageWidth, Float);
        SCRIPT_FIELD(imageHeight, Float);
        SCRIPT_FIELD(imagePosX, Float);
        SCRIPT_FIELD(imagePosY, Float);
    }

    ~UI_TestImage() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        CreateUI();
        ApplyTimeState(currentTimeState);
    }

    void Update(double deltaTime) override {
        if (m_imageEntity == 0) return;

        // Press number keys 0-5 to change texture
        if (Input::WasKeyPressed('0')) SetTimeState(0);
        if (Input::WasKeyPressed('1')) SetTimeState(1);
        if (Input::WasKeyPressed('2')) SetTimeState(2);
        if (Input::WasKeyPressed('3')) SetTimeState(3);
        if (Input::WasKeyPressed('4')) SetTimeState(4);
        if (Input::WasKeyPressed('5')) SetTimeState(5);

        // Press R/G/B to tint the image
        if (Input::WasKeyPressed('R')) {
            Command::SetUIImageColor(m_imageEntity, 1.0f, 0.5f, 0.5f, 1.0f);
            LOG_INFO("Tint: Red");
        }
        if (Input::WasKeyPressed('G')) {
            Command::SetUIImageColor(m_imageEntity, 0.5f, 1.0f, 0.5f, 1.0f);
            LOG_INFO("Tint: Green");
        }
        if (Input::WasKeyPressed('B')) {
            Command::SetUIImageColor(m_imageEntity, 0.5f, 0.5f, 1.0f, 1.0f);
            LOG_INFO("Tint: Blue");
        }
        if (Input::WasKeyPressed('W')) {
            Command::SetUIImageColor(m_imageEntity, 1.0f, 1.0f, 1.0f, 1.0f);
            LOG_INFO("Tint: White (reset)");
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "UI_TestImage";
    }

    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnCollisionStay(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}
    void OnTriggerStay(Entity other) override {}

    // === Public Methods ===

    void SetTimeState(int newState) {
        if (newState < 0) newState = 0;
        if (newState > 5) newState = 5;

        if (newState != currentTimeState) {
            currentTimeState = newState;
            ApplyTimeState(newState);
            LOG_INFO("Time state: " << newState);
        }
    }

    int GetTimeState() const {
        return currentTimeState;
    }

private:
    uint32_t m_canvasEntity = 0;
    uint32_t m_imageEntity = 0;

    void CreateUI() {
        // Create canvas
        m_canvasEntity = Command::CreateUICanvas();

        auto& canvas = Command::GetUICanvas(m_canvasEntity);
        canvas.renderMode = Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY;
        canvas.scaleMode = Component::UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE;
        canvas.referenceWidth = 1920.0f;
        canvas.referenceHeight = 1080.0f;
        canvas.isActive = true;

        // Create image
        m_imageEntity = Command::CreateUIImage(m_canvasEntity);

        auto& rect = Command::GetUIRectTransform(m_imageEntity);
        rect.x = imagePosX;
        rect.y = imagePosY;
        rect.width = imageWidth;
        rect.height = imageHeight;
        rect.pivotX = 0.5f;
        rect.pivotY = 0.5f;

        auto& img = Command::GetUIImage(m_imageEntity);
        img.imageType = Component::UIImage::ImageType::SIMPLE;

        LOG_INFO("UI_TestImage: Created canvas and image");
    }

    void ApplyTimeState(int state) {
        if (m_imageEntity == 0) return;

        const std::string& textureUUID = GetTextureForState(state);

        if (!textureUUID.empty()) {
            Command::SetUIImageTexture(m_imageEntity, textureUUID);
        } else {
            // No texture set - show solid color based on state
            float r = (state == 0 || state == 3) ? 1.0f : 0.3f;
            float g = (state == 1 || state == 4) ? 1.0f : 0.3f;
            float b = (state == 2 || state == 5) ? 1.0f : 0.3f;
            Command::SetUIImageColor(m_imageEntity, r, g, b, 1.0f);
        }
    }

    const std::string& GetTextureForState(int state) const {
        switch (state) {
            case 0: return textureState0;
            case 1: return textureState1;
            case 2: return textureState2;
            case 3: return textureState3;
            case 4: return textureState4;
            case 5: return textureState5;
            default: return textureState0;
        }
    }
};
