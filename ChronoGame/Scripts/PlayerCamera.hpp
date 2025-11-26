#pragma once
#include <iostream>
#include "EngineAPI.hpp"
#include <ScriptSDK/ScriptTypes.h>

class PlayerCamera : public IScript {
public:
    PlayerCamera() {
    }

    void Initialize(Entity entity) override {
    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        auto [mouseX, mouseY] = Input::GetMousePosition();

        if (m_firstMouse) {
            m_lastX = (float)mouseX;
            m_lastY = (float)mouseY;
            m_firstMouse = false;
        }

        float xoffset = (float)mouseX - m_lastX;
        float yoffset = m_lastY - (float)mouseY;
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
        
        if (Input::WasMousePressed(0)) {
            auto forward = GetForward();
            auto hit = Raycast(GetWorldPosition(), forward, 5.f);
            LOG_DEBUG("Position: " << GetPosition().x << " : " << GetPosition().y << " : " << GetPosition().z);
            LOG_DEBUG("Forward: " << forward.x << " : " << forward.y << " : " << forward.z);
            
            LOG_DEBUG("Entity Hit: " << hit.entity);
            std::pair<uint32_t, uint32_t> data = { hit.entity, GetEntity() };

            if (hit.entity != NE::Scripting::INVALID_ENTITY) {
                Events::Send("OnCameraRaycastHit", &data);
            }
        }
    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "PlayerCamera";
    }

    // Event handlers (required by interface)
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    bool isActive = true;

    bool switched = false;

    float m_yaw = 0.0f;   // degrees
    float m_pitch = 0.0f;   // degrees
    bool  m_firstMouse = true;
    float m_lastX = 0.0f;
    float m_lastY = 0.0f;
};
