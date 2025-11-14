#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
#include "ECS/Components/Transform.hpp"
#include "Events/EventBus.hpp"
#include "Core/Couroutine.hpp"
#include <Math/Vec3.hpp>
#include <Input/InputManager.hpp>
#include <ECS/Components/Light.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <Engine.hpp>


class PlayerCamera : public IScript {
public:
    PlayerCamera() {
    }

    void Initialize(NE::ECS::Entity entity) override {
        SetRotation(0.f, 180.f, 0.f);
    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        auto playerPos = NE::ECS::Query::GetEntityTransform(6).position;
        NE::Math::Vec3 camPos = playerPos + NE::Math::Vec3(0.f, 0.6f, 0.f);
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

        SetRotation(NE::Math::Vec3(m_pitch, m_yaw, 0.0f));

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

            NE::Math::Vec3 forward;
            forward.x = cosf(pitchRad) * sinf(yawRad);
            forward.y = sinf(pitchRad);
            forward.z = -cosf(pitchRad) * cosf(yawRad);

            forward.y = 0.0f;
            forward = forward.Normalized();

            const float distance = 0.8f;

            //enttTransform.position = camPos + forward * distance;
            float keepY = enttTransform.position.y;
            enttTransform.position = camPos + forward * distance;
            enttTransform.position.y = keepY;
        }
    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "PlayerCamera";
    }

    // Event handlers (required by interface)
    void OnCollisionEnter(NE::ECS::Entity other) override {}
    void OnCollisionExit(NE::ECS::Entity other) override {}
    void OnTriggerEnter(NE::ECS::Entity other) override {}
    void OnTriggerExit(NE::ECS::Entity other) override {}

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

    NE::ECS::Entity pickedEntity = NE::ECS::NO_ENTITY;
};
