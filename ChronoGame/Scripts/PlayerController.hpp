#pragma once
#include "EngineAPI.hpp"
#include <cmath>
#include <algorithm>
#include <ScriptSDK/Math.h>

#define GLFW_KEY_SPACE 32

class PlayerController : public IScript {
public:
    PlayerController() = default;
    ~PlayerController() override = default;

    void Awake() override {}

    void Initialize(Entity entity) override {
        SCRIPT_COMPONENT_REF(playerBottom, TransformRef);

        SCRIPT_FIELD(moveSpeed, Float);
        SCRIPT_FIELD(jumpForce, Float);
        SCRIPT_FIELD(maxSlopeAngle, Float);
        SCRIPT_FIELD(groundRaycastDistance, Float);
        SCRIPT_FIELD(groundProbeStartOffset, Float);
        SCRIPT_FIELD(groundSnapOffset, Float);
        SCRIPT_FIELD(skinWidth, Float);

        SCRIPT_FIELD(airControl, Float);
        SCRIPT_FIELD(groundFriction, Float);
        SCRIPT_FIELD(maxFallSpeed, Float);
    }

    void Start() override {
        if (Command::HasComponent<Component::Collider>(GetEntity())) {
            auto& col = Command::GetComponent<Component::Collider>(GetEntity());
            m_colliderHalfHeight = col.halfExtents.y;
            LOG_INFO("PlayerController: collider half-height = " << m_colliderHalfHeight);
        } else {
            LOG_WARNING("PlayerController: no Collider found on entity "
                << GetEntity() << " – ground checks may be inaccurate.");
        }

        if (!HasRigidbody()) {
            LOG_ERROR("PlayerController: Rigidbody is required but not found on entity "
                << GetEntity());
        }

        m_isGrounded = false;
        m_hasJumpedThisFrame = false;
    }

    void Update(double deltaTime) override {
        if (!HasRigidbody())
            return;

        LockRotation(true, false, true);
        const float dt = static_cast<float>(deltaTime);

        UpdateGroundedState();
        UpdateHorizontalVelocity(dt);
        HandleJump();
        ClampFallSpeed();

        // tiny vertical snap to ground while grounded
        if (m_isGrounded && m_lastGroundHit.hasHit) {
            ApplyGroundSnap();
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}

    const char* GetTypeName() const override {
        return "PlayerController";
    }

    // === Event Handlers (Required by IScript) ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    void UpdateGroundedState() {
        m_isGrounded = false;
        m_lastGroundHit = {};

        Entity bottomEntity = playerBottom.IsValid() ? playerBottom.GetEntity() : GetEntity();
        Vec3 feetPos = GetWorldPosition(bottomEntity);

        Vec3 origin = feetPos;
        origin.y += groundProbeStartOffset;
        Vec3 downDir{ 0.0f, -1.0f, 0.0f };

        uint32_t layerMask = 0xFFFFFFFF;
        float rayLen = groundRaycastDistance + groundProbeStartOffset + skinWidth;

        RaycastHit hit = Raycast(origin, downDir, rayLen, layerMask);

        if (hit.hasHit) {
            m_isGrounded = true;
            m_lastGroundHit = hit;

            // slope limit
            //float upDot = hit.normal.Dot(Vec3{ 0.0f, 1.0f, 0.0f });
            //upDot = std::clamp(upDot, -1.0f, 1.0f);
            //float slopeDeg = std::acos(upDot) * (180.0f / 3.14159265f);

            //if (slopeDeg > maxSlopeAngle) {
            //    // Too steep -> treat as not grounded
            //    m_isGrounded = false;
            //}
        }
    }

    void UpdateHorizontalVelocity(float dt) {
        if (!HasRigidbody()) return;

        Vec3 inputDir{ 0.0f, 0.0f, 0.0f };

        if (Input::IsKeyDown('W')) {
            inputDir.z -= 1.0f;
        }
        if (Input::IsKeyDown('S')) {
            inputDir.z += 1.0f;
        }
        if (Input::IsKeyDown('A')) {
            inputDir.x -= 1.0f;
        }
        if (Input::IsKeyDown('D')) {
            inputDir.x += 1.0f;
        }

        float mag = std::sqrt(inputDir.x * inputDir.x + inputDir.z * inputDir.z);
        if (mag > 0.01f) {
            inputDir.x /= mag;
            inputDir.z /= mag;
        } else {
            inputDir.x = 0.0f;
            inputDir.z = 0.0f;
        }

        Vec3 vel = GetVelocity();

        Vec3 horizVel = vel;
        horizVel.y = 0.0f;

        if (mag > 0.0f) {
            Vec3 targetVel{
                inputDir.x * moveSpeed,
                0.0f,
                inputDir.z * moveSpeed
            };

            float control = m_isGrounded ? 1.0f : airControl;
            control = std::clamp(control, 0.0f, 1.0f);

            // Snappiness factor
            float t = control * dt * 10.0f;
            t = std::clamp(t, 0.0f, 1.0f);

            horizVel.x = Lerp(horizVel.x, targetVel.x, t);
            horizVel.z = Lerp(horizVel.z, targetVel.z, t);
        } else if (m_isGrounded) {
            // Simple custom friction when grounded and no input
            float speed = std::sqrt(horizVel.x * horizVel.x + horizVel.z * horizVel.z);
            if (speed > 0.01f) {
                float friction = groundFriction * dt;
                float newSpeed = std::max(0.0f, speed - friction);
                float factor = (speed > 0.0f) ? (newSpeed / speed) : 0.0f;

                horizVel.x *= factor;
                horizVel.z *= factor;
            } else {
                horizVel.x = 0.0f;
                horizVel.z = 0.0f;
            }
        }

        vel.x = horizVel.x;
        vel.z = horizVel.z;

        SetVelocity(vel);
    }

    void HandleJump() {
        if (!HasRigidbody()) return;

        if (Input::WasKeyPressed(GLFW_KEY_SPACE)) {
            if (m_isGrounded && !m_hasJumpedThisFrame) {
                Vec3 vel = GetVelocity();
                vel.y = jumpForce;
                SetVelocity(vel);

                m_hasJumpedThisFrame = true;
                m_isGrounded = false;
            }
        }

        if (!Input::IsKeyDown(GLFW_KEY_SPACE)) {
            m_hasJumpedThisFrame = false;
        }
    }

    void ClampFallSpeed() {
        if (!HasRigidbody()) return;

        Vec3 vel = GetVelocity();
        if (vel.y < maxFallSpeed) {
            vel.y = maxFallSpeed;
            SetVelocity(vel);
        }
    }

    void ApplyGroundSnap() {
        if (!m_lastGroundHit.hasHit) return;

        Entity bottomEntity = playerBottom.IsValid() ? playerBottom.GetEntity() : GetEntity();
        Vec3 feetPos = GetPosition(bottomEntity);
        Vec3 origin = feetPos;
        origin.y += groundProbeStartOffset;

        float currentDist = m_lastGroundHit.distance;

        float desiredDist = groundProbeStartOffset + groundSnapOffset;

        float delta = currentDist - desiredDist;
        if (std::fabs(delta) < 0.0001f) return;

        Vec3 pos = GetPosition();
        pos.y -= delta;
        SetPosition(pos);
    }

    // Helper
    static float Lerp(float a, float b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return a + (b - a) * t;
    }

private:
    TransformRef playerBottom{};

    float moveSpeed = 5.0f;
    float jumpForce = 8.0f;
    float maxSlopeAngle = 45.0f;

    float groundRaycastDistance = 0.3f;
    float groundProbeStartOffset = 0.1f;
    float groundSnapOffset = 0.02f;
    float skinWidth = 0.05f;

    float airControl = 0.3f;
    float groundFriction = 20.0f;
    float maxFallSpeed = -50.0f;

    bool m_isGrounded = false;
    bool m_hasJumpedThisFrame = false;
    float m_colliderHalfHeight = 0.5f;

    RaycastHit m_lastGroundHit{};
};
