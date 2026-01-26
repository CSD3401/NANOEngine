#pragma once
#include <algorithm>
#include <cmath>
#include "EngineAPI.hpp"
#include "Manager_.hpp"

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
    }
    ~Player_Controller() override = default;

    // === Custom Methods ===
    void Reset()
    {
        lookRotation = Vec3::Zero();
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
        auto v = GameObject::FindObjectsOfType<Manager_>();
        if (v.size() == 0) {
            LOG_ERROR("No managers found!");
        }
        else if (v.size() > 1) {
            LOG_WARNING("Multiple managers found!");
        }
        else {
            manager = v.begin()->GetComponent<Manager_>();
        }

		// Reset state
        Reset();
    }

    void Update(double deltaTime) override {
        // === Grounded ===
        bool isGrounded = CC_IsGrounded();

        // === Camera controls ===
        std::pair<double, double> const& mouseDelta = Input::GetMouseDelta();
        lookRotation.x += static_cast<float>(mouseDelta.first) * lookSensitivity;   // Yaw
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
        static bool wasJumpKeyDown = false;
        bool isJumpKeyDown = Input::IsKeyDown(' ');
        if (isGrounded && isJumpKeyDown && !wasJumpKeyDown)
        {
            // Todo
        }

        // Assign
    }
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Player_Controller"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    // === Manager ===
    Manager_* manager;

    // === Camera ===
	GameObjectRef playerCameraRef;
	Entity playerCameraEntity;
    Vec3 lookRotation;
    float lookSensitivity;

    // === Movement ===
    float moveSpeed;
    float jumpStrength;
};