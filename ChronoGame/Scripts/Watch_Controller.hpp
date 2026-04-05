#pragma once
#include "EngineAPI.hpp"
#include "ScriptSDK//UI.h"
#include <cstdio>
/*
* Watch_Controller - Overlay swap + clock fill
* Press Q to toggle between present (green) and past (blue).
* Clock drains while in past, refills while in present.
* Auto-switches back to present when depleted.
*
* Inspector fields:
*   pastDuration   - seconds at full charge before depletion (default 10s)
*   refillDuration - seconds from empty to full (default 5s)
*   chronoTextRef  - UIText entity to display remaining time
*/

class Watch_Controller : public IScript {
public:
    GameObjectRef presentOverlayRef;
    GameObjectRef pastOverlayRef;
    GameObjectRef clockFillRef;
    GameObjectRef chronoTextRef;
    GameObjectRef defaultOutlineRef;
    GameObjectRef redOutlineRef;

    Watch_Controller() {
        SCRIPT_GAMEOBJECT_REF(presentOverlayRef);
        SCRIPT_GAMEOBJECT_REF(pastOverlayRef);
        SCRIPT_GAMEOBJECT_REF(clockFillRef);
        SCRIPT_GAMEOBJECT_REF(chronoTextRef);
        SCRIPT_GAMEOBJECT_REF(defaultOutlineRef);
        SCRIPT_GAMEOBJECT_REF(redOutlineRef);
        SCRIPT_FIELD(pastDuration, Float);
        SCRIPT_FIELD(refillDuration, Float);
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

        if (chronoTextRef.IsValid())
            m_chronoText = chronoTextRef.GetEntity();
        else
            LOG_WARNING("Watch_Controller: chronoTextRef not set.");

        if (defaultOutlineRef.IsValid())
            m_defaultOutline = defaultOutlineRef.GetEntity();
        else
            LOG_WARNING("Watch_Controller: defaultOutlineRef not set.");

        if (redOutlineRef.IsValid())
            m_redOutline = redOutlineRef.GetEntity();
        else
            LOG_WARNING("Watch_Controller: redOutlineRef not set.");

        if (pastDuration <= 0.0f) pastDuration = 10.0f;
        if (refillDuration <= 0.0f) refillDuration = 5.0f;

        isPast = false;
        fillValue = 1.0f;
        m_pendingDepletion = false;
        m_lockPastForever = false;
        m_isRedOutlineActive = false;

        ApplyOverlayState();
        ApplyClockFill();
        ApplyOutlineState();
        UpdateChronoText();

        LOG_INFO("Watch_Controller: Ready. Press Q to toggle. Drain=" +
            std::to_string(pastDuration) + "s, Refill=" +
            std::to_string(refillDuration) + "s");
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

        // When locked, the player is forced to remain in the past forever.
        // Ignore manual toggles and skip drain/refill entirely.
        if (!m_lockPastForever) {
            // Toggle on Q press
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
                fillValue -= (1.0f / pastDuration) * dt;
                if (fillValue < 0.0f) {
                    fillValue = 0.0f;
                    m_pendingDepletion = true;
                    LOG_INFO("Watch_Controller: Fill depleted, deferring state change to next frame");
                }
            }
            else {
                fillValue += (1.0f / refillDuration) * dt;
                if (fillValue > 1.0f)
                    fillValue = 1.0f;
            }
        }

        ApplyClockFill();
        ApplyOutlineState(dt);
        UpdateChronoText();
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

    bool IsPastActive() const { return isPast; }
    bool IsPastLockedForever() const { return m_lockPastForever; }

    void ForcePastForever(bool refillClockToFull = true) {
        const bool wasPast = isPast;

        m_lockPastForever = true;
        m_pendingDepletion = false;
        isPast = true;

        if (refillClockToFull) {
            fillValue = 1.0f;
        }

        ApplyOverlayState();
        ApplyClockFill();
        ApplyOutlineState();
        UpdateChronoText();

        if (!wasPast) {
            DeferEvent("ChronoActivated");
            PlayAudio("event:/SWITCH_TO_PAST");
        }

        LOG_INFO("Watch_Controller: Time locked to PAST forever");
    }

    void ClearForcedPastLock() {
        m_lockPastForever = false;
        m_pendingDepletion = false;
        LOG_INFO("Watch_Controller: Permanent past lock cleared");
    }

private:
    Entity m_presentOverlay = 0;
    Entity m_pastOverlay = 0;
    Entity m_clockFill = 0;
    Entity m_chronoText = 0;
    Entity m_defaultOutline = 0;
    Entity m_redOutline = 0;
    bool isPast = false;
    float fillValue = 1.0f;
    bool m_pendingDepletion = false;
    bool m_lockPastForever = false;
    bool m_isRedOutlineActive = false;
    float m_blinkTimer = 0.0f;
    bool m_blinkVisible = true;

    // Inspector fields
    float pastDuration = 10.0f;
    float refillDuration = 5.0f;
    static constexpr float BLINK_INTERVAL = 0.25f; // seconds per blink toggle

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

    void ApplyOutlineState(float dt = 0.0f) {
        bool shouldBeRed = (fillValue < 0.3f);

        if (shouldBeRed != m_isRedOutlineActive) {
            m_isRedOutlineActive = shouldBeRed;
            m_blinkTimer = 0.0f;
            m_blinkVisible = true;

            // Hide default outline when entering red zone
            if (m_defaultOutline != 0)
                SetActive(!shouldBeRed, m_defaultOutline);
        }

        if (m_isRedOutlineActive) {
            // Blink the red outline
            m_blinkTimer += dt;
            if (m_blinkTimer >= BLINK_INTERVAL) {
                m_blinkTimer -= BLINK_INTERVAL;
                m_blinkVisible = !m_blinkVisible;
            }

            if (m_redOutline != 0)
                SetActive(m_blinkVisible, m_redOutline);
        }
        else {
            // Not in red zone — red off, default on
            if (m_redOutline != 0)
                SetActive(false, m_redOutline);
        }
    }

    void UpdateChronoText() {
        if (m_chronoText == 0) return;

        if (m_lockPastForever) {
            NE::Scripting::SetUIText(m_chronoText, "LOCKED");
            return;
        }

        // Remaining seconds based on fill and pastDuration
        float remainingSeconds = fillValue * pastDuration;

        char buf[16];
        snprintf(buf, sizeof(buf), "%.1fs", remainingSeconds);
        NE::Scripting::SetUIText(m_chronoText, buf);
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