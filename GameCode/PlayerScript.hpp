#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
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
    // Example enum for testing
    enum class PlayerState {
        Idle = 0,
        Walking = 1,
        Running = 2,
        Jumping = 3
    };

    PlayerScript() {
        // Register primitive fields
        REGISTER_FIELD(speed);
        REGISTER_FIELD(color);
        REGISTER_FIELD(lives);
      REGISTER_FIELD(godMode);
     REGISTER_FIELD(label);

     // Register enum field with options
        m_fields.RegisterEnum("state",
       [this]() { return state; },
    [this](int val) { state = static_cast<PlayerState>(val); },
          {"Idle", "Walking", "Running", "Jumping"}
        );

        SPD_DEBUG("PlayerScript created");
    }
    
    ~PlayerScript() override {
        SPD_DEBUG("PlayerScript destroyed");
    }

    // === IScript Interface ===
 void Awake() override {
        SPD_DEBUG("PlayerScript::Awake() called for entity {}", GetEntity());
    }

    void Initialize(NE::ECS::Entity entity) override {
        SPD_DEBUG("PlayerScript initialized for entity {}", entity);
    }

    void Start() override {
        SPD_DEBUG("PlayerScript::Start() called for entity {}", GetEntity());
    }

    void OnValidate() override {
 SPD_DEBUG("PlayerScript::OnValidate() - speed={}, lives={}, state={}", 
 speed, lives, static_cast<int>(state));
     
      // Validate field values when changed in editor
   if (speed < 0) speed = 0;
        if (lives < 0) lives = 0;
    }

    void Update(double deltaTime) override {
        m_timeSinceLastLog += deltaTime;
        
        if (m_timeSinceLastLog >= LOG_INTERVAL) {
            SPD_DEBUG("PlayerScript updating - Entity: {}, DeltaTime: {}", GetEntity(), deltaTime);
   SPD_DEBUG("  State: {}", static_cast<int>(state));
      m_timeSinceLastLog = 0.0;
   }

  // Unity-style movement with helper functions
      float moveSpeed = speed * (float)deltaTime;
        
        // Update state based on input
        if(NE::InputManager::IsKeyDown('D')) {
      Translate(moveSpeed, 0, 0);
         state = PlayerState::Walking;
        }
        else if (NE::InputManager::IsKeyDown('A')) {
  Translate(-moveSpeed, 0, 0);
      state = PlayerState::Walking;
        }
        else if (NE::InputManager::IsKeyDown('W')) {
            Translate(0, moveSpeed, 0);
         state = PlayerState::Running;
  }
      else if (NE::InputManager::IsKeyDown('S')) {
    Translate(0, -moveSpeed, 0);
state = PlayerState::Walking;
   }
    else {
      state = PlayerState::Idle;
        }
    }

    void OnDestroy() override {
        SPD_DEBUG("PlayerScript cleanup for entity {}", GetEntity());
    }

    void OnEnable() override {
        SPD_DEBUG("PlayerScript enabled for entity {}", GetEntity());
    }

    void OnDisable() override {
        SPD_DEBUG("PlayerScript disabled for entity {}", GetEntity());
    }

    const char* GetTypeName() const override { 
        return "PlayerScript"; 
    }

    // === Event Handlers ===
    void OnCollisionEnter(NE::ECS::Entity other) override {
   SPD_DEBUG("PlayerScript collision enter with entity {}", other);
  // Your collision logic goes here
    }

    void OnCollisionExit(NE::ECS::Entity other) override {
 SPD_DEBUG("PlayerScript collision exit with entity {}", other);
    }

    void OnTriggerEnter(NE::ECS::Entity other) override {
        SPD_DEBUG("PlayerScript trigger enter with entity {}", other);
    }

    void OnTriggerExit(NE::ECS::Entity other) override {
SPD_DEBUG("PlayerScript trigger exit with entity {}", other);
    }

    // === Exposed editable fields via registry ===
    std::vector<std::string> GetExposedFieldNames() const override { return m_fields.GetNames(); }
  std::string GetFieldType(const std::string& name) const override { return m_fields.GetType(name); }
    std::string GetFieldValueAsString(const std::string& name) const override { return m_fields.GetValue(name); }
    bool SetFieldValueFromString(const std::string& name, const std::string& value) override { return m_fields.SetValue(name, value); }

    // Enum support
    std::vector<std::string> GetEnumOptions(const std::string& fieldName) const override {
   return m_fields.GetEnumOptions(fieldName);
    }

private:
    double m_timeSinceLastLog = 0.0;
    static constexpr double LOG_INTERVAL = 2.0;

    // Editable fields
    float speed = 5.0f;
    NE::Math::Vec3 color{1.0f, 0.5f, 0.25f};
    int lives = 3;
    bool godMode = false;
    std::string label = "Player";
    PlayerState state = PlayerState::Idle; // Enum field

    // Field registry
    ExposedFieldRegistry m_fields;

    void LogMessage(const std::string& message) const {
  std::cout << "[PlayerScript][Entity " << GetEntity() << "]: " << message << std::endl;
    }
};