#pragma once
#include "EngineAPI.hpp"
#include <iostream>

/**
 * LayerRefTestScript - Test script to demonstrate LayerRef functionality
 *
 * This script shows how to use LayerRef fields which display a dropdown
 * in the Inspector showing all available layers from the LayerDatabase.
 *
 * HOW TO TEST:
 * 1. Build the project
 * 2. Attach this script to any entity
 * 3. In the Inspector, you should see:
 *    - "targetLayer" field with a dropdown showing available layers
 *    - "raycastLayer" field with another layer dropdown
 *    - "rayDistance" float field
 * 4. Click the dropdown to see all activated layers (e.g., "0: Default", "1: Player", etc.)
 * 5. Select different layers and observe the console output
 *
 * EXPECTED INSPECTOR APPEARANCE:
 * +------------------------------------------+
 * | LayerRefTestScript                       |
 * +------------------------------------------+
 * | [x] Enabled                              |
 * | Entity ID: 42                            |
 * |------------------------------------------|
 * | --- Script Fields ---                    |
 * |                                          |
 * | targetLayer    [v 0: Default        ]   | <-- Dropdown!
 * | raycastLayer   [v 0: Default        ]   | <-- Dropdown!
 * | rayDistance    [       50.0         ]   |
 * | showDebugInfo  [x]                       |
 * +------------------------------------------+
 *
 * When you click the dropdown, it expands to show:
 * +------------------------+
 * | 0: Default             |
 * | 1: Player              |
 * | 2: Enemy               |
 * | 3: Ground              |
 * | ... (other layers)     |
 * +------------------------+
 */

class LayerRefTestScript : public IScript {
public:
    LayerRefTestScript() {
        // Register LayerRef fields - these will show as dropdowns in the Inspector
        SCRIPT_FIELD_LAYERREF(targetLayer);
        SCRIPT_FIELD_LAYERREF(raycastLayer);

        // Register other fields for comparison
        SCRIPT_FIELD(rayDistance, Float);
        SCRIPT_FIELD(showDebugInfo, Bool);
    }

    void Initialize(Entity entity) override {
        std::cout << "[LayerRefTestScript] Initialized!" << std::endl;
        std::cout << "  Target Layer ID: " << (int)targetLayer.GetID() << std::endl;
        std::cout << "  Raycast Layer ID: " << (int)raycastLayer.GetID() << std::endl;
    }

    void Update(double deltaTime) override {
        // Example: Use the layer for raycasting
        if (showDebugInfo) {
            // Print every 2 seconds
            static float timer = 0.0f;
            timer += static_cast<float>(deltaTime);
            if (timer >= 2.0f) {
                timer = 0.0f;

                std::cout << "[LayerRefTestScript] Current settings:" << std::endl;
                std::cout << "  Target Layer ID: " << (int)targetLayer.GetID() << std::endl;
                std::cout << "  Target Layer Mask: " << targetLayer.ToMask() << std::endl;
                std::cout << "  Raycast Layer ID: " << (int)raycastLayer.GetID() << std::endl;
                std::cout << "  Raycast Layer Mask: " << raycastLayer.ToMask() << std::endl;
            }
        }

        // Example: Perform a raycast using the selected layer
        if (Input::WasKeyPressed('L')) {
            uint32_t layerMask = raycastLayer.ToMask();

            std::cout << "[LayerRefTestScript] Raycasting on layer " << (int)raycastLayer.GetID()
                      << " (mask: " << layerMask << ")" << std::endl;

            RaycastHit hit = Raycast(GetPosition(), GetForward(), rayDistance, layerMask);

            if (hit.hasHit) {
                std::cout << "  HIT! Entity: " << hit.entity
                          << " at distance: " << hit.distance << std::endl;
            } else {
                std::cout << "  No hit on this layer." << std::endl;
            }
        }
    }

    const char* GetTypeName() const override {
        return "LayerRefTestScript";
    }

    // Required interface methods
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    // LayerRef fields - these show as DROPDOWNS in the Inspector!
    LayerRef targetLayer;      // Shows dropdown with all available layers
    LayerRef raycastLayer;     // Another layer dropdown

    // Regular fields for comparison
    float rayDistance = 50.0f;
    bool showDebugInfo = true;
};
