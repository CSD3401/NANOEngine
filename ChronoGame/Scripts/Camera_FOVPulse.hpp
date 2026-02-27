#pragma once
#include <algorithm>
#include "EngineAPI.hpp"

// Attach this script to the CAMERA entity (commonly a child of the player).
// When the time switches to past/present (ChronoActivated / ChronoDeactivated),
// the camera FOV will smoothly pulse: baseFov -> pulseFov -> baseFov.

class Camera_FOVPulse : public IScript {
public:
    Camera_FOVPulse() {
        SCRIPT_FIELD(baseFov, Float);
        SCRIPT_FIELD(pulseFov, Float);
        SCRIPT_FIELD(riseTimeSeconds, Float);
        SCRIPT_FIELD(fallTimeSeconds, Float);
        SCRIPT_FIELD(enabledAtStart, Bool);
    }

    ~Camera_FOVPulse() override = default;

    void Awake() override {
        listeningEnabled = enabledAtStart;
        RegisterEventListeners();
    }

    void Initialize(Entity entity) override { (void)entity; }

    void Start() override {
        if (!HasCamera()) {
            LOG_WARNING("Camera_FOVPulse: Entity has no Camera component. Disabling script.");
            SetEnabled(false);
            return;
        }

        // If designer didn't set baseFov, read it from the camera at runtime.
        if (baseFov <= 0.0f) {
            baseFov = GetCameraFOV();
        } else {
            SetCameraFOV(baseFov);
        }

        // Safety clamps
        baseFov = std::clamp(baseFov, 1.0f, 179.0f);
        pulseFov = std::clamp(pulseFov, 1.0f, 179.0f);
        riseTimeSeconds = std::max(0.01f, riseTimeSeconds);
        fallTimeSeconds = std::max(0.01f, fallTimeSeconds);
    }

    void Update(double deltaTime) override {
        if (!isPulsing) {
            return;
        }

        float dt = static_cast<float>(deltaTime);
        phaseT += dt;

        if (phase == Phase::Rise) {
            float u = std::clamp(phaseT / riseTimeSeconds, 0.0f, 1.0f);
            SetCameraFOV(Lerp(baseFov, pulseFov, SmoothStep(u)));
            if (u >= 1.0f) {
                phase = Phase::Fall;
                phaseT = 0.0f;
            }
        } else { // Fall
            float u = std::clamp(phaseT / fallTimeSeconds, 0.0f, 1.0f);
            SetCameraFOV(Lerp(pulseFov, baseFov, SmoothStep(u)));
            if (u >= 1.0f) {
                isPulsing = false;
                phase = Phase::Rise;
                phaseT = 0.0f;
                SetCameraFOV(baseFov);
            }
        }
    }

    void OnDestroy() override { listeningEnabled = false; }

    void OnEnable() override { listeningEnabled = true; }
    void OnDisable() override { listeningEnabled = false; }
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Camera_FOVPulse"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    enum class Phase { Rise, Fall };

    // Inspector fields
    float baseFov = 60.0f;           // Default vertical FOV when not pulsing
    float pulseFov = 150.0f;         // Peak vertical FOV during pulse
    float riseTimeSeconds = 0.25f;   // Time to go base -> pulse
    float fallTimeSeconds = 0.35f;   // Time to go pulse -> base
    bool enabledAtStart = true;

    // State
    bool eventsRegistered = false;
    bool listeningEnabled = false;
    bool isPulsing = false;
    Phase phase = Phase::Rise;
    float phaseT = 0.0f;

    void RegisterEventListeners() {
        if (eventsRegistered) {
            return;
        }

        Events::Listen("ChronoActivated", [this](void*) {
            if (!listeningEnabled) return;
            TriggerPulse();
        });

        Events::Listen("ChronoDeactivated", [this](void*) {
            if (!listeningEnabled) return;
            TriggerPulse();
        });

        eventsRegistered = true;
    }

    void TriggerPulse() {
        if (!HasCamera()) {
            return;
        }

        // Restart pulse from base, even if already mid-pulse
        baseFov = std::clamp(baseFov <= 0.0f ? GetCameraFOV() : baseFov, 1.0f, 179.0f);
        pulseFov = std::clamp(pulseFov, 1.0f, 179.0f);
        riseTimeSeconds = std::max(0.01f, riseTimeSeconds);
        fallTimeSeconds = std::max(0.01f, fallTimeSeconds);

        isPulsing = true;
        phase = Phase::Rise;
        phaseT = 0.0f;
        SetCameraFOV(baseFov);
    }

    static float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    // SmoothStep for a gentle ease-in/out (0..1 -> 0..1)
    static float SmoothStep(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
};
