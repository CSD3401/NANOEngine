#pragma once
#include <iostream>
#include "EngineAPI.hpp"

class k1bswitch : public IScript {
public:
    k1bswitch() {
        // Register all our fields using the simple macros
        SCRIPT_FIELD(isActive, Bool);
        SCRIPT_FIELD(objectName, String);

        std::cout << "[TextureSwitch] Created with fields registered" << std::endl;
    }

    void Initialize(Entity entity) override {


        //Events::Listen("TimeSwapNow", [entity](void* data) {TextureSwitchActivate(entity); });
    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        if (Input::WasKeyPressed('E')) {
            if (!switched) {
                NE::Renderer::Command::AssignMaterial(GetEntity(), "d246b58e-4ef2-4539-a2e4-7f56d920185f");
            } else {
                NE::Renderer::Command::AssignMaterial(GetEntity(), "03f64d9a-5b8e-46ae-9067-7e8c95393171");
            }

            switched = !switched;
        }

    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "k1bswitch";
    }

    // Event handlers (required by interface)
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    // === Exposed Fields ===
    // These will automatically appear in the editor inspector
    bool isActive = true;
    std::string objectName = "TestObject";

    bool switched = false;
};
