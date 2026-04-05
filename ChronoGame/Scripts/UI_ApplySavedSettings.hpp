#pragma once
#include "EngineAPI.hpp"
#include "UI_SaveSettings.hpp"

/**
 * UI_ApplySavedSettings
 * ---------------------
 * Attach to an always-active entity in a scene to re-apply cached runtime
 * settings after scene load.
 *
 * Reapplies audio buses and `NE::Scripting::SetGamma` from the cache (see UI_SaveSettings).
 */
class UI_ApplySavedSettings : public IScript {
public:
    UI_ApplySavedSettings() = default;
    ~UI_ApplySavedSettings() override = default;

    void Awake() override {}
    void Initialize(Entity /*entity*/) override {}

    void Start() override {
        ApplySavedValues();
    }

    void Update(double /*dt*/) override {}
    void OnDestroy() override {}
    void OnEnable() override { ApplySavedValues(); }
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UI_ApplySavedSettings"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    void ApplySavedValues() {
        if (SavedSettings::hasBeenSaved) {
            SetMasterVolume(SavedSettings::masterVolume);
            SetBGMVolume(SavedSettings::bgmVolume);
            SetSFXVolume(SavedSettings::sfxVolume);
            SetAmbienceVolume(SavedSettings::ambienceVolume);
        }
        if (SavedSettings::ShouldApplySessionGamma()) {
            NE::Scripting::SetGamma(
                SavedSettings::SliderNormToDisplayGamma(SavedSettings::gammaNormalized));
        }
    }
};
