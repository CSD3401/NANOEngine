#pragma once
#include <iostream>
#include "EngineAPI.hpp"

class k2bswitch : public IScript {
public:
    k2bswitch() {
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
                NE::Renderer::Command::AssignMaterial(GetEntity(), "36e41bfb-b41e-445a-922b-a634450d6f02");
            } else {
                NE::Renderer::Command::AssignMaterial(GetEntity(), "d1d2a6cb-e531-4452-a75c-122ec80a39dd");
            }

            switched = !switched;
        }

    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "k2bswitch";
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
