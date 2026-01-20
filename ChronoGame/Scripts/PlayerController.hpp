#pragma once
#include "EngineAPI.hpp"
#include <cmath>
#include <algorithm>
#include <ScriptSDK/Math.h>

class PlayerController : public IScript {
public:
    PlayerController() = default;
    ~PlayerController() override = default;

    void Awake() override {}

    void Initialize(Entity entity) override {
        SCRIPT_FIELD(moveSpeed, Float);
    }

    void Start() override {

    }

    void Update(double deltaTime) override {
        const float gravity = -9.81f;
        const float stick = -2.0f;

        bool isGrounded = CC_IsGrounded();

        Vec3 inputDir{ 0,0,0 };
        if (Input::IsKeyDown('W')) inputDir.x += 1.0f;
        if (Input::IsKeyDown('S')) inputDir.x -= 1.0f;
        if (Input::IsKeyDown('A')) inputDir.z -= 1.0f;
        if (Input::IsKeyDown('D')) inputDir.z += 1.0f;

        float mag = std::sqrt(inputDir.x * inputDir.x + inputDir.z * inputDir.z);
        if (mag > 0.01f) { inputDir.x /= mag; inputDir.z /= mag; } 
        else { inputDir.x = 0; inputDir.z = 0; }

        static bool wasJumpKeyDown = false;
        bool isJumpKeyDown = Input::IsKeyDown(' ');
        if (isGrounded && isJumpKeyDown && !wasJumpKeyDown) {
            const float jumpHeight = 1.5f;
            playerVelocity.y = std::sqrt(jumpHeight * -2.0f * gravity);
            isGrounded = false;
        }
        wasJumpKeyDown = isJumpKeyDown;

        if (isGrounded) {
            if (playerVelocity.y < 0.f) playerVelocity.y = 0.f;
        } else {
            playerVelocity.y += gravity * (float)deltaTime;
        }

        Vec3 horizVel = inputDir * moveSpeed;

        Vec3 finalVel = horizVel;

        if (isGrounded) {
            Vec3 n = CC_GetGroundNormal();
            finalVel = finalVel - n * finalVel.Dot(n);
            finalVel += n * stick;
        } else {
            finalVel.y = playerVelocity.y;
        }

        CC_Move(finalVel * (float)deltaTime);
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}

    const char* GetTypeName() const override {
        return "PlayerController";
    }

    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    float moveSpeed = 5.0f;
	Vec3 playerVelocity{ 0.0f, 0.0f, 0.0f };
};
