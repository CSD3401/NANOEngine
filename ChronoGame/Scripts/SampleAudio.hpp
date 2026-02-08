#pragma once
#include "EngineAPI.hpp"

/**
 * VolumeControlExample
 *
 * Example script showing how to control bus volumes
 *
 * Controls:
 * - Press 0: Decrease Master volume by 0.1
 * - Press 1: Increase Master volume by 0.1
 * - Press 2: Decrease BGM volume by 0.1
 * - Press 3: Increase BGM volume by 0.1
 * - Press 4: Decrease SFX volume by 0.1
 * - Press 5: Increase SFX volume by 0.1
 * - Press 6: Decrease Ambience volume by 0.1
 * - Press 7: Increase Ambience volume by 0.1
 */
class VolumeControlExample : public IScript {
public:
    VolumeControlExample() {
        SCRIPT_FIELD(masterVolume, Float);
        SCRIPT_FIELD(bgmVolume, Float);
        SCRIPT_FIELD(sfxVolume, Float);
        SCRIPT_FIELD(ambienceVolume, Float);
    }
    ~VolumeControlExample() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        // Set default volumes
        masterVolume = 1.0f;
        bgmVolume = 0.6f;
        sfxVolume = 0.8f;
        ambienceVolume = 0.4f;

        // Apply volumes
        SetMasterVolume(masterVolume);
        SetBGMVolume(bgmVolume);
        SetSFXVolume(sfxVolume);
        SetAmbienceVolume(ambienceVolume);

        LOG_INFO("=== Volume Control Example Started ===");
        PrintVolumes();

        PlayAudio("event:/BGM_RABBIT"); // ambience bus
        //PlayAudio("event:/BGM_NIGHTSHIFT_HIGH"); // BGM bus
    }

    void Update(double deltaTime) override {
        // Master Volume Controls (0, 1)
        if (Input::WasKeyPressed('0')) {
            masterVolume = GetMasterVolume() - 0.1f;
            masterVolume = std::max(0.0f, std::min(1.0f, masterVolume));
            SetMasterVolume(masterVolume);
            LOG_INFO("Master: " << masterVolume);
        }

        if (Input::WasKeyPressed('1')) {
            masterVolume = GetMasterVolume() + 0.1f;
            masterVolume = std::max(0.0f, std::min(1.0f, masterVolume));
            SetMasterVolume(masterVolume);
            LOG_INFO("Master: " << masterVolume);
        }

        // BGM Volume Controls (2, 3)
        if (Input::WasKeyPressed('2')) {
            bgmVolume = GetBGMVolume() - 0.1f;
            bgmVolume = std::max(0.0f, std::min(1.0f, bgmVolume));
            SetBGMVolume(bgmVolume);
            LOG_INFO("BGM: " << bgmVolume);
        }

        if (Input::WasKeyPressed('3')) {
            bgmVolume = GetBGMVolume() + 0.1f;
            bgmVolume = std::max(0.0f, std::min(1.0f, bgmVolume));
            SetBGMVolume(bgmVolume);
            LOG_INFO("BGM: " << bgmVolume);
        }

        // SFX Volume Controls (4, 5)
        if (Input::WasKeyPressed('4')) {
            sfxVolume = GetSFXVolume() - 0.1f;
            sfxVolume = std::max(0.0f, std::min(1.0f, sfxVolume));
            SetSFXVolume(sfxVolume);
            LOG_INFO("SFX: " << sfxVolume);
        }

        if (Input::WasKeyPressed('5')) {
            sfxVolume = GetSFXVolume() + 0.1f;
            sfxVolume = std::max(0.0f, std::min(1.0f, sfxVolume));
            SetSFXVolume(sfxVolume);
            LOG_INFO("SFX: " << sfxVolume);
        }

        // Ambience Volume Controls (6, 7)
        if (Input::WasKeyPressed('6')) {
            ambienceVolume = GetAmbienceVolume() - 0.1f;
            ambienceVolume = std::max(0.0f, std::min(1.0f, ambienceVolume));
            SetAmbienceVolume(ambienceVolume);
            LOG_INFO("Ambience: " << ambienceVolume);
        }

        if (Input::WasKeyPressed('7')) {
            ambienceVolume = GetAmbienceVolume() + 0.1f;
            ambienceVolume = std::max(0.0f, std::min(1.0f, ambienceVolume));
            SetAmbienceVolume(ambienceVolume);
            LOG_INFO("Ambience: " << ambienceVolume);
        }


        if (Input::WasKeyPressed('8')) {
            PlayAudio("event:/COLOR_CLICK");
        }

        // Print all volumes (Press 9)
        if (Input::WasKeyPressed('9')) {
            PrintVolumes();
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "VolumeControlExample"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    void PrintVolumes() {
        LOG_INFO("=== Current Bus Volumes ===");
        LOG_INFO("Master:   " << GetMasterVolume());
        LOG_INFO("BGM:      " << GetBGMVolume());
        LOG_INFO("SFX:      " << GetSFXVolume());
        LOG_INFO("Ambience: " << GetAmbienceVolume());
        LOG_INFO("===========================");
    }

    float masterVolume = 0.5f;
    float bgmVolume = 0.5f;
    float sfxVolume = 0.5f;
    float ambienceVolume = 0.5f;
};