#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
//#include "ECS/Components/Rigidbody.hpp"
#include "ECS/Components/Transform.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <Math/Vec3.hpp>

/**
 * Example player script demonstrating how to implement IScript.
 * Now uses the built-in field system from IScript base class.
 */
class PlayerScript : public IScript {

public:
    PlayerScript() {
        // Register fields using the new simplified system
        SCRIPT_FIELD(speed, Float);
        SCRIPT_FIELD(color, Vec3);
        SCRIPT_FIELD(lives, Int);
        SCRIPT_FIELD(godMode, Bool);
        SCRIPT_FIELD(label, String);

        LogMessage("PlayerScript created");
    }
    
    ~PlayerScript() override {
        LogMessage("PlayerScript destroyed");
    }

    // === IScript Interface ===
    void Initialize(NE::ECS::Entity entity) override {
        LogMessage("PlayerScript initialized for entity " + std::to_string(entity));
    }

    void Update(double deltaTime) override {
        m_timeSinceLastLog += deltaTime;
        
        if (m_timeSinceLastLog >= LOG_INTERVAL) {
            LogMessage("PlayerScript updating - Entity: " + std::to_string(GetEntity()) + 
                      ", DeltaTime: " + std::to_string(deltaTime) +
                      ", Speed: " + std::to_string(speed) +
                      ", Lives: " + std::to_string(lives) +
                      ", GodMode: " + (godMode ? "true" : "false"));
            m_timeSinceLastLog = 0.0;
        }

        // Example movement logic using the speed field:
        auto transform = GetComponent<NE::ECS::Component::Transform>();
        if (transform) {
            // Use the speed field directly - this will now be properly synchronized with the editor
            float deltaSpeed = speed * static_cast<float>(deltaTime);

            if(NE::InputManager::IsKeyDown('D'))
                transform->position.x += deltaSpeed;
            else if (NE::InputManager::IsKeyDown('A'))
                transform->position.x -= deltaSpeed;
            else if (NE::InputManager::IsKeyDown('W'))
                transform->position.y += deltaSpeed;
            else if (NE::InputManager::IsKeyDown('S'))
                transform->position.y -= deltaSpeed;
        }
    }

    void OnDestroy() override {
        LogMessage("PlayerScript cleanup for entity " + std::to_string(GetEntity()));

        // In a real implementation, you might:
        // - Unsubscribe from events
        // - Clean up resources
        // - Save player state
    }

    void OnEnable() override {
        LogMessage("PlayerScript enabled for entity " + std::to_string(GetEntity()));
        // Resume player functionality
    }

    void OnDisable() override {
        LogMessage("PlayerScript disabled for entity " + std::to_string(GetEntity()));
        // Pause player functionality, reset input
    }

    const char* GetTypeName() const override { 
        return "PlayerScript"; 
    }

    // === Event Handlers ===
    void OnCollisionEnter(NE::ECS::Entity other) override {
        LogMessage("PlayerScript collision enter with entity " + std::to_string(other));
        // Your collision logic goes here
    }

    void OnCollisionExit(NE::ECS::Entity other) override {
        LogMessage("PlayerScript collision exit with entity " + std::to_string(other));
    }

    void OnTriggerEnter(NE::ECS::Entity other) override {
        LogMessage("PlayerScript trigger enter with entity " + std::to_string(other));
    }

    void OnTriggerExit(NE::ECS::Entity other) override {
        LogMessage("PlayerScript trigger exit with entity " + std::to_string(other));
    }

private:
    double m_timeSinceLastLog = 0.0;
    static constexpr double LOG_INTERVAL = 2.0; // Log every 2 seconds

    // === Editable fields ===
    // These fields will be automatically exposed to the editor
    // using the new built-in field system
    float speed = 5.0f;
    NE::Math::Vec3 color{1.0f, 0.5f, 0.25f};
    int lives = 3;
    bool godMode = false;
    std::string label = "Player";

    // Helper methods
    void LogMessage(const std::string& message) const {
        std::cout << "[PlayerScript][Entity " << GetEntity() << "]: " << message << std::endl;
    }
};