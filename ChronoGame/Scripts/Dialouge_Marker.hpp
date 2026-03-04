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
 * Setup:
 *   1. Assign playerRef     -- drag the Player entity in
 *   2. Assign dialogueUI    -- drag the shared UIText/panel GameObject in
 *   3. Set dialogueText     -- the line(s) to display
 */
class DialogueMarker : public IScript {
public:
    DialogueMarker() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(dialogueUI);
        SCRIPT_FIELD(dialogueText, String);
        SCRIPT_FIELD(audioName, String); // FMOD event path, played on enter, stopped on dismiss
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
        // Only runs after trigger fires, waiting for click to dismiss
        if (!dismissing) return;

        if (Input::WasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
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
        // Ignore if already triggered, or it's not the player
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

    void ShowDialogue() {
        if (!dialogueUI.IsValid()) return;
        NE::Scripting::SetUIText(dialogueUI.GetEntity(), dialogueText.c_str());
        SetActive(true, dialogueUI.GetEntity());
        if (!audioName.empty())
            PlayAudio("event:/" + audioName);
    }

    void HideDialogue() {
        if (!dialogueUI.IsValid()) return;
        SetActive(false, dialogueUI.GetEntity());
        if (!audioName.empty())
            StopAudio("event:/" + audioName);
    }
};