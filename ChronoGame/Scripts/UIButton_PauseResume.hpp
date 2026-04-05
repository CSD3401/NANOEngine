#pragma once
#include "EngineAPI.hpp"
#include "Misc_PauseMenuHotkey.hpp"
#include <ScriptSDK/UI.h>

/**
 * UIButton_PauseResume
 * --------------------
 * Attach to the Resume button inside the pause menu.
 * On click, unpauses the in-scene pause menu (same result as pressing P again).
 */
class UIButton_PauseResume : public IScript {
public:
    UIButton_PauseResume() {
        SCRIPT_GAMEOBJECT_REF(pauseControllerRef);
    }

    ~UIButton_PauseResume() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { m_button = entity; }
    void Start() override {}

    void Update(double /*dt*/) override {
        if (m_button == 0) return;
        if (!UI::WasButtonClicked(m_button) || !UI::IsButtonInteractable(m_button))
            return;

        Misc_PauseMenuHotkey* pauseController = ResolvePauseController();
        if (!pauseController) {
            LOG_WARNING("UIButton_PauseResume: pauseControllerRef missing or has no Misc_PauseMenuHotkey");
            return;
        }

        pauseController->SetPaused(false);
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UIButton_PauseResume"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Entity m_button = 0;
    GameObjectRef pauseControllerRef;

    Misc_PauseMenuHotkey* ResolvePauseController() const {
        if (!pauseControllerRef.IsValid()) return nullptr;

        GameObject controllerGO(pauseControllerRef.GetEntity());
        if (!controllerGO.IsValid()) return nullptr;

        return controllerGO.GetComponent<Misc_PauseMenuHotkey>();
    }
};
