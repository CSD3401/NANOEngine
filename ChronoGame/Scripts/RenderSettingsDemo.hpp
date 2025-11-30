#pragma once
#include "EngineAPI.hpp"

/**
 * @file RenderSettingsDemo.hpp
 * @brief Demonstration script for the RenderSettings API
 *
 * This script demonstrates how to control render settings from scripts,
 * including environment lighting and fog settings.
 *
 * Controls:
 * - Press '1' to cycle through environment sources (Skybox, Gradient, Color)
 * - Press '2' to toggle fog on/off
 * - Press '3' to cycle through fog modes (Linear, Exponential, ExponentialSquared)
 * - Press '4' to animate fog density
 * - Press '5' to cycle through preset ambient colors
 */
class RenderSettingsDemo : public IScript {
public:
    RenderSettingsDemo() {
        // Register exposed fields for the editor
        SCRIPT_FIELD(enableFogAnimation, Bool);
        SCRIPT_FIELD(fogAnimationSpeed, Float);
        SCRIPT_FIELD(ambientIntensity, Float);
        SCRIPT_FIELD(fogStartDistance, Float);
        SCRIPT_FIELD(fogEndDistance, Float);
    }

    void Initialize(Entity entity) override {
        LOG_INFO("RenderSettingsDemo initialized!");
        LOG_INFO("Controls:");
        LOG_INFO("  [1] Cycle environment source");
        LOG_INFO("  [2] Toggle fog");
        LOG_INFO("  [3] Cycle fog mode");
        LOG_INFO("  [4] Toggle fog animation");
        LOG_INFO("  [5] Cycle ambient colors");

        // Initialize render settings with current values
        RenderSettings::SetAmbientIntensity(ambientIntensity);
        RenderSettings::SetFogStart(fogStartDistance);
        RenderSettings::SetFogEnd(fogEndDistance);
    }

    void Update(double deltaTime) override {
        // Control 1: Cycle environment source
        if (Input::WasKeyPressed('1')) {
            CycleEnvironmentSource();
        }

        // Control 2: Toggle fog
        if (Input::WasKeyPressed('2')) {
            ToggleFog();
        }

        // Control 3: Cycle fog mode
        if (Input::WasKeyPressed('3')) {
            CycleFogMode();
        }

        // Control 4: Toggle fog animation
        if (Input::WasKeyPressed('4')) {
            enableFogAnimation = !enableFogAnimation;
            LOG_INFO("Fog animation: "<< enableFogAnimation);
        }

        // Control 5: Cycle ambient colors
        if (Input::WasKeyPressed('5')) {
            CycleAmbientColor();
        }

        // Animate fog if enabled
        if (enableFogAnimation && RenderSettings::IsFogEnabled()) {
            AnimateFog(static_cast<float>(deltaTime));
        }
    }

    const char* GetTypeName() const override {
        return "RenderSettingsDemo";
    }

    // Required interface methods (not used in this demo)
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    void CycleEnvironmentSource() {
        currentEnvSource = (currentEnvSource + 1) % 3;

        switch (currentEnvSource) {
            case 0:
                RenderSettings::SetEnvSource(RenderSettings::EnvSource::Skybox);
                LOG_INFO("Environment Source: SKYBOX");
                break;
            case 1:
                RenderSettings::SetEnvSource(RenderSettings::EnvSource::Gradient);
                LOG_INFO("Environment Source: GRADIENT");
                break;
            case 2:
                RenderSettings::SetEnvSource(RenderSettings::EnvSource::Color);
                LOG_INFO("Environment Source: COLOR");
                break;
        }
    }

    void ToggleFog() {
        bool isEnabled = RenderSettings::IsFogEnabled();
        RenderSettings::SetFogEnabled(!isEnabled);
        LOG_INFO("Fog: " << isEnabled);
    }

    void CycleFogMode() {
        currentFogMode = (currentFogMode + 1) % 3;

        switch (currentFogMode) {
            case 0:
                RenderSettings::SetFogMode(RenderSettings::FogMode::Linear);
                LOG_INFO("Fog Mode: LINEAR");
                LOG_INFO("  Using fog start: "<< fogStartDistance<< ", fog end: "<< fogEndDistance);
                break;
            case 1:
                RenderSettings::SetFogMode(RenderSettings::FogMode::Exponential);
                LOG_INFO("Fog Mode: EXPONENTIAL");
                LOG_INFO("  Using fog density: "<< RenderSettings::GetFogDensity());
                break;
            case 2:
                RenderSettings::SetFogMode(RenderSettings::FogMode::ExponentialSquared);
                LOG_INFO("Fog Mode: EXPONENTIAL SQUARED");
                LOG_INFO("  Using fog density: "<< RenderSettings::GetFogDensity());
                break;
        }
    }

    void CycleAmbientColor() {
        currentAmbientPreset = (currentAmbientPreset + 1) % 5;

        switch (currentAmbientPreset) {
            case 0:
                RenderSettings::SetAmbientColor(1.0f, 1.0f, 1.0f); // White
                LOG_INFO("Ambient Color: WHITE");
                break;
            case 1:
                RenderSettings::SetAmbientColor(1.0f, 0.9f, 0.7f); // Warm
                LOG_INFO("Ambient Color: WARM (Sunset)");
                break;
            case 2:
                RenderSettings::SetAmbientColor(0.4f, 0.5f, 0.7f); // Cool
                LOG_INFO("Ambient Color: COOL (Moonlight)");
                break;
            case 3:
                RenderSettings::SetAmbientColor(0.3f, 0.6f, 0.3f); // Green
                LOG_INFO("Ambient Color: GREEN (Forest)");
                break;
            case 4:
                RenderSettings::SetAmbientColor(0.6f, 0.4f, 0.6f); // Purple
                LOG_INFO("Ambient Color: PURPLE (Mystical)");
                break;
        }
    }

    void AnimateFog(float deltaTime) {
        m_fogAnimationTime += deltaTime * fogAnimationSpeed;

        // Animate fog density for exponential modes
        auto mode = RenderSettings::GetFogMode();
        if (mode == RenderSettings::FogMode::Exponential ||
            mode == RenderSettings::FogMode::ExponentialSquared) {
            float density = 0.05f + std::sin(m_fogAnimationTime) * 0.03f;
            RenderSettings::SetFogDensity(density);
        }

        // Animate fog color
        float r = 0.5f + std::sin(m_fogAnimationTime) * 0.3f;
        float g = 0.5f + std::sin(m_fogAnimationTime * 1.3f) * 0.3f;
        float b = 0.5f + std::sin(m_fogAnimationTime * 1.7f) * 0.3f;
        RenderSettings::SetFogColor(r, g, b);
    }

    // === Exposed Fields ===
    bool enableFogAnimation = false;
    float fogAnimationSpeed = 1.0f;
    float ambientIntensity = 1.0f;
    float fogStartDistance = 10.0f;
    float fogEndDistance = 100.0f;

    // === Internal State ===
    int currentEnvSource = 0;
    int currentFogMode = 0;
    int currentAmbientPreset = 0;
    float m_fogAnimationTime = 0.0f;
};
