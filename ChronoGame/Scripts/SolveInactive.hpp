#pragma once
#include "EngineAPI.hpp"

/**
 * SolveInactive - listens for a puzzle-solved event and deactivates its entity.
 *
 * Usage:
 *  - Attach this script to any object you want to deactivate when a puzzle solves.
 *  - In the inspector, set eventName to match the sender (default: MaterialSequencerSolved).
 */
class SolveInactive : public IScript {
public:
    SolveInactive() {
        SCRIPT_FIELD(isActive, Bool);
        SCRIPT_FIELD(eventName, String);
    }

    void Initialize(Entity entity) override {
        // Subscribe to event once when script is initialized
        Events::Listen(eventName.c_str(), [this](void* data) {
            // Deactivate the entity this script is attached to
            SetActive(false, GetEntity());
        });
        LOG_INFO(std::string("SolveInactive listening to '") + eventName + "'");
    }

    void Update(double) override {
        if (!isActive) return;
    }

    // Unused handlers
    void OnCollisionEnter(Entity) override {}
    void OnCollisionExit(Entity) override {}
    void OnTriggerEnter(Entity) override {}
    void OnTriggerExit(Entity) override {}

private:
    bool isActive = true;
    std::string eventName = "MaterialSequencerSolved";
};
