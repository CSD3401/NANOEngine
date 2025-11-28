#pragma once
#include "EngineAPI.hpp"
#include <vector>

using namespace NE::Scripting;

/**
 * TimeSwapListener - Manages visibility of past/present child objects
 *
 * FEATURES:
 * - Listens for "TimeSwapToPast" and "TimeSwapToPresent" events
 * - Automatically shows/hides appropriate children based on time period
 * - Uses hardcoded TransformRef fields instead of vectors
 *
 * USAGE:
 * 1. Attach to parent GameObject (e.g., "Chair")
 * 2. Assign past children to past1, past2, past3 fields
 * 3. Assign present children to present1, present2, present3 fields
 * 4. Children will be shown/hidden when TimeSwapManager broadcasts events
 */

#define MISSING_DATA 999

class TimeSwapListener : public IScript {
public:
    TimeSwapListener() {
        // Vectors - COMMENTED OUT (keeping for reference)
         SCRIPT_FIELD_VECTOR(pastChildren, Entity);
         SCRIPT_FIELD_VECTOR(presentChildren, Entity);

        SCRIPT_FIELD(startInPresent, Bool);
        SCRIPT_FIELD(showLog, Bool);
    }

    ~TimeSwapListener() override = default;

    void Awake() override {}

    void Initialize(Entity entity) override {
        // Register to listen for time swap events
        Events::Listen("TimeSwapToPast", [this](void* data) {
            ShowPast();
            });

        Events::Listen("TimeSwapToPresent", [this](void* data) {
            ShowPresent();
            });

        if (showLog) {
            LOG_INFO("TimeSwapListener is now listening for time swap events");
        }
    }

    void Start() override {
        // Set initial visibility based on starting time period
        if (startInPresent) {
            ShowPresent();
        }
        else {
            ShowPast();
        }

        if (showLog) {
            LOG_INFO("TimeSwapListener initialized");
        }
    }

    void Update(double deltaTime) override {
        // Nothing to update per frame
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}

    void OnValidate() override {
    }

    const char* GetTypeName() const override {
        return "TimeSwapListener";
    }

    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    void ShowPast() {
         for (Entity child : pastChildren)
         {
             if (child != 0) {
                 SetActive(true, child);
             }
         }
         for (Entity child : presentChildren)
         {
             if (child != 0) {
                 SetActive(false, child);
             }
         }

        if (showLog) {
            LOG_INFO("Showing PAST children, hiding PRESENT children");
        }
    }

    void ShowPresent() {
        for (Entity child : pastChildren)
        {
            if (child != 0) {
                SetActive(false, child);
            }
        }
        for (Entity child : presentChildren)
        {
            if (child != 0) {
                SetActive(true, child);
            }
        }

        if (showLog) {
            LOG_INFO("Showing PRESENT children, hiding PAST children");
        }
    }

    // === Exposed Fields ===
    // Vectors - COMMENTED OUT (keeping for reference)
    std::vector<Entity> pastChildren;
    std::vector<Entity> presentChildren;
    bool startInPresent = true;
    bool showLog = false;
};