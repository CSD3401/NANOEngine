#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
#include "ECS/Components/Transform.hpp"
#include "Events/EventBus.hpp"
#include <Math/Vec3.hpp>

/**
 * Test script to demonstrate the new built-in field system.
 * This script shows how easy it is to expose fields to the editor.
 */

void PlayerTestEvent(void* data) {
    int tmp = *reinterpret_cast<int*>(data);
   // SPD_CRITICAL("HIIII Player {}", tmp);
    std::cout << tmp << std::endl;
}


class TestScript : public IScript {
public:
    TestScript() {
        // Register all our fields using the simple macros
        SCRIPT_FIELD(rotationSpeed, Float);
        SCRIPT_FIELD(bounceHeight, Float);
        SCRIPT_FIELD(color, Vec3);
        SCRIPT_FIELD(particleCount, Int);
        SCRIPT_FIELD(isActive, Bool);
        SCRIPT_FIELD(objectName, String);

        std::cout << "[TestScript] Created with fields registered" << std::endl;
    }

    void Initialize(NE::ECS::Entity entity) override {
        //std::cout << "[TestScript] Initialized for entity " << entity << std::endl;
        //std::cout << "[TestScript] Initial values:" << std::endl;
        //std::cout << "  - Rotation Speed: " << rotationSpeed << std::endl;
        //std::cout << "  - Bounce Height: " << bounceHeight << std::endl;
        //std::cout << "  - Color: (" << color.x << ", " << color.y << ", " << color.z << ")" << std::endl;
        //std::cout << "  - Particle Count: " << particleCount << std::endl;
        //std::cout << "  - Is Active: " << (isActive ? "true" : "false") << std::endl;
        //std::cout << "  - Object Name: " << objectName << std::endl;

        NANOEngine::Events::RegisterScriptEventListener("OnPlayerHit", PlayerTestEvent);
    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        // Use the exposed fields in our logic
        auto transform = GetComponent<NE::ECS::Component::Transform>();
        if (transform) {
            // Rotate using the rotation speed field
            static float totalTime = 0.0f;
            totalTime += static_cast<float>(deltaTime);
            
            // Simple bobbing motion using bounce height
            transform->localPosition.y = std::sin(totalTime * 2.0f) * bounceHeight;
            
            // You could also use the color field to set renderer color
            // You could use particleCount to control particle systems
            // etc.
        }
    }

    void OnDestroy() override {
        //std::cout << "[TestScript] Destroyed for entity " << GetEntity() << std::endl;
    }

    const char* GetTypeName() const override {
        return "TestScript";
    }

    // Event handlers (required by interface)
    void OnCollisionEnter(NE::ECS::Entity other) override {}
    void OnCollisionExit(NE::ECS::Entity other) override {}
    void OnTriggerEnter(NE::ECS::Entity other) override {}
    void OnTriggerExit(NE::ECS::Entity other) override {}

private:
    // === Exposed Fields ===
    // These will automatically appear in the editor inspector
    float rotationSpeed = 90.0f;  // degrees per second
    float bounceHeight = 1.0f;    // units
    NE::Math::Vec3 color{1.0f, 0.0f, 0.0f};  // red by default
    int particleCount = 50;
    bool isActive = true;
    std::string objectName = "TestObject";
};
