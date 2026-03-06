#pragma once
#include "EngineAPI.hpp"

#define GLFW_MOUSE_BUTTON_LEFT 0

/*
 * DialogueMarker
 *
 * Place this on an empty GameObject with a trigger collider.
 * When the player walks into the collider for the first time,
 * the shared UI panel activates and shows dialogueText.
 * A left-click dismisses it.
 *
 * If another DialogueMarker is already playing audio, it will be
 * stopped automatically when this one triggers.
 *
 * Setup:
 *   1. Assign playerRef     -- drag the Player entity in
 *   2. Assign dialogueUI    -- drag the shared UIText/panel GameObject in
 *   3. Set dialogueText     -- the line(s) to display
 *   4. Set audioName        -- FMOD event path (optional)
 */
class DialogueMarker : public IScript {
public:
    DialogueMarker() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(dialogueUI);
        SCRIPT_FIELD(dialogueText, String);
        SCRIPT_FIELD(audioName, String);
    }

    ~DialogueMarker() override = default;

    void Awake()            override {}
    void Initialize(Entity) override {}
    void OnDestroy()        override {}
    void OnEnable()         override {}
    void OnDisable()        override {}
    void OnValidate()       override {}

    const char* GetTypeName() const override { return "DialogueMarker"; }

    void Start() override {
        if (!playerRef.IsValid())
            LOG_ERROR("DialogueMarker: playerRef not assigned!");

        if (!dialogueUI.IsValid())
            LOG_ERROR("DialogueMarker: dialogueUI not assigned!");

        if (dialogueText.empty())
            LOG_WARNING("DialogueMarker: dialogueText is empty.");

        triggered = false;
        dismissing = false;
        ignoreNextClick = false;
    }

    void Update(double) override {
        if (!dismissing) return;

        if (Input::WasKeyPressed('0')) {
            if (ignoreNextClick) {
                ignoreNextClick = false;
                return;
            }
            HideDialogue();
            dismissing = false;
        }
    }

    // Collision callbacks
    void OnCollisionEnter(Entity o) override { (void)o; }
    void OnCollisionExit(Entity o)  override { (void)o; }
    void OnCollisionStay(Entity o)  override { (void)o; }

    void OnTriggerEnter(Entity other) override {
        if (triggered) return;
        if (!playerRef.IsValid()) return;
        if (other != playerRef.GetEntity()) return;

        triggered = true;
        dismissing = true;
        ignoreNextClick = true;
        ShowDialogue();
    }

    void OnTriggerExit(Entity o)  override { (void)o; }
    void OnTriggerStay(Entity o)  override { (void)o; }

private:
    // Inspector
    GameObjectRef playerRef;
    GameObjectRef dialogueUI;
    std::string   dialogueText = "";
    std::string   audioName = "";

    // Runtime
    bool triggered = false;
    bool dismissing = false;
    bool ignoreNextClick = false;

    // Static: tracks whichever marker is currently playing audio
    static DialogueMarker* s_activeMarker;

    void ShowDialogue() {
        // Stop the previous marker's audio if one is playing
        if (s_activeMarker != nullptr && s_activeMarker != this) {
            s_activeMarker->StopCurrentAudio();
        }
        s_activeMarker = this;

        if (!dialogueUI.IsValid()) return;
        NE::Scripting::SetUIText(dialogueUI.GetEntity(), dialogueText.c_str());
        SetActive(true, dialogueUI.GetEntity());

        if (!audioName.empty())
            PlayAudio("event:/" + audioName);
    }

    void HideDialogue() {
        StopCurrentAudio();

        // Clear ourselves as active marker
        if (s_activeMarker == this)
            s_activeMarker = nullptr;

        if (!dialogueUI.IsValid()) return;
        SetActive(false, dialogueUI.GetEntity());
    }

    void StopCurrentAudio() {
        if (!audioName.empty())
            StopAudio("event:/" + audioName);
    }
};

// Initialize static member
DialogueMarker* DialogueMarker::s_activeMarker = nullptr;