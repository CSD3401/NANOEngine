#pragma once
#include "EngineAPI.hpp"

/**
 * Credits_ReturnToMainMenu
 * -----------------------
 * Attach to any entity in the Credit scene (e.g. the Credit Controller or an empty).
 * Listens for "CreditsDone" (fired by Credits_Controller when scrolling ends)
 * and switches back to the Main Menu scene.
 */
class Credits_ReturnToMainMenu : public IScript {
public:
    Credits_ReturnToMainMenu() = default;
    ~Credits_ReturnToMainMenu() override = default;

    void Awake() override {}
    void Initialize(Entity) override {}
    void Start() override {
        Events::Listen("CreditsDone", [](void*) {
            NE::Scripting::SwitchScene("7f6eb653-c6f7-426d-b9ac-9c0bada73cfc");
        });
    }

    void Update(double /*dt*/) override {}
    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "Credits_ReturnToMainMenu"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }
};
