#pragma once
#include <algorithm>
#include <cmath>
#include "EngineAPI.hpp"
#include "Misc_Manager.hpp"

#define GLFW_KEY_SPACE 32
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Player_Controller is script which handles player input and movement.
* It also handles camera controls.
*/

class Player_Controller : public IScript {
public:
    Player_Controller() {
		SCRIPT_GAMEOBJECT_REF(playerCameraRef);
        SCRIPT_FIELD(lookSensitivity, Float);
        SCRIPT_FIELD(moveSpeed, Float);
        SCRIPT_FIELD(jumpStrength, Float);
        SCRIPT_FIELD(snappiness, Float);
		SCRIPT_FIELD(gravity, Float);
    }
    ~Player_Controller() override = default;

    // === Custom Methods ===
    void Reset()
    {
        lookRotation = Vec3::Zero();
        velocity = Vec3::Zero();
    }

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
		// Get player camera entity
        if (playerCameraRef.IsValid()) {
			playerCameraEntity = playerCameraRef.GetEntity();
        }

		// Find Manager_
        auto v = GameObject::FindObjectsOfType<Misc_Manager>();
        if (v.size() == 0) {
            LOG_ERROR("No managers found!");
        }
        else if (v.size() > 1) {
            LOG_WARNING("Multiple managers found!");
        }
        else {
            manager = v.begin()->GetComponent<Misc_Manager>();
        }

		// Reset state
        Reset();
    }

    void Update(double deltaTime) override {
        // === Grounded ===
        bool isGrounded = CC_IsGrounded();

        // === Camera controls ===
        std::pair<double, double> const& mouseDelta = Input::GetMouseDelta();
        lookRotation.x -= static_cast<float>(mouseDelta.first) * lookSensitivity;   // Yaw
        lookRotation.y -= static_cast<float>(mouseDelta.second) * lookSensitivity;  // Pitch
        lookRotation.y = std::clamp(lookRotation.y, -89.0f, 89.0f);
        CC_Rotate(lookRotation.x);
        TF_SetRotation({ lookRotation.y, 0.0f, 0.0f }, playerCameraEntity);

        // === Movement controls ===
        // Input Direction
        Vec3 inputDirection = Vec3::Zero();
        if (Input::IsKeyDown('W')) { inputDirection.z += 1.0f; }
        if (Input::IsKeyDown('S')) { inputDirection.z -= 1.0f; }
        if (Input::IsKeyDown('A')) { inputDirection.x -= 1.0f; }
        if (Input::IsKeyDown('D')) { inputDirection.x += 1.0f; }
        inputDirection.Normalize();

        // Camera-relative direction
        Vec3 cameraForward = TF_GetForward(playerCameraEntity);
        Vec3 cameraRight = TF_GetRight(playerCameraEntity);

        // Movement direction
        Vec3 moveDirection = (cameraRight * inputDirection.x) + (cameraForward * inputDirection.z);
        moveDirection.Normalize();

        // === Jumping ===
        if (isGrounded)
        {   
			velocity.y = -2.0f; // Small downward force to keep grounded
            if (Input::IsKeyDown(' '))
            {
                velocity.y = jumpStrength;
                isGrounded = false;
            }
        }
        else
        {
			velocity.y -= gravity * static_cast<float>(deltaTime);
        }

		// === Velocity Smoothing ===
        Vec3 targetVelocity
        {
            moveDirection.x * moveSpeed,
            velocity.y,
            moveDirection.z * moveSpeed
        };
		velocity.x = manager->SnappyLerp(
            velocity.x, 
            targetVelocity.x, 
            snappiness, 
            static_cast<float>(deltaTime)
        );
        velocity.z = manager->SnappyLerp(
            velocity.z,
            targetVelocity.z,
            snappiness,
            static_cast<float>(deltaTime)
        );

        // Assign
        CC_Move(velocity * static_cast<float>(deltaTime));
    }
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Player_Controller"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    // === Manager ===
    Misc_Manager* manager;

    // === Camera ===
	GameObjectRef playerCameraRef;
	Entity playerCameraEntity;
    Vec3 lookRotation;
    float lookSensitivity;

    // === Movement ===
    Vec3 velocity;
    float moveSpeed;
    float jumpStrength;
    float snappiness;
	float gravity;
};