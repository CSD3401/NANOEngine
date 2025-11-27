#pragma once
#include "EngineAPI.hpp"
#include <cmath>
#include <algorithm>

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
 *   This version makes WASD movement camera-relative. Assign your camera
 *   Transform in the editor via `cameraTransform`.
 */

    using namespace ChronoGame;

class PlayerController : public IScript {
public:
    PlayerController() = default;
    ~PlayerController() override = default;

    void Awake() override {}

    void Initialize(Entity entity) override {
        // Register editable fields
        SCRIPT_FIELD(moveSpeed, Float);
        SCRIPT_FIELD(jumpForce, Float);
        SCRIPT_FIELD(manualGravity, Float);
        SCRIPT_FIELD(frictionCoefficient, Float);
        SCRIPT_FIELD(maxSlopeAngle, Float);
        SCRIPT_FIELD(groundRaycastDistance, Float);
        // Expose camera Transform reference (explicit, no macro)
        RegisterTransformRefField("cameraTransform", &cameraTransform);
    }

    void Start() override {
        // Try to get collider half-height so we know where the "feet" are
        if (Command::HasComponent<Component::Collider>(GetEntity())) {
            auto& col = Command::GetComponent<Component::Collider>(GetEntity());
            m_colliderHalfHeight = col.halfExtents.y;
            LOG_INFO("Kinematic PlayerController: collider half-height = " << m_colliderHalfHeight);
        }
        else {
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

        // 2. Handle horizontal movement input (camera-relative XZ)
        UpdateHorizontalVelocity(dt);

        // 3. Handle jump input
        HandleJump(dt);

        // 4. Integrate vertical velocity (gravity etc.)
        ApplyVerticalPhysics(dt);

        // 5. Move character (kinematic)
        MoveKinematic(dt);
    }

    // Collision callbacks (kept for completeness)
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    // =========================
    // INPUT / MOVEMENT
    // =========================
    void UpdateHorizontalVelocity(float dt) {
        // Read raw WASD input
        float h = 0.0f;
        float v = 0.0f;
        if (Input::IsKeyDown('D')) h += 1.0f;
        if (Input::IsKeyDown('A')) h -= 1.0f;
        if (Input::IsKeyDown('W')) v += 1.0f;
        if (Input::IsKeyDown('S')) v -= 1.0f;

        Vec3 camFwd = { 0.0f, 0.0f, -1.0f };
        Vec3 camRight = { 1.0f, 0.0f,  0.0f };

        if (cameraTransform.IsValid()) {
            Vec3 camEuler = GetRotation(cameraTransform.GetEntity()); // (pitch, yaw, roll) in degrees
            float yawRad = camEuler.y * (3.14159265359f / 180.0f);

            // Swapped basis for your coordinate frame:
            // forward = (cos(yaw), 0, sin(yaw))  -> at yaw=0, +X
            // right   = (-sin(yaw), 0, cos(yaw)) -> at yaw=0, +Z
            float cy = std::cos(yawRad);
            float sy = std::sin(yawRad);

            camFwd = { cy, 0.0f,  sy };
            camRight = { -sy, 0.0f,  cy };
        }

        // Desired move direction relative to camera
        Vec3 desired = camFwd * v + camRight * h;
        if (desired.LengthSquared() > 1e-6f) desired = desired.Normalized() * moveSpeed;
        else {
            desired = Vec3(0.0f, 0.0f, 0.0f);
        }

        // Apply on ground vs in air
        if (m_isGrounded) {
            // Snap horizontal velocity to desired when grounded
            m_velocity.x = desired.x;
            m_velocity.z = desired.z;
            // Apply simple ground friction
            float drag = std::max(0.0f, 1.0f - frictionCoefficient * dt);
            m_velocity.x *= drag;
            m_velocity.z *= drag;
        }
        else {
            // Air control: blend current horizontal velocity toward desired
            float t = m_airControl * dt * 5.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            m_velocity.x = m_velocity.x * (1.0f - t) + desired.x * t;
            m_velocity.z = m_velocity.z * (1.0f - t) + desired.z * t;
        }

        // Vertical velocity handled elsewhere (gravity/jump)
    }

    void HandleJump(float /*dt*/) {
        m_hasJumpedThisFrame = false;

        // Use WasKeyPressed from your SDK wrapper
        if (Input::WasKeyPressed(GLFW_KEY_SPACE) && m_isGrounded) {
            m_velocity.y = jumpForce;
            m_isGrounded = false;
            m_hasJumpedThisFrame = true;
        }
    }

    void ApplyVerticalPhysics(float dt) {
        if (!m_isGrounded) {
            // Apply manual gravity when airborne
            m_velocity.y += manualGravity * dt;

            if (m_velocity.y < m_maxFallSpeed) {
                m_velocity.y = m_maxFallSpeed;
            }
        }
        else {
            if (m_velocity.y < 0.0f) {
                m_velocity.y = 0.0f;
            }
        }
    }

    // =========================
    // KINEMATIC MOVEMENT
    // =========================
    void MoveKinematic(float dt) {
        // Compute desired displacement for this frame
        Vec3 displacement = m_velocity * dt;

        // Simple step: try to move horizontally, then vertically.
        Vec3 pos = GetPosition();
        Vec3 newPos = pos;

        // Horizontal XZ
        Vec3 horiz = { displacement.x, 0.0f, displacement.z };

        if (horiz.LengthSquared() > 0.0f) {
            // Raycast forward in movement direction to avoid penetrating walls.
            Vec3 dir = horiz.Normalized();
            float dist = horiz.Length();

            RaycastHit hit = Raycast(pos, dir, dist + m_skinWidth, worldCollisionMask);
            if (hit.hasHit && hit.entity != GetEntity()) {
                // Stop just before the wall
                float allowed = std::max(0.0f, hit.distance - m_skinWidth);
                newPos += dir * allowed;

                // Remove horizontal component into wall
                m_velocity.x = 0.0f;
                m_velocity.z = 0.0f;
            }
            else {
                newPos += horiz;
            }
        }

        // Vertical Y
        float vMove = displacement.y;
        if (std::abs(vMove) > 0.0f) {
            Vec3 vDir = { 0.0f, (vMove > 0.0f) ? 1.0f : -1.0f, 0.0f };
            float vDist = std::abs(vMove) + m_skinWidth;

            RaycastHit vhit = Raycast(newPos, vDir, vDist, worldCollisionMask);
            if (vhit.hasHit && vhit.entity != GetEntity()) {
                float allowed = std::max(0.0f, vhit.distance - m_skinWidth);
                newPos.y += vDir.y * allowed;

                if (vDir.y < 0.0f) {
                    // Hit ground from above
                    m_isGrounded = true;
                }

                // Stop vertical velocity on collision
                m_velocity.y = 0.0f;
            }
            else {
                newPos.y += vMove;
            }
        }

        // Commit move
        SetPosition(newPos);
    }

    // =========================
    // GROUND CHECK
    // =========================
    void UpdateGroundedState() {
        // Raycast straight down from character center to find ground
        float rayLen = m_colliderHalfHeight + groundRaycastDistance;
        Vec3 origin = GetPosition();

        Vec3 downDir = { 0.0f, -1.0f, 0.0f };
        RaycastHit hit = Raycast(origin, downDir, rayLen, layerMask);

        if (hit.hasHit && hit.entity != GetEntity()) {
            m_isGrounded = true;

            // Hit point Y = origin.y - distance
            float hitY = origin.y - hit.distance;

            // Where we want the FEET to end up:
            float targetFeetY = hitY + m_groundSnapOffset;

            // Center = feet + halfHeight
            float targetCenterY = targetFeetY + m_colliderHalfHeight;

            // If we’re within snap distance, gently snap to ground to prevent hovering
            float error = targetCenterY - origin.y;
            if (std::abs(error) < 0.25f) {
                SetPosition({ origin.x, origin.y + error, origin.z });
            }
        }
        else {
            m_isGrounded = false;
        }
    }

private:
    // ======= Exposed fields (editable in editor) =======
    float moveSpeed = 6.0f;                // horizontal units/sec
    float jumpForce = 7.5f;                // initial Y velocity when jumping
    float manualGravity = -18.0f;          // gravity accel (neg = downward)
    float frictionCoefficient = 5.0f;      // basic ground damping
    float maxSlopeAngle = 45.0f;           // not fully used here
    float groundRaycastDistance = 0.2f;    // extra ray length below feet for detection

    // ======= Collision and ground layers =======
    uint32_t worldCollisionMask = 0xFFFFFFFF;  // world blocking mask
    uint32_t layerMask = 0xFFFFFFFF;           // ground check mask

    // ======= Ground snapping =======
    float m_groundSnapOffset = 0.02f;      // small offset to avoid clipping into floor
    float m_skinWidth = 0.05f;             // used for wall/gap tolerance

    // Air control
    float m_airControl = 0.3f;              // 0 = no air control, 1 = full

    // Fall limit
    float m_maxFallSpeed = -50.0f;

    // Camera Transform reference (set in editor). Movement will follow this camera's forward/right.
    TransformRef cameraTransform{};

    // Internal state
    bool m_hasJumpedThisFrame = false;
    bool m_isGrounded = false;
    Vec3 m_velocity{ 0.0f, 0.0f, 0.0f };
    float m_colliderHalfHeight = 0.5f;
};
