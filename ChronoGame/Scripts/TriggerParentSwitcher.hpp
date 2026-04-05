#pragma once
#include "EngineAPI.hpp"

/**
 * TriggerParentSwitcher
 * ----------------------
 * Attach this script to a trigger-box entity (one with a trigger collider).
 * When something enters the trigger:
 *   - deactivates `inactiveParent`
 *   - activates `activeParent`
 *
 * Optionally, assign `playerRef` to only trigger when the player enters.
 */
class TriggerParentSwitcher : public IScript {
public:
    TriggerParentSwitcher() {
        SCRIPT_GAMEOBJECT_REF(inactiveParent);
        SCRIPT_GAMEOBJECT_REF(activeParent);
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_FIELD(oneShot, Bool);
        SCRIPT_FIELD(startWithActiveParentInPast, Bool);
        SCRIPT_FIELD(toggleSelectionOnTrigger, Bool);
    }

    ~TriggerParentSwitcher() override = default;

    void Awake() override {}
    void Initialize(Entity /*entity*/) override {}

    void Start() override {
        m_selectedActiveParent = startWithActiveParentInPast;
        m_isPast = false; // Game starts in Present in your setup; ChronoActivated will flip this.

        if (!inactiveParent.IsValid() || !activeParent.IsValid()) {
            LOG_WARNING("TriggerParentSwitcher: inactiveParent or activeParent is not assigned");
        }

        RegisterEventListeners();
        ApplySelectionForCurrentState();
    }

    void Update(double /*dt*/) override {}

    void OnTriggerEnter(Entity other) override {
        if (oneShot && m_switched) return;

        if (!inactiveParent.IsValid() || !activeParent.IsValid()) return;

        // If a playerRef is assigned, only react when that entity enters.
        if (playerRef.IsValid() && other != playerRef.GetEntity()) return;

        if (toggleSelectionOnTrigger) {
            m_selectedActiveParent = !m_selectedActiveParent;
        } else {
            // Default behavior: on trigger, make `activeParent` the chosen one.
            m_selectedActiveParent = true;
        }

        ApplySelectionForCurrentState();

        m_switched = true;
    }

    void OnTriggerExit(Entity /*other*/) override {}
    void OnTriggerStay(Entity /*other*/) override {}

    void OnDestroy() override {}
    void OnEnable() override {
        // In case inspector/scene toggles enable this script mid-play.
        ApplySelectionForCurrentState();
    }
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "TriggerParentSwitcher"; }

    void OnCollisionEnter(Entity /*other*/) override {}
    void OnCollisionExit(Entity /*other*/) override {}
    void OnCollisionStay(Entity /*other*/) override {}

private:
    GameObjectRef inactiveParent;
    GameObjectRef activeParent;
    GameObjectRef playerRef;

    bool oneShot = true;
    bool m_switched = false;

    // When `true` -> activate activeParent when in Past
    bool m_selectedActiveParent = false;
    bool m_isPast = false;

    bool startWithActiveParentInPast = false;
    bool toggleSelectionOnTrigger = false;

    bool m_eventsRegistered = false;

    void RegisterEventListeners() {
        if (m_eventsRegistered) return;

        // These events are used elsewhere in your project (see Misc_ICOSwitcher).
        Events::Listen("ChronoActivated", [this](void*) {
            m_isPast = true;
            ApplySelectionForCurrentState();
        });

        Events::Listen("ChronoDeactivated", [this](void*) {
            m_isPast = false;
            ApplySelectionForCurrentState();
        });

        m_eventsRegistered = true;
    }

    void ApplySelectionForCurrentState() {
        if (!inactiveParent.IsValid() || !activeParent.IsValid()) return;

        if (!m_isPast) {
            // Present: Past subtree should be hidden.
            SetActive(false, inactiveParent.GetEntity());
            SetActive(false, activeParent.GetEntity());
            return;
        }

        // Past: show exactly one of the two.
        SetActive(m_selectedActiveParent, activeParent.GetEntity());
        SetActive(!m_selectedActiveParent, inactiveParent.GetEntity());
    }
};

