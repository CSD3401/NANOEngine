#pragma once
#include "EngineAPI.hpp"
#include <vector>
#include <ScriptSDK/ScriptAPI.h>

using namespace NE::Scripting;

/**
 * TimeSwapListener - Manages visibility of past/present child objects
 *
 * FEATURES:
 * - Listens for "TimeSwapToPast" and "TimeSwapToPresent" events
 * - Automatically shows/hides appropriate children based on time period
 * - Supports multiple past and present children per object
 *
 * USAGE:
 * 1. Attach to parent GameObject (e.g., "Chair")
 * 2. Add past child GameObjects to "pastChildren" vector (e.g., "Past Chair")
 * 3. Add present child GameObjects to "presentChildren" vector (e.g., "Present Chair")
 * 4. Children will be shown/hidden when TimeSwapManager broadcasts events
 *
 * EXAMPLE HIERARCHY:
 * Chair (has TimeSwapListener script)
 *   - Past Chair (added to pastChildren vector)
 *   -Past Chair Cushion (added to pastChildren vector)
 *   - Present Chair (added to presentChildren vector)
 *   - Present Chair Cushion (added to presentChildren vector)
 */
class TimeSwapListener : public IScript {
public:
    TimeSwapListener() {
        // Constructor empty - fields registered in Initialize
    }

    ~TimeSwapListener() override = default;

    void Awake() override {
        // Register to listen for time swap events
        RegisterScriptEventListener("TimeSwapToPast", [this](void* data) {
            ShowPast();
            });

        RegisterScriptEventListener("TimeSwapToPresent", [this](void* data) {
            ShowPresent();
            });

        if (showLog) {
            LOG_INFO("TimeSwapListener is now listening for time swap events");
        }
    }

    void Initialize(Entity entity) override {
        // Register vector fields for past and present children
        SCRIPT_FIELD_VECTOR(pastChildren, Entity);
        SCRIPT_FIELD_VECTOR(presentChildren, Entity);

        SCRIPT_FIELD(startInPresent, Bool);
        SCRIPT_FIELD(showLog, Bool);
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
            LOG_INFO("TimeSwapListener initialized with "
                << pastChildren.size() << " past children and "
                << presentChildren.size() << " present children");
        }
    }

    void Update(double deltaTime) override {
        // Nothing to update per frame
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}

    void OnValidate() override {
        // Validate that we have children in at least one vector
        if (pastChildren.empty() && presentChildren.empty()) {
            LOG_WARNING("TimeSwapListener has no children assigned!");
        }
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
        // Show all past children
        for (Entity child : pastChildren) 
        {
			SetActive(true, child);
        }

        // Hide all present children
        for (Entity child : presentChildren) 
        {
            SetActive(false, child);

        }

        if (showLog) {
            LOG_INFO("Showing PAST children, hiding PRESENT children");
        }
    }

    void ShowPresent() {
        // Hide all past children
        for (Entity child : pastChildren) 
        {
            SetActive(false, child);
        }

        // Show all present children
        for (Entity child : presentChildren) 
        {
            SetActive(true, child);
        }

        if (showLog) {
            LOG_INFO("Showing PRESENT children, hiding PAST children");
        }
    }

    // === Exposed Fields (registered in Initialize) ===
    std::vector<Entity> pastChildren;       // List of child GameObjects for past era
    std::vector<Entity> presentChildren;    // List of child GameObjects for present era
    bool startInPresent = true;             // Which time period to start in
    bool showLog = false;                 // Enable debug logging
};