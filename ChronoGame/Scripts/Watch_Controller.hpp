#pragma once
#include "EngineAPI.hpp"
#include "ScriptSDK//UI.h"
/*
* Watch_Controller - Overlay swap + clock fill
* Press E to toggle between present (green) and past (blue).
* Clock drains while in past, refills while in present.
* Auto-switches back to present when depleted.
*
* All state changes (overlay swap + events) that could conflict
* with ICO switcher are deferred via coroutine.
*/

class Watch_Controller : public IScript {
public:
    GameObjectRef presentOverlayRef;
    GameObjectRef pastOverlayRef;
    GameObjectRef clockFillRef;

    Watch_Controller() {
        SCRIPT_GAMEOBJECT_REF(presentOverlayRef);
        SCRIPT_GAMEOBJECT_REF(pastOverlayRef);
        SCRIPT_GAMEOBJECT_REF(clockFillRef);
    }
    ~Watch_Controller() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        if (presentOverlayRef.IsValid())
            m_presentOverlay = presentOverlayRef.GetEntity();
        else
            LOG_WARNING("Watch_Controller: presentOverlayRef not set.");

        if (pastOverlayRef.IsValid())
            m_pastOverlay = pastOverlayRef.GetEntity();
        else
            LOG_WARNING("Watch_Controller: pastOverlayRef not set.");

        if (clockFillRef.IsValid())
            m_clockFill = clockFillRef.GetEntity();
        else
            LOG_WARNING("Watch_Controller: clockFillRef not set.");

        isPast = false;
        fillValue = 1.0f;
        m_pendingDepletion = false;

        ApplyOverlayState();
        ApplyClockFill();

        LOG_INFO("Watch_Controller: Ready. Press Q to toggle past/present.");

        //DeferEvent("StartCutscene"); // I jus put here first - play cutscene on run
    }

    void Update(double deltaTime) override {
        float dt = static_cast<float>(deltaTime);

        // Handle deferred depletion from previous frame
        if (m_pendingDepletion) {
            m_pendingDepletion = false;
            isPast = false;
            ApplyOverlayState();
            DeferEvent("ChronoDeactivated");
            PlayAudio("event:/SWITCH_TO_PRESENT");
            LOG_INFO("Watch_Controller: Depletion applied, back to PRESENT");
        }

        // Toggle on E press
        if (Input::WasKeyPressed('Q')) {
            isPast = !isPast;
            ApplyOverlayState();

            if (isPast) {
                DeferEvent("ChronoActivated");
                PlayAudio("event:/SWITCH_TO_PAST");
                LOG_INFO("Watch_Controller: Switched to PAST");
            }
            else {
                DeferEvent("ChronoDeactivated");
                PlayAudio("event:/SWITCH_TO_PRESENT");
                LOG_INFO("Watch_Controller: Switched to PRESENT");
            }
        }

        // Drain in past, refill in present
        if (isPast) {
            fillValue -= 0.5f * dt;
            if (fillValue < 0.0f) {
                fillValue = 0.0f;
                // Don't change state now — defer to next frame
                m_pendingDepletion = true;
                LOG_INFO("Watch_Controller: Fill depleted, deferring state change to next frame");
            }
        }
        else {
            fillValue += 0.3f * dt;
            if (fillValue > 1.0f)
                fillValue = 1.0f;
        }

        ApplyClockFill();
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Watch_Controller"; }
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Entity m_presentOverlay = 0;
    Entity m_pastOverlay = 0;
    Entity m_clockFill = 0;
    bool isPast = false;
    float fillValue = 1.0f;
    bool m_pendingDepletion = false;

    void ApplyOverlayState() {
        if (m_presentOverlay != 0)
            SetActive(!isPast, m_presentOverlay);
        if (m_pastOverlay != 0)
            SetActive(isPast, m_pastOverlay);
    }

    void ApplyClockFill() {
        if (m_clockFill != 0)
            NE::ECS::Command::SetUIImageFillAmount(m_clockFill, fillValue);
    }

    void DeferEvent(const char* eventName) {
        std::string name(eventName);
        Coroutines::Handle h = Coroutines::Create();
        Coroutines::AddWait(h, 0.0f);
        Coroutines::AddAction(h, [name]() {
            Events::Send(name.c_str());
            });
        Coroutines::Start(h);
    }
};