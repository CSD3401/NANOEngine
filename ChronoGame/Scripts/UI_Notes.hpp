#pragma once
#include "EngineAPI.hpp"
#include "Interactable_.hpp"
#include "Player_Controller.hpp"
#include "Player_Raycast.hpp"

#define GLFW_MOUSE_BUTTON_LEFT 0

class UI_Notes : public Interactable_ {
public:
    UI_Notes() = default;
    ~UI_Notes() override = default;

    void Initialize(Entity entity) override {
        _SetEntity(entity);
        SCRIPT_GAMEOBJECT_REF(objectToActivate);
    }

    void Start() override {
        ResolvePlayerController();
        ResolvePlayerRaycast();
    }

    const char* GetTypeName() const override { return "UI_Notes"; }

    // Open with raycast interaction only
    void Interact() override {
        if (!objectToActivate) {
            return;
        }

        Entity target = objectToActivate.GetEntity();

        if (!noteIsOpen) {
            CacheAndDisablePlayerInput();
            noteIsOpen = true;
            SetActive(true, target);
            waitingForMouseReleaseAfterOpen = true;
            LOG_DEBUG("open note!");
            PlayAudio("event:/COLOR_CLICK");
        }
    }

    // Close without raycast: any later left click will close it
    void Update(double /*dt*/) override {
        if (!objectToActivate) return;

        Entity target = objectToActivate.GetEntity();

        if (!noteIsOpen) {
            waitingForMouseReleaseAfterOpen = false;
            return;
        }

        // Ignore the same click that opened the note
        if (waitingForMouseReleaseAfterOpen) {
            if (!Input::IsMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
                waitingForMouseReleaseAfterOpen = false;
            }
            return;
        }

        // Any new left click closes the note
        if (Input::WasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
            noteIsOpen = false;
            SetActive(false, target);
            RestorePlayerInput();
            LOG_DEBUG("close note!");
            PlayAudio("event:/COLOR_CLICK");
        }
    }

    void OnDisable() override {
        if (!noteIsOpen)
            return;

        noteIsOpen = false;
        waitingForMouseReleaseAfterOpen = false;

        if (objectToActivate)
            SetActive(false, objectToActivate.GetEntity());

        RestorePlayerInput();
    }

    void OnDestroy() override {
        // Safe without cached Entity/Component pointers (they may be invalid during teardown).
        RestorePlayerInput();
    }

private:
    GameObjectRef objectToActivate;
    bool noteIsOpen = false;
    bool waitingForMouseReleaseAfterOpen = false;

    Player_Controller* cachedPlayerController = nullptr;
    bool cachedPlayerControllerWasEnabled = true;

    Player_Raycast* cachedPlayerRaycast = nullptr;
    bool cachedPlayerRaycastWasEnabled = true;

    void ResolvePlayerController() {
        if (cachedPlayerController)
            return;

        auto players = GameObject::FindObjectsOfType<Player_Controller>();
        if (players.size() == 0) {
            LOG_WARNING("UI_Notes: could not auto-find Player_Controller.");
            return;
        }
        if (players.size() > 1) {
            LOG_WARNING("UI_Notes: multiple Player_Controller found; using the first one.");
        }

        cachedPlayerController = players.begin()->GetComponent<Player_Controller>();
    }

    void ResolvePlayerRaycast() {
        if (cachedPlayerRaycast)
            return;

        auto raycasts = GameObject::FindObjectsOfType<Player_Raycast>();
        if (raycasts.size() == 0) {
            LOG_WARNING("UI_Notes: could not auto-find Player_Raycast.");
            return;
        }
        if (raycasts.size() > 1) {
            LOG_WARNING("UI_Notes: multiple Player_Raycast found; using the first one.");
        }

        cachedPlayerRaycast = raycasts.begin()->GetComponent<Player_Raycast>();
    }

    void CacheAndDisablePlayerInput() {
        ResolvePlayerController();
        ResolvePlayerRaycast();

        if (cachedPlayerController) {
            cachedPlayerControllerWasEnabled = cachedPlayerController->IsEnabled();
            // Keep current look rotation so the camera does not snap when the note closes.
            cachedPlayerController->ResetMovementOnly();
            cachedPlayerController->SetEnabled(false);
        }

        if (cachedPlayerRaycast) {
            cachedPlayerRaycastWasEnabled = cachedPlayerRaycast->IsEnabled();
            cachedPlayerRaycast->SetEnabled(false);
        }
    }

    void RestorePlayerInput() {
        Player_Controller* pc = nullptr;
        auto players = GameObject::FindObjectsOfType<Player_Controller>();
        if (!players.empty())
            pc = players.begin()->GetComponent<Player_Controller>();
        if (pc) {
            pc->ResetMovementOnly();
            pc->SetEnabled(cachedPlayerControllerWasEnabled);
        }

        Player_Raycast* pr = nullptr;
        auto raycasts = GameObject::FindObjectsOfType<Player_Raycast>();
        if (!raycasts.empty())
            pr = raycasts.begin()->GetComponent<Player_Raycast>();
        if (pr)
            pr->SetEnabled(cachedPlayerRaycastWasEnabled);

        cachedPlayerController = nullptr;
        cachedPlayerRaycast = nullptr;
    }
};
