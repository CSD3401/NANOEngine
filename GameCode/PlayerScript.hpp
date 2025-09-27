#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
//#include "ECS/Components/Rigidbody.hpp"
#include "ECS/Components/Transform.hpp"


/**
 * Example player script demonstrating how to implement IScript.
 */
class PlayerScript : public IScript {

public:
    PlayerScript() {
        LogMessage("PlayerScript created");
    }
    
    ~PlayerScript() override {
        LogMessage("PlayerScript destroyed");
    }

    // === IScript Interface ===
    void Initialize(NE::ECS::Entity entity) override {
        LogMessage("PlayerScript initialized for entity " + std::to_string(entity));

        // In a real implementation, you might:
        // - Get references to other components (Transform, Renderer, etc.)
        // - Set up initial state
        // - Subscribe to input events
        // - Initialize physics properties
    }

    void Update(double deltaTime) override {
        m_timeSinceLastLog += deltaTime;
        
        if (m_timeSinceLastLog >= LOG_INTERVAL) {
            LogMessage("PlayerScript updating - Entity: " + std::to_string(GetEntity()) + 
                      ", DeltaTime: " + std::to_string(deltaTime));
            m_timeSinceLastLog = 0.0;
        }

		// Example movement logic (pseudo-code):
		auto transform = GetComponent<NE::ECS::Component::Transform>();
        if (transform) {

			transform->position.x += 0.1f * deltaTime;
        
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

    // Helper methods
    void LogMessage(const std::string& message) const {
        std::cout << "[PlayerScript][Entity " << GetEntity() << "]: " << message << std::endl;
    }
};