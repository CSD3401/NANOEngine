#pragma once
#include "EngineAPI.hpp"

/**
 * PhoneBooth
 *
 * Plays sequential voiceover audio when player is nearby.
 * First interaction plays VOICEOVER1, subsequent E presses play VOICEOVER2-5.
 *
 * Setup:
 * 1. Add this script to your phone booth entity
 * 2. Assign playerRef (drag Player entity)
 * 3. Add a trigger collider to the phone booth
 * 4. Set interactionDistance
 * 5. Walk near phone booth to trigger first voiceover
 * 6. Press E to play next voiceovers
 */
class PhoneBooth : public IScript {
public:
    PhoneBooth() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_FIELD(interactionDistance, Float);
    }

    ~PhoneBooth() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        currentVoiceoverIndex = 1;  // Start with VOICEOVER1
        hasPlayedInitial = false;
        playerInRange = false;

        if (!playerRef.IsValid()) {
            LOG_ERROR("Interactable_Gate: playerRef not assigned!");
        }

        if (interactionDistance <= 0.0f) {
            interactionDistance = 3.0f; // Default 3 units
        }

        if (playerRef.IsValid()) {
            playerEntity = playerRef.GetEntity();
        }
    }

    void Update(double deltaTime) override {
        (void)deltaTime;

        // Check distance to player
        Vec3 boothPos = TF_GetPosition();
        Vec3 playerPos = TF_GetPosition(playerEntity);

        // Calculate distance
        Vec3 delta = playerPos - boothPos;
        float distance = delta.Length();

        bool wasInRange = playerInRange;
        playerInRange = distance <= interactionDistance;

        // Just entered range - play first voiceover automatically
        if (playerInRange && !wasInRange && !hasPlayedInitial) {
            PlayCurrentVoiceover();
            hasPlayedInitial = true;
        }

        // Press E to play next voiceover
        if (playerInRange && hasPlayedInitial && Input::WasKeyPressed('E')) {
            if (currentVoiceoverIndex < 5) {
                currentVoiceoverIndex++;
                PlayCurrentVoiceover();
            } else {
                LOG_DEBUG("PhoneBooth: All voiceovers played");
            }
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "PhoneBooth"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    void PlayCurrentVoiceover() {
        std::string eventPath = "event:/VOICEOVER" + std::to_string(currentVoiceoverIndex);
        PlayAudio(eventPath);
        LOG_INFO("PhoneBooth: Playing " << eventPath);
    }

    // Inspector fields
    GameObjectRef playerRef;
    float interactionDistance = 3.0f;

    // Private state
    Entity playerEntity;
    int currentVoiceoverIndex;
    bool hasPlayedInitial;
    bool playerInRange;
};
