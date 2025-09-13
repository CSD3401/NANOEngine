#pragma once

#include "Scripting/IScript.hpp"
#include <iostream>

/**
 * Example player script demonstrating how to implement IScript.
 * This would be part of your Game DLL project.
 */
class PlayerScript : public IScript {

public:
    PlayerScript();
    ~PlayerScript() override;

    // === IScript Interface ===
    void Initialize(NE::ECS::Entity entity) override;
    void Update(double deltaTime) override;
    void OnDestroy() override;
    void OnEnable() override;
    void OnDisable() override;
    const char* GetTypeName() const override { return "PlayerScript"; }

    // === Event Handlers ===
    void OnCollisionEnter(NE::ECS::Entity other) override;
    void OnCollisionExit(NE::ECS::Entity other) override;
    void OnTriggerEnter(NE::ECS::Entity other) override;
    void OnTriggerExit(NE::ECS::Entity other) override;

private:

    // Helper methods
    void LogMessage(const std::string& message) const;
};

// === PlayerScript.cpp Implementation ===

PlayerScript::PlayerScript(){

    LogMessage("PlayerScript created");
}

PlayerScript::~PlayerScript() {
    LogMessage("PlayerScript destroyed");
}

void PlayerScript::Initialize(NE::ECS::Entity entity) {
    LogMessage("PlayerScript initialized for entity ");

    // In a real implementation, you might:
    // - Get references to other components (Transform, Renderer, etc.)
    // - Set up initial state
    // - Subscribe to input events
    // - Initialize physics properties
}

void PlayerScript::Update(double deltaTime) {
  


    LogMessage("Updating Player");
    
}

void PlayerScript::OnDestroy() {
    LogMessage("PlayerScript cleanup");

    // In a real implementation, you might:
    // - Unsubscribe from events
    // - Clean up resources
    // - Save player state
}

void PlayerScript::OnEnable() {
    LogMessage("PlayerScript enabled");
    // Resume player functionality
}

void PlayerScript::OnDisable() {
    LogMessage("PlayerScript disabled");
    // Pause player functionality, reset input
}

void PlayerScript::OnCollisionEnter(NE::ECS::Entity other) {
    LogMessage("OnCollisionEnter triggered.");
    // Your collision logic goes here
}

void PlayerScript::OnCollisionExit(NE::ECS::Entity other) {
    LogMessage("OnCollisionExit triggered.");
}

void PlayerScript::OnTriggerEnter(NE::ECS::Entity other) {
    LogMessage("OnTriggerEnter triggered.");
}

void PlayerScript::OnTriggerExit(NE::ECS::Entity other) {
    LogMessage("OnTriggerExit triggered.");
}

void PlayerScript::LogMessage(const std::string& message) const {
    std::cout << "[PlayerScript][Entity]: " << message << std::endl;
}