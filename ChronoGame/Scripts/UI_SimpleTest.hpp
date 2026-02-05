#pragma once
#include "EngineAPI.hpp"

/**
 * UI_SimpleTest - Minimal script to test UI rendering
 *
 * Attach this to any entity and run the game.
 * It will create a colored rectangle in the center of the screen.
 */
class UI_SimpleTest : public IScript {
public:
    uint32_t canvasEntity = 0;
    uint32_t imageEntity = 0;
    bool uiCreated = false;

    UI_SimpleTest() {}
    ~UI_SimpleTest() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        if (!uiCreated) {
            CreateTestUI();
            uiCreated = true;
        }
    }

    void CreateTestUI() {
        // Create canvas
        canvasEntity = Command::CreateUICanvas();

        auto& canvas = Command::GetUICanvas(canvasEntity);
        canvas.renderMode = Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY;
        canvas.scaleMode = Component::UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE;
        canvas.referenceWidth = 1920.0f;
        canvas.referenceHeight = 1080.0f;
        canvas.isActive = true;

        // Create image
        imageEntity = Command::CreateUIImage(canvasEntity);

        // Position: center of screen, 200x200 pixels
        auto& rect = Command::GetUIRectTransform(imageEntity);
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.width = 200.0f;
        rect.height = 200.0f;
        rect.pivotX = 0.5f;
        rect.pivotY = 0.5f;

        // Color: solid red (no texture needed)
        auto& img = Command::GetUIImage(imageEntity);
        img.imageType = Component::UIImage::ImageType::SIMPLE;
        Command::SetUIImageColor(imageEntity, 1.0f, 0.0f, 0.0f, 1.0f);  // Red

        LOG_INFO("UI_SimpleTest: Created canvas and image");
    }

    void Update(double deltaTime) override {
        // Press R/G/B to change color
        if (imageEntity != 0) {
            if (Input::WasKeyPressed('R')) {
                Command::SetUIImageColor(imageEntity, 1.0f, 0.0f, 0.0f, 1.0f);
                LOG_INFO("Color: Red");
            }
            if (Input::WasKeyPressed('G')) {
                Command::SetUIImageColor(imageEntity, 0.0f, 1.0f, 0.0f, 1.0f);
                LOG_INFO("Color: Green");
            }
            if (Input::WasKeyPressed('B')) {
                Command::SetUIImageColor(imageEntity, 0.0f, 0.0f, 1.0f, 1.0f);
                LOG_INFO("Color: Blue");
            }
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "UI_SimpleTest"; }

    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnCollisionStay(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}
    void OnTriggerStay(Entity other) override {}
};
