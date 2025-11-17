#pragma once
#include <iostream>
#include "EngineAPI.hpp"

class PlayerCamera : public NE::Scripting::IScript {
public:
    PlayerCamera() {
    }

    void Initialize(NE::Scripting::Entity entity) override {
        SetRotation(0.f, 180.f, 0.f);
    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        auto playerPos = NE::ECS::Query::GetEntityTransform(6).position;
        // Implicit conversion from Math::Vec3 to Scripting::Vec3
        NE::Scripting::Vec3 camPos(playerPos.x,playerPos.y,playerPos.z);
        camPos.y += 0.6f;  // Add camera height offset
        SetPosition(camPos);

        // --- mouse look ---
        auto [mouseX, mouseY] = NE::InputManager::MousePos();

        if (m_firstMouse) {
            m_lastX = (float)mouseX;
            m_lastY = (float)mouseY;
            m_firstMouse = false;
        }

        float xoffset = (float)mouseX - m_lastX;
        float yoffset = (float)mouseY - m_lastY; // invert so moving mouse up looks up
        m_lastX = (float)mouseX;
        m_lastY = (float)mouseY;

        const float sensitivity = 0.1f; // tweak
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        m_yaw += xoffset;
        m_pitch += yoffset;

        // clamp pitch so camera doesn't flip
        if (m_pitch > 89.0f)  m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;

        SetRotation(m_pitch, m_yaw, 0.0f);

        if (NE::InputManager::IsKeyDown('Q')) {
            //auto selectedEntt = NE::GetPickedEntity(960, 540);
            //SPD_WARNING("Selected Entity: " << selectedEntt);
            //if (pickedEntity == NE::ECS::NO_ENTITY) {
            //    if (selectedEntt == 5 || selectedEntt == 6) {
            //        pickedEntity = selectedEntt;
            //    }
            //} else {
            //    pickedEntity = NE::ECS::NO_ENTITY;
            //}
        }

        if (pickedEntity != NE::ECS::NO_ENTITY) {
            auto& enttTransform = NE::ECS::Command::GetEntityTransform(pickedEntity);

            float pitchRad = m_pitch * 0.017453292519943295f;
            float yawRad = m_yaw * 0.017453292519943295f;

            NE::Scripting::Vec3 forward;
            forward.x = cosf(pitchRad) * sinf(yawRad);
            forward.y = sinf(pitchRad);
            forward.z = -cosf(pitchRad) * cosf(yawRad);

            forward.y = 0.0f;
            forward = forward.Normalized();

            const float distance = 0.8f;

            // Calculate new position with implicit Vec3 conversion
            float keepY = enttTransform.position.y;
            auto newPos = camPos + forward * distance;
            newPos.y = keepY;
            enttTransform.position = newPos;  // Implicit conversion from Scripting::Vec3 to Math::Vec3
        }
    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "PlayerCamera";
    }

    // Event handlers (required by interface)
    void OnCollisionEnter(NE::Scripting::Entity other) override {}
    void OnCollisionExit(NE::Scripting::Entity other) override {}
    void OnTriggerEnter(NE::Scripting::Entity other) override {}
    void OnTriggerExit(NE::Scripting::Entity other) override {}

private:
    // === Exposed Fields ===
    // These will automatically appear in the editor inspector
    bool isActive = true;
    //std::string objectName = "TestObject";

    bool switched = false;

    float m_yaw = 0.0f;   // degrees
    float m_pitch = 0.0f;   // degrees
    bool  m_firstMouse = true;
    float m_lastX = 0.0f;
    float m_lastY = 0.0f;

    NE::Scripting::Entity pickedEntity = NE::ECS::NO_ENTITY;
};
