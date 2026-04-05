#pragma once
#include "EngineAPI.hpp"
#include "UI_SaveSettings.hpp"
#include <ScriptSDK/UI.h>

class UI_RestorePauseSettings : public IScript {
public:
    UI_RestorePauseSettings() {
        SCRIPT_GAMEOBJECT_REF(masterSlider);
        SCRIPT_GAMEOBJECT_REF(bgmSlider);
        SCRIPT_GAMEOBJECT_REF(sfxSlider);
        SCRIPT_GAMEOBJECT_REF(ambienceSlider);
        SCRIPT_GAMEOBJECT_REF(gammaSlider);
    }

    ~UI_RestorePauseSettings() override = default;

    void Awake() override {}
    void Initialize(Entity /*entity*/) override {}
    void Start() override { Apply(); }
    void OnEnable() override { Apply(); }
    void Update(double /*dt*/) override {}
    void OnDestroy() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UI_RestorePauseSettings"; }

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

    void Apply() {
        if (SavedSettings::hasBeenSaved) {
            if (masterSlider.IsValid())
                UI::SetSliderNormalizedValue(masterSlider.GetEntity(), SavedSettings::masterVolume);
            if (bgmSlider.IsValid())
                UI::SetSliderNormalizedValue(bgmSlider.GetEntity(), SavedSettings::bgmVolume);
            if (sfxSlider.IsValid())
                UI::SetSliderNormalizedValue(sfxSlider.GetEntity(), SavedSettings::sfxVolume);
            if (ambienceSlider.IsValid())
                UI::SetSliderNormalizedValue(ambienceSlider.GetEntity(), SavedSettings::ambienceVolume);

            SetMasterVolume(SavedSettings::masterVolume);
            SetBGMVolume(SavedSettings::bgmVolume);
            SetSFXVolume(SavedSettings::sfxVolume);
            SetAmbienceVolume(SavedSettings::ambienceVolume);
        }

        if (!SavedSettings::ShouldApplySessionGamma())
            return;

        if (gammaSlider.IsValid())
            UI::SetSliderNormalizedValue(gammaSlider.GetEntity(), SavedSettings::gammaNormalized);
        NE::Scripting::SetGamma(
            SavedSettings::SliderNormToDisplayGamma(SavedSettings::gammaNormalized));
    }
};
