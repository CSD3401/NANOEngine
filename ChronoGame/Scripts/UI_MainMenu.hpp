#pragma once
#include "EngineAPI.hpp"
#include <ScriptSDK/UI.h>

/*
* UI_MainMenu:
* - ensures the cursor is visible and unlocked in the main menu
* - fades OUT a black overlay canvas on scene start
*
* Setup:
* - Attach to any entity in MainMenu scene
* - Assign one UICanvas entity in blackOverlayCanvas (full-screen black)
*/
class UI_MainMenu : public IScript {
public:
    UI_MainMenu() {
        SCRIPT_GAMEOBJECT_REF(blackOverlayCanvas);
        SCRIPT_FIELD(enableFadeOut, Bool);
        SCRIPT_FIELD(fadeOutDuration, Float);
    }
    ~UI_MainMenu() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        Input::SetMouseLocked(false);
        NE::Scripting::SetMouseVisible(true);

        m_phase = Phase::DONE;
        m_timer = 0.0f;

        if (!enableFadeOut) return;

        m_overlayEntity = ResolveOverlayCanvas();
        if (m_overlayEntity == 0) return;
        if (fadeOutDuration <= 0.0f) fadeOutDuration = 2.0f;

        // Start from full black.
        SetActive(true, m_overlayEntity);
        auto& canvas = NE::ECS::Command::GetUICanvas(m_overlayEntity);
        canvas.isActive = true;
        NE::ECS::Command::SetUICanvasAlpha(m_overlayEntity, 1.0f);

        m_timer = 0.0f;
        m_phase = Phase::FADE_OUT;
    }

    void Update(double deltaTime) override {
        if (m_phase == Phase::DONE || m_overlayEntity == 0) return;

        if (m_phase == Phase::FADE_OUT) {
            m_timer += static_cast<float>(deltaTime);
            const float t = (fadeOutDuration > 0.0f)
                ? std::min(1.0f, m_timer / fadeOutDuration)
                : 1.0f;
            const float alpha = 1.0f - t;

            NE::ECS::Command::SetUICanvasAlpha(m_overlayEntity, alpha);
            if (t >= 1.0f - 1e-4f) {
                NE::ECS::Command::SetUICanvasAlpha(m_overlayEntity, 0.0f);
                // Hide overlay after fade so it no longer blocks raycasts.
                SetActive(false, m_overlayEntity);
                m_phase = Phase::DONE;
            }
        }
    }
    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "UI_MainMenu"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef blackOverlayCanvas;
    bool enableFadeOut = true;
    float fadeOutDuration = 2.0f;

    Entity m_overlayEntity = 0;
    enum class Phase { FADE_OUT, DONE };
    Phase m_phase = Phase::DONE;
    float m_timer = 0.0f;

    Entity ResolveOverlayCanvas() const {
        if (!blackOverlayCanvas.IsValid()) {
            LOG_WARNING("UI_MainMenu: blackOverlayCanvas is not assigned");
            return 0;
        }
        const Entity e = blackOverlayCanvas.GetEntity();
        if (e == 0) {
            LOG_WARNING("UI_MainMenu: blackOverlayCanvas reference is unresolved");
            return 0;
        }
        if (!NE::ECS::Query::HasUICanvas(e)) {
            LOG_WARNING("UI_MainMenu: blackOverlayCanvas has no UICanvas");
            return 0;
        }
        return e;
    }

};
