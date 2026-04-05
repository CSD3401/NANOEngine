#pragma once
#include "EngineAPI.hpp"
#include <ScriptSDK/UI.h>
#include <algorithm>

/**
 * SavedSettings
 * -------------
 * Runtime-only settings cache that persists across scene switches while the game
 * is still running. It does NOT save to disk yet.
 *
 * Gamma: `gammaNormalized` is updated when the user moves the gamma slider (`NotifyLiveGammaNormalized`)
 * or uses Save / Reset. On DLL unload (editor stop or game exit) we call `SetGamma(1.0f)` so the host
 * display returns to default; the last `gammaNormalized` stays in memory for the next play session until
 * the process exits (statics reset).
 */
struct SavedSettings {
    static inline float masterVolume = 1.0f;
    static inline float bgmVolume = 1.0f;
    static inline float sfxVolume = 1.0f;
    static inline float ambienceVolume = 1.0f;
    static inline float gammaNormalized = 1.0f;
    static inline bool hasBeenSaved = false;
    /** True after gamma slider (or Save/Reset) has set `gammaNormalized` this run — used to re-apply across scenes without full Save. */
    static inline bool hasLiveGammaThisSession = false;

    static float Clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }

    /** Prefer over `UI::GetSliderNormalizedValue` while dragging — reads ECS `UISlider` directly. */
    static float ReadUISliderNormalized(Entity entity, float fallback) {
        if (entity == 0 || !NE::ECS::Query::HasUISlider(entity))
            return Clamp01(fallback);
        return Clamp01(NE::ECS::Query::GetUISlider(entity).GetNormalizedValue());
    }

    /** Engine gamma when UISlider normalized value is 0. At slider 1, gamma is 1.0 (`NE::Scripting::SetGamma` default). */
    static constexpr float kDefaultGammaAtSliderZero = 0.5f;

    /** Maps settings slider 0..1 to the value passed to `NE::Scripting::SetGamma`. */
    static float SliderNormToDisplayGamma(float norm, float gammaAtSliderZero = kDefaultGammaAtSliderZero) {
        norm = Clamp01(norm);
        gammaAtSliderZero = std::clamp(gammaAtSliderZero, 0.1f, 0.999f);
        return gammaAtSliderZero + norm * (1.0f - gammaAtSliderZero);
    }

    static void NotifyLiveGammaNormalized(float norm) {
        gammaNormalized = Clamp01(norm);
        hasLiveGammaThisSession = true;
    }

    static bool ShouldApplySessionGamma() {
        return hasBeenSaved || hasLiveGammaThisSession;
    }

    static void SaveAll(float master, float bgm, float sfx, float ambience, float gamma) {
        masterVolume = Clamp01(master);
        bgmVolume = Clamp01(bgm);
        sfxVolume = Clamp01(sfx);
        ambienceVolume = Clamp01(ambience);
        gammaNormalized = Clamp01(gamma);
        hasLiveGammaThisSession = true;
        hasBeenSaved = true;
    }
};

/**
 * UI_SaveSettings
 * ---------------
 * Attach to the Save button entity and assign all relevant settings sliders.
 * One button saves all current slider values into the runtime cache.
 */
class UI_SaveSettings : public IScript {
public:
    UI_SaveSettings() {
        SCRIPT_GAMEOBJECT_REF(masterSlider);
        SCRIPT_GAMEOBJECT_REF(bgmSlider);
        SCRIPT_GAMEOBJECT_REF(sfxSlider);
        SCRIPT_GAMEOBJECT_REF(ambienceSlider);
        SCRIPT_GAMEOBJECT_REF(gammaSlider);
    }

    ~UI_SaveSettings() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { m_buttonEntity = entity; }
    void Start() override {}

    void Update(double /*dt*/) override {
        if (m_buttonEntity == 0) return;
        if (!UI::WasButtonClicked(m_buttonEntity) || !UI::IsButtonInteractable(m_buttonEntity))
            return;

        SaveCurrentSettings();
        LOG_INFO("UI_SaveSettings: runtime settings saved.");
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UI_SaveSettings"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef masterSlider;
    GameObjectRef bgmSlider;
    GameObjectRef sfxSlider;
    GameObjectRef ambienceSlider;
    GameObjectRef gammaSlider;
    Entity m_buttonEntity = 0;

    static float ReadSlider(const GameObjectRef& ref, float fallback) {
        if (!ref.IsValid()) return SavedSettings::Clamp01(fallback);
        return SavedSettings::ReadUISliderNormalized(ref.GetEntity(), fallback);
    }

    void SaveCurrentSettings() {
        SavedSettings::SaveAll(
            ReadSlider(masterSlider, SavedSettings::masterVolume),
            ReadSlider(bgmSlider, SavedSettings::bgmVolume),
            ReadSlider(sfxSlider, SavedSettings::sfxVolume),
            ReadSlider(ambienceSlider, SavedSettings::ambienceVolume),
            ReadSlider(gammaSlider, SavedSettings::gammaNormalized));
    }
};