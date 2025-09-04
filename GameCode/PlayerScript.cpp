// --- FIX ---
// In Visual Studio projects using Precompiled Headers, "pch.h" (or your
// precompiled header file) MUST be the very first include.
#include "pch.h"

// Now, include the header for the class you are implementing.
#include "PlayerScript.hpp"

#include <iostream>

// You can eventually include engine component headers here to manipulate the entity
// #include "TransformComponent.h"
// #include "Entity.h"

void PlayerScript::OnCreate() {
    // This code runs once when the script is first attached to an entity
    // and the game starts.
    std::cout << "PlayerScript created!" << std::endl;
}

void PlayerScript::OnUpdate(double deltaTime) {
    // This code runs every single frame.
    // All your real-time game logic goes here.

    // For example, let's just print a message.
    // In a real game, you would get the TransformComponent from m_Entity
    // and update its position.
    // std::cout << "PlayerScript updating with deltaTime: " << deltaTime << std::endl;
}

void PlayerScript::OnDestroy() {
    // This code runs when the script is being destroyed.
    // Perfect for cleanup.
    std::cout << "PlayerScript destroyed!" << std::endl;
}

