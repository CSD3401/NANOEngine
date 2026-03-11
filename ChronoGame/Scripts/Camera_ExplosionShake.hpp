#pragma once
#include <algorithm>
#include <cmath>
#include "EngineAPI.hpp"
#include "Player_Controller.hpp"

/*
* Camera_ExplosionShake
* Attach this to the PLAYER CAMERA entity.
*
* Uses the same event-listening pattern as Camera_FOVPulse.
* When triggered, it applies an explosive decaying rotation burst to the camera.
*
* It does this in two ways:
* 1) Directly rotates the camera entity itself (FOVPulse-style self-contained behavior).
* 2) Also pushes an additive offset into Player_Controller as a fallback in case the
*    player controller writes camera rotation later in the frame.
*/
class Camera_ExplosionShake : public IScript {
public:
    Camera_ExplosionShake() {
        SCRIPT_FIELD(listenEventName, String);
        SCRIPT_FIELD(durationSeconds, Float);
        SCRIPT_FIELD(frequency, Float);
        SCRIPT_FIELD(maxPitchDegrees, Float);
        SCRIPT_FIELD(maxYawDegrees, Float);
        SCRIPT_FIELD(maxRollDegrees, Float);
        SCRIPT_FIELD(enabledAtStart, Bool);
    }

    ~Camera_ExplosionShake() override = default;

    void Awake() override {
        listeningEnabled = enabledAtStart;
        RegisterEventListeners();
    }

    void Initialize(Entity entity) override { (void)entity; }

    void Start() override {
        CachePlayerController();
        durationSeconds = std::max(0.01f, durationSeconds);
        frequency = std::max(0.01f, frequency);
        lastDirectOffset = Vec3::Zero();

        if (!HasCamera()) {
            LOG_WARNING("Camera_ExplosionShake: entity has no Camera component. Direct rotation fallback will still be attempted.");
        }
    }

    void Update(double deltaTime) override {
        if (!isShaking) {
            return;
        }

        if (!cachedPlayerController) {
            CachePlayerController();
        }

        elapsedSeconds += static_cast<float>(deltaTime);
        float normalizedTime = std::clamp(elapsedSeconds / durationSeconds, 0.0f, 1.0f);
        float decay = 1.0f - normalizedTime;
        decay *= decay;

        float time = elapsedSeconds * frequency;
        Vec3 offset = Vec3(
            maxPitchDegrees * decay * (std::sin(time * 23.0f + phasePitchA) + 0.45f * std::sin(time * 41.0f + phasePitchB)),
            maxYawDegrees * decay * (std::cos(time * 19.0f + phaseYawA) + 0.35f * std::sin(time * 37.0f + phaseYawB)),
            maxRollDegrees * decay * (std::sin(time * 29.0f + phaseRollA) + 0.55f * std::cos(time * 47.0f + phaseRollB))
        );

        ApplyShakeOffset(offset);

        if (normalizedTime >= 1.0f) {
            StopShake();
        }
    }

    void OnDestroy() override { StopShake(); }
    void OnEnable() override { listeningEnabled = true; }

    void OnDisable() override {
        listeningEnabled = false;
        StopShake();
    }

    void OnValidate() override {}
    const char* GetTypeName() const override { return "Camera_ExplosionShake"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    std::string listenEventName = "ExplosionCameraShake";
    float durationSeconds = 0.65f;
    float frequency = 1.25f;
    float maxPitchDegrees = 18.0f;
    float maxYawDegrees = 10.0f;
    float maxRollDegrees = 24.0f;
    bool enabledAtStart = true;

    bool eventsRegistered = false;
    bool listeningEnabled = true;
    bool isShaking = false;
    float elapsedSeconds = 0.0f;
    float triggerCounter = 0.0f;

    float phasePitchA = 0.0f;
    float phasePitchB = 0.0f;
    float phaseYawA = 0.0f;
    float phaseYawB = 0.0f;
    float phaseRollA = 0.0f;
    float phaseRollB = 0.0f;

    Vec3 lastDirectOffset = Vec3::Zero();
    Player_Controller* cachedPlayerController = nullptr;

    void RegisterEventListeners() {
        if (eventsRegistered || listenEventName.empty()) {
            return;
        }

        Events::Listen(listenEventName.c_str(), [this](void*) {
            if (!listeningEnabled) {
                return;
            }
            TriggerShake();
            });

        eventsRegistered = true;
    }

    void CachePlayerController() {
        cachedPlayerController = nullptr;

        Entity parent = GetParent(GetEntity());
        if (parent != NE::Scripting::INVALID_ENTITY) {
            cachedPlayerController = GameObject(parent).GetComponent<Player_Controller>();
        }

        if (!cachedPlayerController) {
            auto players = GameObject::FindObjectsOfType<Player_Controller>();
            if (!players.empty()) {
                cachedPlayerController = players.begin()->GetComponent<Player_Controller>();
            }
        }
    }

    void TriggerShake() {
        triggerCounter += 1.0f;
        elapsedSeconds = 0.0f;
        isShaking = true;

        phasePitchA = 0.73f * triggerCounter;
        phasePitchB = 1.41f * triggerCounter;
        phaseYawA = 0.97f * triggerCounter;
        phaseYawB = 1.87f * triggerCounter;
        phaseRollA = 1.19f * triggerCounter;
        phaseRollB = 2.11f * triggerCounter;

        lastDirectOffset = Vec3::Zero();
        ApplyShakeOffset(Vec3::Zero());
        LOG_DEBUG("Camera_ExplosionShake: shake triggered.");
    }

    void ApplyShakeOffset(const Vec3& offset) {
        if (cachedPlayerController) {
            cachedPlayerController->SetExplosionCameraOffset(offset);
        }

        Vec3 currentRotation = TF_GetRotation(GetEntity());
        Vec3 baseRotation = currentRotation - lastDirectOffset;
        TF_SetRotation(baseRotation + offset, GetEntity());
        lastDirectOffset = offset;
    }

    void StopShake() {
        if (cachedPlayerController) {
            cachedPlayerController->ClearExplosionCameraOffset();
        }

        Vec3 currentRotation = TF_GetRotation(GetEntity());
        TF_SetRotation(currentRotation - lastDirectOffset, GetEntity());

        lastDirectOffset = Vec3::Zero();
        isShaking = false;
        elapsedSeconds = 0.0f;
    }
};
