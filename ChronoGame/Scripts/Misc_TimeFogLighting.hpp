#pragma once
#include <algorithm>
#include <string>
#include "EngineAPI.hpp"

/*
    Misc_TimeFogLighting
    -------------------
    Listens to the time-switch events:
      - ChronoActivated   -> PAST / NORMAL
      - ChronoDeactivated -> PRESENT

    Per request:
      - PRESENT : thicker fog + redder lighting
      - PAST    : restore to the configured normal fog + lighting

    Color input note:
      - Inspector color fields in this script are treated as standard RGB 0-255 values.
      - The script converts them to 0-1 internally before sending them to RenderSettings.

    Why this version fixes scene-switch issues:
      - The script now exposes NORMAL / PAST fog and ambient settings in the Inspector.
      - Restore no longer depends only on whatever RenderSettings happened to be at runtime.
      - If a new scene inherits the red present tint, this script can force the scene back to
        its configured normal look as soon as it starts (or when ChronoActivated fires).

    Usage:
      1) Add this script to an always-alive entity in each scene that needs this behavior.
      2) Fill in the normalFog / normalAmbient fields to match that scene's intended default look.
      3) Fill in the presentFog / presentAmbient fields for the "present" look.
      4) Usually keep:
            applyStateOnStart = true
            startInPresent    = false
         so the scene starts by forcing the normal/past settings immediately.
*/

class Misc_TimeFogLighting : public IScript {
public:
    Misc_TimeFogLighting() {
        // Toggles
        SCRIPT_FIELD(affectFog, Bool);
        SCRIPT_FIELD(affectAmbient, Bool);
        SCRIPT_FIELD(applyStateOnStart, Bool);
        SCRIPT_FIELD(startInPresent, Bool);
        SCRIPT_FIELD(captureCurrentAsNormalOnStart, Bool);

        // NORMAL / PAST fog
        SCRIPT_FIELD(normalFogEnabled, Bool);
        SCRIPT_ENUM_FIELD(normalFogMode, "Linear", "Exponential", "ExponentialSquared");
        SCRIPT_FIELD(normalFogColor, Vec3);
        SCRIPT_FIELD(normalFogDensity, Float);
        SCRIPT_FIELD(normalFogStart, Float);
        SCRIPT_FIELD(normalFogEnd, Float);

        // NORMAL / PAST ambient
        SCRIPT_ENUM_FIELD(normalEnvSource, "Skybox", "Gradient", "Color");
        SCRIPT_FIELD(normalAmbientColor, Vec3);
        SCRIPT_FIELD(normalAmbientIntensity, Float);

        // PRESENT fog look
        SCRIPT_FIELD(presentFogEnabled, Bool);
        SCRIPT_FIELD(presentFogColor, Vec3);
        SCRIPT_FIELD(presentFogDensity, Float);    // Used if mode is Exponential / ExponentialSquared
        SCRIPT_FIELD(presentFogStart, Float);      // Used if mode is Linear
        SCRIPT_FIELD(presentFogEnd, Float);        // Used if mode is Linear

        // PRESENT ambient look
        SCRIPT_FIELD(presentAmbientColor, Vec3);
        SCRIPT_FIELD(presentAmbientIntensity, Float);
    }

    ~Misc_TimeFogLighting() override = default;

    void Awake() override {
        listeningEnabled = true;
        RegisterEventListeners();
    }

    void Initialize(Entity entity) override { (void)entity; }

    void Start() override {
        if (captureCurrentAsNormalOnStart) {
            CaptureCurrentIntoNormalFields();
        }

        if (applyStateOnStart) {
            if (startInPresent) {
                ApplyPresent();
            } else {
                ApplyPast();
            }
        }
    }

    void Update(double) override {}

    void OnDestroy() override {
        listeningEnabled = false;
        RestoreConfiguredNormal();
    }

    void OnEnable() override { listeningEnabled = true; }

    void OnDisable() override {
        listeningEnabled = false;
        RestoreConfiguredNormal();
    }

    void OnValidate() override {
        // Safety clamps for inspector-driven values
        normalFogColor          = ClampColor255(normalFogColor);
        normalAmbientColor      = ClampColor255(normalAmbientColor);
        normalFogDensity        = std::max(0.0f, normalFogDensity);
        normalFogStart          = std::max(0.0f, normalFogStart);
        normalFogEnd            = std::max(normalFogStart + 0.01f, normalFogEnd);
        normalAmbientIntensity  = std::max(0.0f, normalAmbientIntensity);

        presentFogColor         = ClampColor255(presentFogColor);
        presentAmbientColor     = ClampColor255(presentAmbientColor);
        presentFogDensity       = std::max(0.0f, presentFogDensity);
        presentFogStart         = std::max(0.0f, presentFogStart);
        presentFogEnd           = std::max(presentFogStart + 0.01f, presentFogEnd);
        presentAmbientIntensity = std::max(0.0f, presentAmbientIntensity);
    }

    const char* GetTypeName() const override { return "Misc_TimeFogLighting"; }

    void OnCollisionEnter(Entity) override {}
    void OnCollisionExit(Entity) override {}
    void OnCollisionStay(Entity) override {}
    void OnTriggerEnter(Entity) override {}
    void OnTriggerExit(Entity) override {}
    void OnTriggerStay(Entity) override {}

private:
    // ===== Inspector fields =====
    bool affectFog = true;
    bool affectAmbient = true;

    // If true, applies either present or past immediately at Start().
    bool applyStateOnStart = true;
    bool startInPresent = false; // Your game treats ChronoActivated as "past".

    // Optional convenience:
    // OFF by default because if a scene starts while globals are already tinted,
    // capturing current settings would lock in the wrong normal state.
    bool captureCurrentAsNormalOnStart = false;

    // NORMAL / PAST fog settings
    bool normalFogEnabled = false;
    RenderSettings::FogMode normalFogMode = RenderSettings::FogMode::Exponential;
    Vec3 normalFogColor = Vec3(128.0f, 128.0f, 128.0f);
    float normalFogDensity = 0.01f;
    float normalFogStart = 0.0f;
    float normalFogEnd = 50.0f;

    // NORMAL / PAST ambient settings
    RenderSettings::EnvSource normalEnvSource = RenderSettings::EnvSource::Color;
    Vec3 normalAmbientColor = Vec3(255.0f, 255.0f, 255.0f);
    float normalAmbientIntensity = 1.0f;

    // PRESENT fog settings (thicker = higher density, closer end distance, etc.)
    bool  presentFogEnabled = true;
    Vec3  presentFogColor   = Vec3(89.0f, 13.0f, 13.0f);
    float presentFogDensity = 0.055f;
    float presentFogStart   = 0.0f;
    float presentFogEnd     = 18.0f;

    // PRESENT ambient settings (redder)
    Vec3  presentAmbientColor = Vec3(115.0f, 26.0f, 26.0f);
    float presentAmbientIntensity = 1.0f;

    // ===== Runtime state =====
    bool eventsRegistered = false;
    bool listeningEnabled = false;

    static float ClampChannel255(float value) {
        return std::clamp(value, 0.0f, 255.0f);
    }

    static Vec3 ClampColor255(const Vec3& color) {
        return Vec3(
            ClampChannel255(color.x),
            ClampChannel255(color.y),
            ClampChannel255(color.z)
        );
    }

    static Vec3 NormalizeColor255(const Vec3& color255) {
        Vec3 clamped = ClampColor255(color255);
        return clamped / 255.0f;
    }

    static Vec3 DenormalizeColor255(const Vec3& normalizedColor) {
        Vec3 clamped(
            std::clamp(normalizedColor.x, 0.0f, 1.0f),
            std::clamp(normalizedColor.y, 0.0f, 1.0f),
            std::clamp(normalizedColor.z, 0.0f, 1.0f)
        );
        return clamped * 255.0f;
    }

    void RegisterEventListeners() {
        if (eventsRegistered) return;

        // PAST / NORMAL
        Events::Listen("ChronoActivated", [this](void*) {
            if (!listeningEnabled) return;
            ApplyPast();
        });

        // PRESENT
        Events::Listen("ChronoDeactivated", [this](void*) {
            if (!listeningEnabled) return;
            ApplyPresent();
        });

        eventsRegistered = true;
    }

    void CaptureCurrentIntoNormalFields() {
        normalFogEnabled       = RenderSettings::IsFogEnabled();
        normalFogMode          = RenderSettings::GetFogMode();
        normalFogColor         = DenormalizeColor255(RenderSettings::GetFogColor());
        normalFogStart         = RenderSettings::GetFogStart();
        normalFogEnd           = RenderSettings::GetFogEnd();
        normalFogDensity       = RenderSettings::GetFogDensity();

        normalEnvSource        = RenderSettings::GetEnvSource();
        normalAmbientColor     = DenormalizeColor255(RenderSettings::GetAmbientColor());
        normalAmbientIntensity = RenderSettings::GetAmbientIntensity();

        LOG_INFO("Misc_TimeFogLighting: captured current render settings into NORMAL inspector-backed values");
    }

    void RestoreConfiguredNormal() {
        // Fog
        if (affectFog) {
            RenderSettings::SetFogEnabled(normalFogEnabled);
            RenderSettings::SetFogMode(normalFogMode);
            RenderSettings::SetFogColor(NormalizeColor255(normalFogColor));
            RenderSettings::SetFogStart(normalFogStart);
            RenderSettings::SetFogEnd(normalFogEnd);
            RenderSettings::SetFogDensity(normalFogDensity);
        }

        // Ambient
        if (affectAmbient) {
            RenderSettings::SetEnvSource(normalEnvSource);
            RenderSettings::SetAmbientColor(NormalizeColor255(normalAmbientColor));
            RenderSettings::SetAmbientIntensity(normalAmbientIntensity);
        }

        LOG_INFO("Misc_TimeFogLighting: restored configured NORMAL render settings");
    }

    void ApplyPresent() {
        // ===== Fog =====
        if (affectFog) {
            RenderSettings::SetFogEnabled(presentFogEnabled);
            RenderSettings::SetFogColor(NormalizeColor255(presentFogColor));

            // Keep the scene's current fog mode, but push parameters for the present look.
            RenderSettings::FogMode mode = RenderSettings::GetFogMode();

            if (mode == RenderSettings::FogMode::Linear) {
                float start = std::max(0.0f, presentFogStart);
                float end   = std::max(start + 0.01f, presentFogEnd);
                RenderSettings::SetFogStart(start);
                RenderSettings::SetFogEnd(end);
            } else {
                RenderSettings::SetFogDensity(std::max(0.0f, presentFogDensity));
            }
        }

        // ===== Ambient =====
        if (affectAmbient) {
            RenderSettings::SetAmbientColor(NormalizeColor255(presentAmbientColor));
            RenderSettings::SetAmbientIntensity(std::max(0.0f, presentAmbientIntensity));
        }

        LOG_INFO("Misc_TimeFogLighting: applied PRESENT look (thicker fog + red lighting)");
    }

    void ApplyPast() {
        RestoreConfiguredNormal();
        LOG_INFO("Misc_TimeFogLighting: restored PAST / NORMAL look");
    }
};
