#pragma once
#include "EngineAPI.hpp"

/*
* BackgroundAudio Script
* Plays background music and ambience tracks on loop
* Place this on an empty GameObject in your scene
*
* Controls:
* - Press 1: Play BGM_NIGHTSHIFT_HIGH
* - Press 2: Play BGM_NIGHTSHIFT_LOW
*/

class BackgroundAudio : public IScript {
public:
    BackgroundAudio() {
        SCRIPT_FIELD(bgmEventPath, String);
        SCRIPT_FIELD(ambienceEventPath, String);
        SCRIPT_FIELD(playOnStart, Bool);
        SCRIPT_FIELD(bgmVolume, Float);
        SCRIPT_FIELD(ambienceVolume, Float);
    }
    ~BackgroundAudio() override = default;

    // === Lifecycle Methods ===
    void Awake() override {}

    void Initialize(Entity entity) override {}

    void Start() override {
        // Set default values if not set in inspector
        if (bgmEventPath.empty()) {
            bgmEventPath = "event:/BGM_NIGHTSHIFT_HIGH";
        }
        if (ambienceEventPath.empty()) {
            ambienceEventPath = "event:/BGM_RABBIT";
        }

        // Initialize state
        isBGMPlaying = false;
        isAmbiencePlaying = false;
        currentBGM = "";

        // Play on start if enabled
        if (playOnStart) {
            PlayBGM();
            PlayAmbience();
        }
    }

    void Update(double deltaTime) override {
        // Press 1 to play BGM_NIGHTSHIFT_HIGH
        if (Input::IsKeyDown('1')) {
            SwitchBGM("event:/BGM_NIGHTSHIFT_HIGH");
        }

        // Press 2 to play BGM_NIGHTSHIFT_LOW
        if (Input::IsKeyDown('2')) {
            SwitchBGM("event:/BGM_NIGHTSHIFT_LOW");
        }
    }

    void OnDestroy() override {
        // Stop both tracks when destroyed
        StopBGM();
        StopAmbience();
    }

    // === Custom Methods ===
    void SwitchBGM(const std::string& newBGM) {
        // Stop current BGM if playing
        if (isBGMPlaying && !currentBGM.empty()) {
            StopAudio(currentBGM);
            LOG_INFO("Stopped: " << currentBGM);
        }

        // Play new BGM
        currentBGM = newBGM;
        PlayAudio(currentBGM);
        isBGMPlaying = true;
        LOG_INFO("Started: " << currentBGM);
    }

    void PlayBGM() {
        if (!isBGMPlaying && !bgmEventPath.empty()) {
            currentBGM = bgmEventPath;
            PlayAudio(bgmEventPath);
            isBGMPlaying = true;
            LOG_INFO("Started BGM: " << bgmEventPath);
        }
    }

    void StopBGM() {
        if (isBGMPlaying && !currentBGM.empty()) {
            StopAudio(currentBGM);
            isBGMPlaying = false;
            LOG_INFO("Stopped BGM");
        }
    }

    void PlayAmbience() {
        if (!isAmbiencePlaying && !ambienceEventPath.empty()) {
            PlayAudio(ambienceEventPath);
            isAmbiencePlaying = true;
            LOG_INFO("Started Ambience: " << ambienceEventPath);
        }
    }

    void StopAmbience() {
        if (isAmbiencePlaying && !ambienceEventPath.empty()) {
            StopAudio(ambienceEventPath);
            isAmbiencePlaying = false;
            LOG_INFO("Stopped Ambience");
        }
    }


    // === Optional Callbacks ===
    void OnEnable() override {
        // Resume when enabled
        if (playOnStart) {
            PlayBGM();
            PlayAmbience();
        }
    }

    void OnDisable() override {
        // Stop when disabled
        StopBGM();
        StopAmbience();
    }

    void OnValidate() override {}
    const char* GetTypeName() const override { return "BackgroundAudio"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    // === Inspector Fields ===
    std::string bgmEventPath;
    std::string ambienceEventPath;
    bool playOnStart = true;
    float bgmVolume = 1.0f;
    float ambienceVolume = 1.0f;

    // === Private State ===
    bool isBGMPlaying;
    bool isAmbiencePlaying;
    std::string currentBGM;  // Track which BGM is currently playing
};