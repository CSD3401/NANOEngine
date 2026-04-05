#pragma once
#include "EngineAPI.hpp"
#include <ScriptSDK/ScriptAPI.h>
/*
* Miscellaneous_ICOSwitcher:
* - Listens for ChronoActivated/ChronoDeactivated events
* - Toggles two referenced GameObjects (present vs past)
* - Destroys itself if references are invalid
*
* NOTE: Both Activate() and Deactivate() defer coroutine creation
* to next frame via Update() to avoid re-entrancy crashes in the
* coroutine system (Coroutines::Create() cannot be called from
* inside a coroutine action callback).
*/

class Misc_ICOSwitcher : public IScript {
public:
    Misc_ICOSwitcher() {
        SCRIPT_GAMEOBJECT_REF(presentObj);
        SCRIPT_GAMEOBJECT_REF(pastObj);
    }

    ~Misc_ICOSwitcher() override = default;

    // === Lifecycle Methods ===
    void Awake() override {
        RegisterEventListeners();
        LOG_INFO("Miscellaneous_ICOSwitcher: listeners registered");
    }

    void Initialize(Entity entity) override {}

    void Start() override {
        // Irwen - mouse visibility here for now
        NE::Scripting::SetMouseVisible(false);

        if (!CheckObjectsValid()) return;

        // Start in present state
        //SetActive(true, presentObj.GetEntity());
        //SetActive(false, pastObj.GetEntity());

        // Delay so everyone can register listeners first
        Coroutines::Handle h = Coroutines::Create();
        Coroutines::AddWait(h, 0.0f);
        Coroutines::AddAction(h, []() { Events::Send("TimePastDisabled", nullptr); });
        Coroutines::Start(h);
    }

    void Update(double deltaTime) override {
        if (m_pendingPastEnabled) {
            m_pendingPastEnabled = false;
            Coroutines::Handle h = Coroutines::Create();
            Coroutines::AddWait(h, 0.0f);
            Coroutines::AddAction(h, []() { Events::Send("TimePastEnabled", nullptr); });
            Coroutines::Start(h);
        }
        if (m_pendingPastDisabled) {
            m_pendingPastDisabled = false;
            Coroutines::Handle h = Coroutines::Create();
            Coroutines::AddWait(h, 0.0f);
            Coroutines::AddAction(h, []() { Events::Send("TimePastDisabled", nullptr); });
            Coroutines::Start(h);
        }
    }

    void OnDestroy() override {
        listeningEnabled = false;
    }

    // === Optional Callbacks ===
    void OnEnable() override {
        listeningEnabled = true;
        LOG_INFO("Miscellaneous_ICOSwitcher: enabled");
    }

    void OnDisable() override {
        listeningEnabled = false;
        LOG_INFO("Miscellaneous_ICOSwitcher: disabled");
    }

    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Miscellaneous_ICOSwitcher";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef presentObj;
    GameObjectRef pastObj;
    bool eventsRegistered = false;
    bool listeningEnabled = false;
    bool m_pendingPastEnabled = false;
    bool m_pendingPastDisabled = false;

    void RegisterEventListeners() {
        if (eventsRegistered) {
            return;
        }

        Events::Listen("ChronoActivated", [this](void*) {
            if (!listeningEnabled) {
                LOG_INFO("Miscellaneous_ICOSwitcher: ChronoActivated ignored (disabled)");
                return;
            }
            Activate();
            });

        Events::Listen("ChronoDeactivated", [this](void*) {
            if (!listeningEnabled) {
                LOG_INFO("Miscellaneous_ICOSwitcher: ChronoDeactivated ignored (disabled)");
                return;
            }
            Deactivate();
            });

        eventsRegistered = true;
    }

    bool CheckObjectsValid() const {
        return presentObj.IsValid() && pastObj.IsValid();
    }

    void Activate() {
        if (!CheckObjectsValid()) {
            LOG_WARNING("Miscellaneous_ICOSwitcher: Invalid references on activate, destroying");
            Command::DestroyEntity(GetEntity());
            return;
        }

        LOG_INFO("Miscellaneous_ICOSwitcher: ChronoActivated -> present off, past on");
        SetActive(false, presentObj.GetEntity());
        SetActive(true, pastObj.GetEntity());

        m_pendingPastEnabled = true;
    }

    void Deactivate() {
        if (!CheckObjectsValid()) {
            LOG_WARNING("Miscellaneous_ICOSwitcher: Invalid references on deactivate, destroying");
            Command::DestroyEntity(GetEntity());
            return;
        }

        LOG_INFO("Miscellaneous_ICOSwitcher: ChronoDeactivated -> present on, past off");
        SetActive(true, presentObj.GetEntity());
        SetActive(false, pastObj.GetEntity());

        m_pendingPastDisabled = true;
    }
};