#pragma once
#include "EngineAPI.hpp"

using namespace NE::Scripting;

/**
 * TimeSwapManager - Toggles between past and present time periods
 *
 * FEATURES:
 * - Press 'P' key to swap between past and present
 * - Broadcasts "TimeSwapToPast" or "TimeSwapToPresent" events
 * - Other scripts listen for these events to show/hide appropriate objects
 *
 * USAGE:
 * 1. Attach to a manager GameObject (one per scene)
 * 2. TimeSwapListener scripts will automatically respond to the swap events
 */
class TimeSwapManager : public IScript {
public:
    TimeSwapManager() {
        // Constructor empty - fields registered in Initialize
        SCRIPT_FIELD(startInPresent, Bool);
        SCRIPT_FIELD(showLog, Bool);
    }

    ~TimeSwapManager() override = default;

    void Awake() override {}

    void Initialize(Entity entity) override {

    }

    void Start() override {
        LOG_INFO("TimeSwapManager: Testing event system...");
        //Events::Send("TestEvent");  // Send a simple test event

        isInPresent = startInPresent;

        if (showLog) {
            LOG_INFO("TimeSwapManager initialized. Starting in: "
                << (isInPresent ? "PRESENT" : "PAST"));
        }

        // Send initial state to all listeners
        //BroadcastCurrentState();
    }

    void Update(double deltaTime) override {
        // Check if P key was pressed this frame
        if (Input::WasKeyPressed('P')) {
            ToggleTime();
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "TimeSwapManager";
    }

    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    void ToggleTime() {
        // Toggle the state
        isInPresent = !isInPresent;

        if (showLog) {
            LOG_INFO("Time swapped to: " << (isInPresent ? "PRESENT" : "PAST"));
        }

        // Broadcast the new state
        BroadcastCurrentState();
    }

    void BroadcastCurrentState() {
        if (isInPresent) {
            Events::Send("TimeSwapToPresent");
        }
        else {
            Events::Send("TimeSwapToPast");
        }
    }

    // === Exposed Fields (registered in Initialize) ===
    bool startInPresent = true;  // Which time period to start in
    bool showLog = false;        // Enable debug logging

    // === Internal State (not exposed) ===
    bool isInPresent = true;     // Current time period state
};