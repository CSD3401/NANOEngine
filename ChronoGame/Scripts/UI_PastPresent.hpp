#pragma once
#include "EngineAPI.hpp"

/**
 * UI_PastPresent - Simple demo to swap between Past and Present textures
 *
 * USAGE:
 * 1. Attach to any entity
 * 2. Press Play
 * 3. Press P = Past, O = Present (or 1/2)
 */
class UI_PastPresent : public IScript {
public:
    // Hardcoded texture UUIDs (from your Assets/Textures folder)
    const std::string PAST_TEXTURE = "a2d4be05-8860-4de0-bfa1-a4c867c21136";
    const std::string PRESENT_TEXTURE = "29f22585-6492-4267-b7eb-c2bf7271be1b";

    // Which state are we in? (false = present, true = past)
    bool isPast = false;

    UI_PastPresent() {}
    ~UI_PastPresent() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        CreateUI();
        // Start with Present
        ShowPresent();
    }

    void Update(double deltaTime) override {
        if (m_imageEntity == 0) return;

        // Press P or 1 for Past
        if (Input::WasKeyPressed('P') || Input::WasKeyPressed('1')) {
            ShowPast();
        }
        // Press O or 2 for Present
        if (Input::WasKeyPressed('O') || Input::WasKeyPressed('2')) {
            ShowPresent();
        }
        // Press Space to toggle
        if (Input::WasKeyPressed(' ')) {
            if (isPast) {
                ShowPresent();
            } else {
                ShowPast();
            }
        }
    }

    void ShowPast() {
        if (m_imageEntity == 0) {
            LOG_INFO("ERROR: No image entity!");
            return;
        }
        bool success = Command::SetUIImageTexture(m_imageEntity, PAST_TEXTURE.c_str());
        isPast = true;
        LOG_INFO("Showing: PAST (texture load: " << (success ? "OK" : "FAILED") << ")");
    }

    void ShowPresent() {
        if (m_imageEntity == 0) {
            LOG_INFO("ERROR: No image entity!");
            return;
        }
        bool success = Command::SetUIImageTexture(m_imageEntity, PRESENT_TEXTURE.c_str());
        isPast = false;
        LOG_INFO("Showing: PRESENT (texture load: " << (success ? "OK" : "FAILED") << ")");
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "UI_PastPresent";
    }

    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnCollisionStay(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}
    void OnTriggerStay(Entity other) override {}

private:
    uint32_t m_canvasEntity = 0;
    uint32_t m_imageEntity = 0;

    void CreateUI() {
        // Create canvas
        m_canvasEntity = Command::CreateUICanvas();
        LOG_INFO("Created canvas entity: " << m_canvasEntity);

        auto& canvas = Command::GetUICanvas(m_canvasEntity);
        canvas.renderMode = Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY;
        canvas.scaleMode = Component::UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE;
        canvas.referenceWidth = 1920.0f;
        canvas.referenceHeight = 1080.0f;
        canvas.isActive = true;

        // Create image - larger size to see the texture clearly
        m_imageEntity = Command::CreateUIImage(m_canvasEntity);
        LOG_INFO("Created image entity: " << m_imageEntity);

        auto& rect = Command::GetUIRectTransform(m_imageEntity);
        rect.x = 0.0f;      // Center
        rect.y = 0.0f;
        rect.width = 400.0f;
        rect.height = 400.0f;
        rect.pivotX = 0.5f;
        rect.pivotY = 0.5f;

        auto& img = Command::GetUIImage(m_imageEntity);
        img.imageType = Component::UIImage::ImageType::SIMPLE;
        img.color = NE::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);  // White (no tint)

        LOG_INFO("UI_PastPresent: Created UI - Press P=Past, O=Present, Space=Toggle");
    }
};
