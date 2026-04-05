#pragma once
#include "EngineAPI.hpp"
#include <ScriptSDK/UI.h>
#include <algorithm>

/**
 * UI_OverlayFadeIn
 * ----------------
 * Fades a full-screen UICanvas from alpha 0 -> 1. Does not load scenes.
 *
 * Triggers:
 * - Button: enableButtonTrigger + click on triggerButton (if set) or the entity this script is on.
 * - Event: non-empty listenEventName -> Events::Send that name from elsewhere.
 *
 * Pair with UIButton_SwitchSceneTwo/Three: set switchDelaySeconds to match fadeInDuration.
 */
class UI_OverlayFadeIn : public IScript {
public:
    UI_OverlayFadeIn() {
        SCRIPT_GAMEOBJECT_REF(blackOverlayCanvas);
        SCRIPT_GAMEOBJECT_REF(triggerButton);
        SCRIPT_FIELD(fadeInDuration, Float);
        SCRIPT_FIELD(listenEventName, String);
        SCRIPT_FIELD(enableButtonTrigger, Bool);
    }

    ~UI_OverlayFadeIn() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { m_hostEntity = entity; }

    void Start() override {
        if (fadeInDuration <= 0.0f) fadeInDuration = 2.0f;

        const std::string evt = Trim(listenEventName);
        if (!evt.empty()) {
            Events::Listen(evt.c_str(), [this](void*) {
                if (m_phase != Phase::Idle) return;
                BeginFadeIn();
                });
        }
    }

    void Update(double deltaTime) override {
        const Entity buttonEntity = ResolveTriggerButtonEntity();
        if (m_phase == Phase::Idle && enableButtonTrigger && buttonEntity != 0 &&
            NE::Scripting::WasButtonClicked(buttonEntity) &&
            NE::Scripting::IsButtonInteractable(buttonEntity)) {
            BeginFadeIn();
        }

        if (m_phase != Phase::Fading) return;

        m_timer += static_cast<float>(deltaTime);
        const float t = (fadeInDuration > 0.0f)
            ? std::min(1.0f, m_timer / fadeInDuration)
            : 1.0f;
        NE::ECS::Command::SetUICanvasAlpha(m_overlayEntity, t);
        if (t >= 1.0f - 1e-4f) {
            NE::ECS::Command::SetUICanvasAlpha(m_overlayEntity, 1.0f);
            m_phase = Phase::Done;
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UI_OverlayFadeIn"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef blackOverlayCanvas;
    GameObjectRef triggerButton;
    std::string listenEventName;
    float fadeInDuration = 2.0f;
    bool enableButtonTrigger = true;

    Entity m_hostEntity = 0;
    Entity m_overlayEntity = 0;
    enum class Phase { Idle, Fading, Done };
    Phase m_phase = Phase::Idle;
    float m_timer = 0.0f;

    static std::string Trim(const std::string& value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    Entity ResolveTriggerButtonEntity() const {
        if (triggerButton.IsValid()) {
            const Entity e = triggerButton.GetEntity();
            if (e != 0 && NE::ECS::Query::HasUIButton(e)) return e;
        }
        if (m_hostEntity != 0 && NE::ECS::Query::HasUIButton(m_hostEntity)) return m_hostEntity;
        return 0;
    }

    Entity ResolveOverlayCanvas() const {
        if (!blackOverlayCanvas.IsValid()) {
            LOG_WARNING("UI_OverlayFadeIn: blackOverlayCanvas is not assigned");
            return 0;
        }
        const Entity e = blackOverlayCanvas.GetEntity();
        if (e == 0) {
            LOG_WARNING("UI_OverlayFadeIn: blackOverlayCanvas reference is unresolved");
            return 0;
        }
        if (!NE::ECS::Query::HasUICanvas(e)) {
            LOG_WARNING("UI_OverlayFadeIn: blackOverlayCanvas has no UICanvas");
            return 0;
        }
        return e;
    }

    void BeginFadeIn() {
        PlayAudio("Event:/MAIN_MENU/BUTTON_CLICK");

        m_overlayEntity = ResolveOverlayCanvas();
        if (m_overlayEntity == 0) return;

        SetActive(true, m_overlayEntity);
        auto& canvas = NE::ECS::Command::GetUICanvas(m_overlayEntity);
        canvas.isActive = true;
        NE::ECS::Command::SetUICanvasAlpha(m_overlayEntity, 0.0f);
        m_timer = 0.0f;
        m_phase = Phase::Fading;
    }
};