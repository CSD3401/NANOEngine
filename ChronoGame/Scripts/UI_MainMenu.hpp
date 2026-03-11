#pragma once
#include "EngineAPI.hpp"

/*
* UI_MainMenu ensures the cursor is visible and unlocked in the main menu.
* Attach this script to any entity in the main menu scene.
*/
class UI_MainMenu : public IScript {
public:
    UI_MainMenu() = default;
    ~UI_MainMenu() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        Input::SetMouseLocked(false);
        NE::Scripting::SetMouseVisible(true);
    }

    void Update(double /*deltaTime*/) override {}
    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "UI_MainMenu"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }
};
