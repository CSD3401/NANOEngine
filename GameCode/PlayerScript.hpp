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
#include <Core/SpdLogger.hpp>

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

        //LogMessage("PlayerScript created");
		SPD_DEBUG("PlayerScript created");

    }
    
    ~PlayerScript() override {
        //LogMessage("PlayerScript destroyed");
		SPD_DEBUG("PlayerScript destroyed");
    }

    // === IScript Interface ===
    void Awake() override {
        SPD_DEBUG("PlayerScript::Awake() called for entity {}", GetEntity());
        // Initialize things that should happen even if disabled
    }

    void Initialize(NE::ECS::Entity entity) override {
        //LogMessage("PlayerScript initialized for entity " + std::to_string(entity));
		SPD_DEBUG("PlayerScript initialized for entity {}", entity);

        // In a real implementation, you might:
        // - Get references to other components (Transform, Renderer, etc.)
        // - Set up initial state
        // - Subscribe to input events
        // - Initialize physics properties
    }

    void Start() override {
  SPD_DEBUG("PlayerScript::Start() called for entity {}", GetEntity());
 // Called before first Update(), only if enabled
 }

    void OnValidate() override {
  SPD_DEBUG("PlayerScript::OnValidate() - speed={}, lives={}", speed, lives);
        // Validate field values when changed in editor
        if (speed < 0) speed = 0;
if (lives < 0) lives = 0;
    }

    void Update(double deltaTime) override {
        m_timeSinceLastLog += deltaTime;
        
        if (m_timeSinceLastLog >= LOG_INTERVAL) {
            //LogMessage("PlayerScript updating - Entity: " + std::to_string(GetEntity()) + 
            //          ", DeltaTime: " + std::to_string(deltaTime));
			SPD_DEBUG("PlayerScript updating - Entity: {}, DeltaTime: {}", GetEntity(), deltaTime);
            m_timeSinceLastLog = 0.0;
        }

        // Unity-style movement with helper functions
        float moveSpeed = speed * (float)deltaTime;
        
        if(NE::InputManager::IsKeyDown('D'))
            Translate(moveSpeed, 0, 0);
        else if (NE::InputManager::IsKeyDown('A'))
            Translate(-moveSpeed, 0, 0);
        else if (NE::InputManager::IsKeyDown('W'))
            Translate(0, moveSpeed, 0);
        else if (NE::InputManager::IsKeyDown('S'))
            Translate(0, -moveSpeed, 0);
    }

    void OnDestroy() override {
        //LogMessage("PlayerScript cleanup for entity " + std::to_string(GetEntity()));
		SPD_DEBUG("PlayerScript cleanup for entity {}", GetEntity());

        // In a real implementation, you might:
        // - Unsubscribe from events
        // - Clean up resources
        // - Save player state
    }

    void OnEnable() override {
        //LogMessage("PlayerScript enabled for entity " + std::to_string(GetEntity()));
		SPD_DEBUG("PlayerScript enabled for entity {}", GetEntity());
        // Resume player functionality
    }

    void OnDisable() override {
        //LogMessage("PlayerScript disabled for entity " + std::to_string(GetEntity()));
		SPD_DEBUG("PlayerScript disabled for entity {}", GetEntity());
        // Pause player functionality, reset input
    }

    const char* GetTypeName() const override { 
        return "PlayerScript"; 
    }

    // === Event Handlers ===
    void OnCollisionEnter(NE::ECS::Entity other) override {
        //LogMessage("PlayerScript collision enter with entity " + std::to_string(other));
		SPD_DEBUG("PlayerScript collision enter with entity {}", other);
        // Your collision logic goes here
    }

    void OnCollisionExit(NE::ECS::Entity other) override {
        //LogMessage("PlayerScript collision exit with entity " + std::to_string(other));
		SPD_DEBUG("PlayerScript collision exit with entity {}", other);
    }

    void OnTriggerEnter(NE::ECS::Entity other) override {
        LogMessage("PlayerScript trigger enter with entity " + std::to_string(other));
		SPD_DEBUG("PlayerScript trigger enter with entity {}", other);
    }

    void OnTriggerExit(NE::ECS::Entity other) override {
        //LogMessage("PlayerScript trigger exit with entity " + std::to_string(other));
		SPD_DEBUG("PlayerScript trigger exit with entity {}", other);
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