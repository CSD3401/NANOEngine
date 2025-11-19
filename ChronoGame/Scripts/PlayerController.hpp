#pragma once
#include "EngineAPI.hpp"
#include <cmath>

// GLFW key codes
#define GLFW_KEY_SPACE 32

/**
 * KINEMATIC 3D player controller.
 *
 * - Does NOT require a Rigidbody.
 * - Requires: Transform + Collider (for getting size and doing raycasts vs world).
 * - Maintains its own velocity (m_velocity) and moves by directly setting Transform.
 * - Uses raycast-based ground detection & simple raycast-based wall blocking.
 *
 * NOTE:
 * - jumpForce is now effectively "jumpSpeed" (initial upward velocity).
 * - manualGravity should be NEGATIVE (e.g. -18.81f).
 */
class PlayerController : public NE::Scripting::IScript {
public:
    PlayerController() {
        REGISTER_FIELD(moveSpeed);
        REGISTER_FIELD(jumpForce);          // interpreted as jump speed now
        REGISTER_FIELD(manualGravity);      // should be negative
        REGISTER_FIELD(frictionCoefficient);
        REGISTER_FIELD(maxSlopeAngle);
        REGISTER_FIELD(groundRaycastDistance);
    }

    ~PlayerController() override = default;

    void Awake() override {}

    void Initialize(NE::ECS::Entity entity) override {
        _SetEntity(entity);
    }

    void Start() override {
        // Try to get collider half-height so we know where the "feet" are
        if (NE::ECS::Command::HasComponent<NE::ECS::Component::Collider>(GetEntity())) {

            auto& col = NE::ECS::Command::GetComponent<NE::ECS::Component::Collider>(GetEntity());
            m_colliderHalfHeight = col.halfExtents.y;
            LOG_INFO("Kinematic PlayerController: collider half-height = " << m_colliderHalfHeight);
        } else {
            LOG_WARNING("Kinematic PlayerController: no Collider found on entity "
                << GetEntity() << " – ground checks may be inaccurate.");
        }

        m_velocity = { 0.0f, 0.0f, 0.0f };
        m_isGrounded = false;
    }

    void Update(double deltaTime) override {
        const float dt = static_cast<float>(deltaTime);

        // 1. Update grounded state via raycast (from current position)
        UpdateGroundedState();

        // 2. Handle horizontal movement input (world-space XZ)
        UpdateHorizontalVelocity(dt);

        // 3. Handle jump input
        HandleJump(dt);

        // 4. Apply gravity (when not grounded)
        ApplyGravity(dt);

        // 5. Move character kinematically, with simple wall collision using raycast
        MoveKinematic(dt);
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}

    const char* GetTypeName() const override {
        return "KinematicPlayerController";
    }

    // Collision callbacks not used in kinematic mode
    void OnCollisionEnter(NE::ECS::Entity) override {}
    void OnCollisionExit(NE::ECS::Entity) override {}
    void OnTriggerEnter(NE::ECS::Entity) override {}
    void OnTriggerExit(NE::ECS::Entity) override {}

    // Exposed fields
    std::vector<std::string> GetExposedFieldNames() const override { return m_fields.GetNames(); }
    std::string GetFieldType(const std::string& name) const override { return m_fields.GetType(name); }
    std::string GetFieldValueAsString(const std::string& name) const override { return m_fields.GetValue(name); }
    bool SetFieldValueFromString(const std::string& name, const std::string& value) override {
        return m_fields.SetValue(name, value);
    }

private:
    // =========================
    // GROUND CHECK
    // =========================
    void UpdateGroundedState() {
        // If we're moving upwards noticeably, don't try to snap to ground
        if (m_velocity.y > 0.1f) {
            m_isGrounded = false;
            return;
        }

        NE::Scripting::Vec3 pos = GetPosition();

        // Current feet position (center - halfHeight)
        float feetY = pos.y - m_colliderHalfHeight;

        // Start the ray a bit ABOVE the feet so we aren't starting inside the floor
        NE::Scripting::Vec3 origin = pos;
        origin.y = feetY + m_groundProbeStartOffset; // e.g. small positive offset

        NE::Scripting::Vec3 downDir{ 0.0f, -1.0f, 0.0f };
        uint32_t layerMask = 0xFFFFFFFF;

        float rayLen = groundRaycastDistance + m_groundProbeStartOffset + m_skinWidth;

        NE::Scripting::RaycastHit hit = Raycast(origin, downDir, rayLen, layerMask);

        if (hit.hasHit && hit.entity != GetEntity()) {
            m_isGrounded = true;

            // Hit point Y = origin.y - distance
            float hitY = origin.y - hit.distance;

            // Where we want the FEET to end up:
            float targetFeetY = hitY + m_groundSnapOffset;

            // Center = feet + halfHeight
            float targetCenterY = targetFeetY + m_colliderHalfHeight;

            NE::Scripting::Vec3 newPos = pos;
            newPos.y = targetCenterY;
            SetPosition(newPos);

            if (m_velocity.y < 0.0f) {
                m_velocity.y = 0.0f;
            }
        } else {
            m_isGrounded = false;
        }

        // Optional debug
        //SPD_DEBUG("Grounded: " << m_isGrounded
        //    << " centerY=" << GetPosition().y
        //    << " feetY=" << (GetPosition().y - m_colliderHalfHeight)
        //    << " velY=" << m_velocity.y);
    }

    // =========================
    // HORIZONTAL MOVEMENT
    // =========================
    void UpdateHorizontalVelocity(float dt) {
        NE::Math::Vec3 inputDir{ 0.0f, 0.0f, 0.0f };

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

        // Current horizontal velocity
        NE::Scripting::Vec3 horizVel = m_velocity;
        horizVel.y = 0.0f;

        if (mag > 0.0f) {
            // Target horizontal velocity
            NE::Math::Vec3 targetVel{
                inputDir.x * moveSpeed,
                0.0f,
                inputDir.z * moveSpeed
            };

            // If grounded, snap quickly; if in air, lerp slowly (air control)
            float accel = m_isGrounded ? 1.0f : m_airControl;
            horizVel.x = Lerp(horizVel.x, targetVel.x, accel * dt);
            horizVel.z = Lerp(horizVel.z, targetVel.z, accel * dt);
        } else if (m_isGrounded) {
            // Apply friction when grounded and no input
            float speed = std::sqrt(horizVel.x * horizVel.x + horizVel.z * horizVel.z);
            if (speed > 0.01f) {
                float friction = frictionCoefficient * dt;
                float newSpeed = std::max(0.0f, speed - friction);
                float factor = (speed > 0.0f) ? (newSpeed / speed) : 0.0f;

                horizVel.x *= factor;
                horizVel.z *= factor;
            } else {
                horizVel.x = 0.0f;
                horizVel.z = 0.0f;
            }
        }

        m_velocity.x = horizVel.x;
        m_velocity.z = horizVel.z;
    }

    // =========================
    // JUMP
    // =========================
    void HandleJump(float /*dt*/) {
        if (Input::WasKeyPressed(GLFW_KEY_SPACE)) {
            if (m_isGrounded && !m_hasJumpedThisFrame) {
                LOG_INFO("Kinematic jump!");
                m_hasJumpedThisFrame = true;
                m_isGrounded = false;
                m_velocity.y = jumpForce; // treat as initial upward velocity
            }
        }

        if (!Input::IsKeyDown(GLFW_KEY_SPACE)) {
            m_hasJumpedThisFrame = false;
        }
    }

    // =========================
    // GRAVITY
    // =========================
    void ApplyGravity(float dt) {
        if (!m_isGrounded) {
            m_velocity.y += manualGravity * dt;

            if (m_velocity.y < m_maxFallSpeed) {
                m_velocity.y = m_maxFallSpeed;
            }
        } else {
            if (m_velocity.y < 0.0f) {
                m_velocity.y = 0.0f;
            }
        }
    }

    // =========================
    // KINEMATIC MOVEMENT
    // =========================
    void MoveKinematic(float dt) {
        NE::Scripting::Vec3 pos = GetPosition();
        NE::Scripting::Vec3 displacement = m_velocity * dt;

        // Separate horizontal and vertical
        NE::Scripting::Vec3 horizMove{ displacement.x, 0.0f, displacement.z };
        float horizLen = std::sqrt(horizMove.x * horizMove.x + horizMove.z * horizMove.z);

        if (horizLen > 0.0001f) {
            NE::Scripting::Vec3 dir{
                horizMove.x / horizLen,
                0.0f,
                horizMove.z / horizLen
            };

            // Simple wall blocking via raycast
            uint32_t layerMask = 0xFFFFFFFF;
            float rayLen = horizLen + m_skinWidth;

            NE::Scripting::RaycastHit hit = Raycast(pos, dir, rayLen, layerMask);

            if (hit.hasHit && hit.entity != GetEntity()) {
                float allowed = std::max(0.0f, hit.distance - m_skinWidth);
                if (allowed < horizLen) {
                    horizMove = dir * allowed;
                }
            }
        }

        NE::Scripting::Vec3 finalMove{
            horizMove.x,
            displacement.y,
            horizMove.z
        };

        pos = pos + finalMove;
        SetPosition(pos);
    }

    // Simple helper
    static float Lerp(float a, float b, float t) {
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return a + (b - a) * t;
    }

    // === EDITABLE PARAMETERS ===

    // Movement parameters
    float moveSpeed = 5.0f;
    float jumpForce = 8.0f;        // now used as jump speed (units/sec)
    float manualGravity = -18.81f; // should be negative
    float frictionCoefficient = 20.0f;
    float maxSlopeAngle = 45.0f;   // (not used yet, placeholder for slope handling)

    // Ground detection
    float groundRaycastDistance = 0.3f; // ray length below player
    float m_groundProbeStartOffset = 0.1f;  // start slightly above feet
    float m_groundSnapOffset = 0.02f;       // snap distance into ground
    float m_skinWidth = 0.05f;              // used for wall/gap tolerance

    // Air control
    float m_airControl = 0.3f;              // 0 = no air control, 1 = full

    // Fall limit
    float m_maxFallSpeed = -50.0f;

    // Internal state
    bool m_hasJumpedThisFrame = false;
    bool m_isGrounded = false;
    NE::Scripting::Vec3 m_velocity{ 0.0f, 0.0f, 0.0f };
    float m_colliderHalfHeight = 0.5f;

    // Field registry for editor
    ExposedFieldRegistry m_fields;
};
