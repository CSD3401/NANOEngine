#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
//#include "ECS/Components/Rigidbody.hpp"
#include "ECS/Components/Transform.hpp"
#include "ExposedFieldRegistry.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <Math/Vec3.hpp>


/**
 * Example player script demonstrating how to implement IScript.
 */
class PlayerScript : public IScript {

public:
    PlayerScript() {
        // register fields with the helper (macro convenience)
        REGISTER_FIELD(speed);
        REGISTER_FIELD(color);
        REGISTER_FIELD(lives);
        REGISTER_FIELD(godMode);
        REGISTER_FIELD(label);

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

		// Example movement logic:
		auto transform = GetComponent<NE::ECS::Component::Transform>();
        if (transform) {

            if(NE::InputManager::IsKeyDown('D'))
			    transform->position.x += 0.2f * deltaTime;
			else if (NE::InputManager::IsKeyDown('A'))
				transform->position.x -= 0.2f * deltaTime;
			else if (NE::InputManager::IsKeyDown('W'))
				transform->position.y += 0.2f * deltaTime;
			else if (NE::InputManager::IsKeyDown('S'))
				transform->position.y -= 0.2f * deltaTime;
        
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

    // === Exposed editable fields via registry === NEED TO CHANGE
    std::vector<std::string> GetExposedFieldNames() const override { return m_fields.GetNames(); }
    std::string GetFieldType(const std::string& name) const override { return m_fields.GetType(name); }
    std::string GetFieldValueAsString(const std::string& name) const override { return m_fields.GetValue(name); }
    bool SetFieldValueFromString(const std::string& name, const std::string& value) override { return m_fields.SetValue(name, value); }

private:
    double m_timeSinceLastLog = 0.0;
    static constexpr double LOG_INTERVAL = 2.0; // Log every 2 seconds

    // Editable fields
    float speed = 5.0f;
    NE::Math::Vec3 color{1.0f, 0.5f, 0.25f};
    int lives = 3;
    bool godMode = false;
    std::string label = "Player";

    // Field registry
    ExposedFieldRegistry m_fields;

    // Helper methods
    void LogMessage(const std::string& message) const {
        std::cout << "[PlayerScript][Entity " << GetEntity() << "]: " << message << std::endl;
    }
};