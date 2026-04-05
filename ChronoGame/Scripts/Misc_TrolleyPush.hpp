#pragma once
#include <cmath>
#include "EngineAPI.hpp"
#include "Player_Controller.hpp"

/*
 * Misc_TrolleyPush
 * ----------------
 * Attach this to a trolley / pushable object.
 *
 * While the player is touching (collision or trigger) the trolley, the trolley
 * moves in the same camera-relative WASD direction as the player.
 *
 * The script also includes a small proximity fallback because CharacterController
 * setups sometimes do not fire rigidbody collision callbacks reliably against
 * dynamic pushable objects.
 */
class Misc_TrolleyPush : public IScript {
public:
    Misc_TrolleyPush() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(playerCameraRef);
        SCRIPT_FIELD(moveSpeed, Float);
        SCRIPT_FIELD(pushDistance, Float);
        SCRIPT_FIELD(stopDamping, Float);
        SCRIPT_FIELD(useTriggerToo, Bool);
        SCRIPT_FIELD(useTransformFallback, Bool);
        SCRIPT_FIELD(debugLogs, Bool);
        SCRIPT_FIELD(pushAudioName, String);  // e.g. "TROLLEY_PUSH" (without event:/)
    }

    ~Misc_TrolleyPush() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { (void)entity; }

    void Start() override {
        CacheReferences();

        if (moveSpeed <= 0.0f)    moveSpeed = 2.75f;
        if (pushDistance <= 0.0f) pushDistance = 1.6f;
        if (stopDamping <= 0.0f)  stopDamping = 10.0f;

        isPushAudioPlaying = false;

        if (debugLogs) {
            LOG_DEBUG("Misc_TrolleyPush start - self={}, playerValid={}, cameraValid={}, hasRB={}",
                GetEntity(),
                playerRef.IsValid() ? 1 : 0,
                playerCameraRef.IsValid() ? 1 : 0,
                RB_HasRigidbody(GetEntity()) ? 1 : 0);
        }
    }

    void Update(double deltaTime) override {
        CacheReferences();

        Vec3 moveDir = GetPlayerMoveDirection();
        bool hasInput = moveDir.LengthSquared() > 0.0001f;
        bool touchingPlayer = isPlayerColliding || IsPlayerWithinPushDistance(moveDir, hasInput);

        if (!touchingPlayer || !hasInput) {
            DampHorizontalMotion(static_cast<float>(deltaTime));
            StopPushAudio();
            return;
        }

        // Only push if the player is generally moving toward the trolley.
        if (playerRef.IsValid()) {
            Vec3 toTrolley = TF_GetPosition(GetEntity()) - TF_GetPosition(playerRef.GetEntity());
            toTrolley.y = 0.0f;
            if (toTrolley.LengthSquared() > 0.0001f) {
                toTrolley.Normalize();
                if (moveDir.Dot(toTrolley) < 0.1f) {
                    DampHorizontalMotion(static_cast<float>(deltaTime));
                    StopPushAudio();
                    return;
                }
            }
        }

        PushTrolley(moveDir, static_cast<float>(deltaTime));
        StartPushAudio();
    }

    void OnDestroy() override { StopPushAudio(); }
    void OnEnable()  override {}
    void OnDisable() override { StopPushAudio(); }
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_TrolleyPush"; }

    void OnCollisionEnter(Entity other) override { if (IsPlayer(other)) isPlayerColliding = true; }
    void OnCollisionExit(Entity other)  override { if (IsPlayer(other)) isPlayerColliding = false; }
    void OnCollisionStay(Entity other)  override { if (IsPlayer(other)) isPlayerColliding = true; }

    void OnTriggerEnter(Entity other) override { if (useTriggerToo && IsPlayer(other)) isPlayerColliding = true; }
    void OnTriggerExit(Entity other)  override { if (useTriggerToo && IsPlayer(other)) isPlayerColliding = false; }
    void OnTriggerStay(Entity other)  override { if (useTriggerToo && IsPlayer(other)) isPlayerColliding = true; }

private:
    void CacheReferences() {
        if (!playerRef.IsValid()) {
            auto players = GameObject::FindObjectsOfType<Player_Controller>();
            if (!players.empty()) {
                playerRef.SetEntity(players.front().GetEntityId());
            }
        }

        if (!playerCameraRef.IsValid()) {
            GameObject cam = GameObject::Find("Camera");
            if (!cam.IsValid()) cam = GameObject::Find("PlayerCamera");
            if (!cam.IsValid()) cam = GameObject::Find("MainCamera");
            if (cam.IsValid()) {
                playerCameraRef.SetEntity(cam.GetEntityId());
            }
        }
    }

    bool IsPlayer(Entity other) const {
        return playerRef.IsValid() && other == playerRef.GetEntity();
    }

    Vec3 GetPlayerMoveDirection() const {
        Vec3 inputDirection = Vec3::Zero();
        if (Input::IsKeyDown('W')) inputDirection.z += 1.0f;
        if (Input::IsKeyDown('S')) inputDirection.z -= 1.0f;
        if (Input::IsKeyDown('A')) inputDirection.x -= 1.0f;
        if (Input::IsKeyDown('D')) inputDirection.x += 1.0f;

        if (inputDirection.LengthSquared() <= 0.0001f) return Vec3::Zero();
        inputDirection.Normalize();

        Entity basis = playerCameraRef.IsValid() ? playerCameraRef.GetEntity() : GetEntity();
        Vec3 cameraForward = TF_GetForward(basis);
        Vec3 cameraRight = TF_GetRight(basis);

        cameraForward.y = 0.0f;
        cameraRight.y = 0.0f;
        if (cameraForward.LengthSquared() > 0.0001f) cameraForward.Normalize();
        if (cameraRight.LengthSquared() > 0.0001f) cameraRight.Normalize();

        Vec3 moveDirection = (cameraRight * inputDirection.x) + (cameraForward * inputDirection.z);
        if (moveDirection.LengthSquared() <= 0.0001f) return Vec3::Zero();

        moveDirection.Normalize();
        return moveDirection;
    }

    bool IsPlayerWithinPushDistance(const Vec3& moveDir, bool hasInput) const {
        if (!playerRef.IsValid() || !hasInput) return false;

        Vec3 playerPos = TF_GetPosition(playerRef.GetEntity());
        Vec3 trolleyPos = TF_GetPosition(GetEntity());

        Vec3 horizontalDelta = trolleyPos - playerPos;
        horizontalDelta.y = 0.0f;
        float horizontalDistance = horizontalDelta.Length();
        float verticalDistance = std::fabs(trolleyPos.y - playerPos.y);

        if (horizontalDistance > pushDistance || verticalDistance > 2.0f) return false;

        if (horizontalDistance > 0.0001f) {
            Vec3 towardTrolley = horizontalDelta.Normalized();
            return moveDir.Dot(towardTrolley) > 0.1f;
        }

        return true;
    }

    void PushTrolley(const Vec3& moveDir, float deltaTime) {
        if (RB_HasRigidbody(GetEntity())) {
            Vec3 vel = RB_GetVelocity(GetEntity());
            vel.x = moveDir.x * moveSpeed;
            vel.z = moveDir.z * moveSpeed;
            RB_SetVelocity(vel, GetEntity());
            RB_SetAngularVelocity(Vec3::Zero(), GetEntity());
        }
        else if (useTransformFallback) {
            Vec3 pos = TF_GetPosition(GetEntity());
            pos += moveDir * (moveSpeed * deltaTime);
            TF_SetPosition(pos, GetEntity());
        }
    }

    void DampHorizontalMotion(float deltaTime) {
        if (RB_HasRigidbody(GetEntity())) {
            Vec3 vel = RB_GetVelocity(GetEntity());
            float t = stopDamping * deltaTime;
            if (t > 1.0f) t = 1.0f;
            vel.x += (0.0f - vel.x) * t;
            vel.z += (0.0f - vel.z) * t;
            RB_SetVelocity(vel, GetEntity());
        }
    }

    void StartPushAudio() {
        if (isPushAudioPlaying || pushAudioName.empty()) return;
        PlayAudio("event:/" + pushAudioName);
        isPushAudioPlaying = true;
    }

    void StopPushAudio() {
        if (!isPushAudioPlaying || pushAudioName.empty()) return;
        StopAudio("event:/" + pushAudioName);
        isPushAudioPlaying = false;
    }

private:
    GameObjectRef playerRef;
    GameObjectRef playerCameraRef;
    float moveSpeed = 2.75f;
    float pushDistance = 1.6f;
    float stopDamping = 10.0f;
    bool  useTriggerToo = true;
    bool  useTransformFallback = true;
    bool  debugLogs = false;
    std::string pushAudioName = "";   // FMOD event name without "event:/"

    bool isPlayerColliding = false;
    bool isPushAudioPlaying = false;
};