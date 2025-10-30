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
#include <Core/SpdLogger.hpp>

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

        //LogMessage("PlayerScript created");
		SPD_DEBUG("PlayerScript created");

    }
    
    ~PlayerScript() override {
        //LogMessage("PlayerScript destroyed");
		SPD_DEBUG("PlayerScript destroyed");
    }

    // === IScript Interface ===
    void Initialize(NE::ECS::Entity entity) override {
        //LogMessage("PlayerScript initialized for entity " + std::to_string(entity));
		SPD_DEBUG("PlayerScript initialized for entity {}", entity);

        // In a real implementation, you might:
        // - Get references to other components (Transform, Renderer, etc.)
        // - Set up initial state
        // - Subscribe to input events
        // - Initialize physics properties
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