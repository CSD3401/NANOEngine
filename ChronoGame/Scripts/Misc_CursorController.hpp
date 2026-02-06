#pragma once
#include "EngineAPI.hpp"

class Misc_CursorController : public IScript {
public:
    Misc_CursorController() {
        SCRIPT_FIELD(hideOnStart, bool);
    }
    ~Misc_CursorController() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        if (hideOnStart) {
            Input::HideCursor();
        }
    }

    void Update(double deltaTime) override {
        (void)deltaTime;
        if (Input::WasKeyPressed('K')) {
            if (Input::IsCursorVisible()) {
                Input::HideCursor();
            } else {
                Input::ShowCursor();
            }
        }
    }

    void OnDestroy() override {
        Input::ShowCursor();
    }

    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_CursorController"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    bool hideOnStart = true;
};
