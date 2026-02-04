#pragma once
#include "EngineAPI.hpp"
/*
* Miscellaneous_ICOSwitcher:
* - Listens for ChronoActivated/ChronoDeactivated events
* - Toggles two referenced GameObjects (idle vs running)
* - Destroys itself if references are invalid
*/

class Misc_ICOSwitcher : public IScript {
public:
    Misc_ICOSwitcher() {
        //SCRIPT_GAMEOBJECT_REF(objectsIdle);
        //SCRIPT_GAMEOBJECT_REF(objectsRunning);
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
        // Addition: Ensure a consistent initial state at play start (running on, idle off).
        
		// RF - Start the game in the present state aka presentObj - active  , pastObj - inactive
        //Activate();
    }
    void Update(double deltaTime) override {}

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
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    //GameObjectRef objectsIdle; // list of past obj
    //GameObjectRef objectsRunning; // list of present obj
    GameObjectRef presentObj; // list of past obj -> put a empty parent to contain all present obj
    GameObjectRef pastObj; // list of past obj -> put a empty parent to contain all present obj
    bool eventsRegistered = false;
    bool listeningEnabled = false;

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
        if (CheckObjectsValid()) {
            LOG_INFO("Miscellaneous_ICOSwitcher: ChronoActivated -> idle off, running on");
            SetActive(false, presentObj.GetEntity());
            SetActive(true, pastObj.GetEntity());
        } else {
            LOG_WARNING("Miscellaneous_ICOSwitcher: Invalid references on activate, destroying");
            Command::DestroyEntity(GetEntity());
        }
    }

    void Deactivate() {
        if (CheckObjectsValid()) {
            LOG_INFO("Miscellaneous_ICOSwitcher: ChronoDeactivated -> idle on, running off");
            SetActive(true, presentObj.GetEntity());
            SetActive(false, pastObj.GetEntity());
        } else {
            LOG_WARNING("Miscellaneous_ICOSwitcher: Invalid references on deactivate, destroying");
            Command::DestroyEntity(GetEntity());
        }
    }
};
