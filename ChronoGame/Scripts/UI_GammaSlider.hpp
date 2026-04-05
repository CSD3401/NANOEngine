#pragma once
#include "EngineAPI.hpp"
#include "UI_SaveSettings.hpp"
#include <ScriptSDK/UI.h>

/**
 * UI_GammaSlider
 * --------------
 * Attach to any entity; assign the UISlider used for gamma in Settings.
 * Drives the Script SDK `NE::Scripting::SetGamma(float)` API from the slider (normalized 0–1).
 *
 * Slider mapping (same as `SavedSettings::SliderNormToDisplayGamma`):
 *   - 1.0 → gamma 1.0 (SDK default)
 *   - 0.0 → darker (see `SavedSettings::kDefaultGammaAtSliderZero`)
 *
 * Optional `gammaAtSliderZero` tweaks the dark end; keep it aligned with
 * `SavedSettings::kDefaultGammaAtSliderZero` if you change the constant for save/load/pause apply.
 */
class UI_GammaSlider : public IScript {
public:
    UI_GammaSlider() {
        SCRIPT_GAMEOBJECT_REF(gammaSlider);
        SCRIPT_FIELD(gammaAtSliderZero, Float);
    }

    ~UI_GammaSlider() override = default;

    void Awake() override {}
    void Initialize(Entity /*entity*/) override {}

    void Start() override {
        ApplySavedOrSceneGamma(/*syncSliderFromSaved*/ true);
    }

    void Update(double /*dt*/) override {
        if (m_slider == 0)
            return;
        const float norm = CurrentSliderNorm();
        ApplyGammaFromNorm(norm);
    }

    void OnDestroy() override {}
    void OnEnable() override {
        // Re-opening Settings often enables the canvas without running `Start` again — keep thumb in sync.
        ApplySavedOrSceneGamma(/*syncSliderFromSaved*/ true);
    }
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UI_GammaSlider"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef gammaSlider;
    float gammaAtSliderZero = 0.5f; // match SavedSettings::kDefaultGammaAtSliderZero unless you tune range
    Entity m_slider = 0;

    void ApplyGammaFromNorm(float norm) {
        SavedSettings::NotifyLiveGammaNormalized(norm);
        NE::Scripting::SetGamma(SavedSettings::SliderNormToDisplayGamma(norm, gammaAtSliderZero));
    }

    void RefreshSliderEntity() {
        m_slider = gammaSlider.IsValid() ? gammaSlider.GetEntity() : 0;
    }

    float CurrentSliderNorm() const {
        if (m_slider == 0)
            return 1.0f;
        const float fb = SavedSettings::hasBeenSaved ? SavedSettings::gammaNormalized : 1.0f;
        return SavedSettings::ReadUISliderNormalized(m_slider, fb);
    }

    void ApplySavedOrSceneGamma(bool syncSliderFromSaved) {
        RefreshSliderEntity();
        if (m_slider == 0)
            return;
        if (SavedSettings::hasBeenSaved && syncSliderFromSaved)
            UI::SetSliderNormalizedValue(m_slider, SavedSettings::gammaNormalized);
        ApplyGammaFromNorm(CurrentSliderNorm());
    }
};
