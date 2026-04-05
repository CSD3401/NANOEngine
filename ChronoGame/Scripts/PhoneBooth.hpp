#pragma once
#include "EngineAPI.hpp"

/*
 * PhoneBooth
 *
 * - PRESENT: when player enters range, plays a looping static noise.
 *            When player leaves range, stops the static.
 * - PAST:    when player enters range for the first time, plays a
 *            unique voice audio once (set via inspector field voiceAudioName).
 *
 * Listens to ChronoActivated (-> past) / ChronoDeactivated (-> present)
 * to know which timeline is active, same convention as Misc_TimeFogLighting.
 *
 * Setup:
 *   1. Attach this script to the phone booth entity
 *   2. Assign playerRef (drag Player entity)
 *   3. Set interactionDistance
 *   4. Set voiceAudioName  -- FMOD event name WITHOUT "event:/" prefix
 *                             e.g. "PHONEBOOTH_01_VOICE"
 *   5. Set staticAudioName -- FMOD looping static event name (default provided)
 *   6. Add a trigger collider OR rely on distance check in Update
 */
class PhoneBooth : public IScript {
public:
    PhoneBooth() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_FIELD(interactionDistance, Float);
        SCRIPT_FIELD(voiceAudioName, String);   // unique per booth, played once in past
        SCRIPT_FIELD(staticAudioName, String);   // looping static, played in present
    }

    ~PhoneBooth() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        playerInRange = false;
        voicePlayed = false;
        isInPast = false;   // start assuming present
        staticPlaying = false;

        if (!playerRef.IsValid())
            LOG_ERROR("PhoneBooth: playerRef not assigned!");

        if (interactionDistance <= 0.0f)
            interactionDistance = 3.0f;

        if (voiceAudioName.empty())
            LOG_WARNING("PhoneBooth: voiceAudioName is empty - no voice will play in past.");

        if (staticAudioName.empty())
            staticAudioName = "PHONEBOOTH_STATIC"; // sensible default

        if (playerRef.IsValid())
            playerEntity = playerRef.GetEntity();

        // Listen to timeline events - same convention as Misc_TimeFogLighting
        Events::Listen("ChronoActivated", [this](void*) {
            OnEnterPresent();
            });
        Events::Listen("ChronoDeactivated", [this](void*) {
            OnEnterPast();
            });
    }

    void Update(double deltaTime) override {
        (void)deltaTime;

        if (!playerRef.IsValid()) return;

        // Distance check
        Vec3 boothPos = TF_GetPosition();
        Vec3 playerPos = TF_GetPosition(playerEntity);
        Vec3 diff = playerPos - boothPos;
        float distance = diff.Length();

        bool wasInRange = playerInRange;
        playerInRange = (distance <= interactionDistance);

        // Player just entered range
        if (playerInRange && !wasInRange) {
            OnPlayerEnter();
        }
        // Player just left range
        else if (!playerInRange && wasInRange) {
            OnPlayerExit();
        }
    }

    void OnDestroy() override {
        StopStatic();
    }

    void OnEnable()   override {}
    void OnDisable()  override { StopStatic(); }
    void OnValidate() override {}

    const char* GetTypeName() const override { return "PhoneBooth"; }

    void OnCollisionEnter(Entity o) override { (void)o; }
    void OnCollisionExit(Entity o)  override { (void)o; }
    void OnCollisionStay(Entity o)  override { (void)o; }
    void OnTriggerEnter(Entity o)   override { (void)o; }
    void OnTriggerExit(Entity o)    override { (void)o; }
    void OnTriggerStay(Entity o)    override { (void)o; }

private:
    // Inspector
    GameObjectRef playerRef;
    float         interactionDistance = 3.0f;
    std::string   voiceAudioName = "";    // e.g. "PHONEBOOTH_01_VOICE"
    std::string   staticAudioName = "PHONEBOOTH_STATIC";

    // Runtime
    Entity playerEntity;
    bool   playerInRange = false;
    bool   voicePlayed = false;
    bool   isInPast = false;
    bool   staticPlaying = false;

    // ----------------------------------------------------------

    void OnPlayerEnter() {
        if (isInPast) {
            // Past: play voice once per visit to this timeline
            if (!voicePlayed && !voiceAudioName.empty()) {
                PlayAudio("event:/" + voiceAudioName);
                voicePlayed = true;
                LOG_DEBUG("PhoneBooth: Playing voice: " + voiceAudioName);
            }
        }
        else {
            // Present: start looping static
            StartStatic();
        }
    }

    void OnPlayerExit() {
        if (!isInPast) {
            StopStatic();
        }
        // Voice in past just plays once and ends naturally - no stop needed
    }

    void OnEnterPast() {
        isInPast = true;
        // If player is already inside the booth range when timeline switches, stop static
        if (playerInRange) {
            StopStatic();
            // Play voice if not yet played this past-visit
            if (!voicePlayed && !voiceAudioName.empty()) {
                PlayAudio("event:/" + voiceAudioName);
                voicePlayed = true;
                LOG_DEBUG("PhoneBooth: Timeline switched to past - playing voice: " + voiceAudioName);
            }
        }
    }

    void OnEnterPresent() {
        // Stop voice if it is still playing when switching back to present
        if (!voiceAudioName.empty())
            StopAudio("event:/" + voiceAudioName);
        isInPast = false;
        // If player is inside range when switching back to present, start static
        if (playerInRange) {
            StartStatic();
        }
    }

    void StartStatic() {
        if (!staticPlaying) {
            PlayAudio("event:/" + staticAudioName);
            staticPlaying = true;
            LOG_DEBUG("PhoneBooth: Static started.");
        }
    }

    void StopStatic() {
        if (staticPlaying) {
            StopAudio("event:/" + staticAudioName);
            staticPlaying = false;
            LOG_DEBUG("PhoneBooth: Static stopped.");
        }
    }
};