#pragma once
#include <iostream>
#include "EngineAPI.hpp"

class Gears : public NE::Scripting::IScript {
public:
    Gears() {
    }

    void Initialize(NE::Scripting::Entity entity) override {

    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        if (Input::WasKeyPressed('E')) {
            auto& transform = NE::ECS::Command::GetEntityTransform(GetEntity());

            if (transform.scale == NE::Math::Vec3(0.f, 0.f, 0.f)) {
                transform.scale = NE::Math::Vec3(0.2f, 0.2f, 0.2f);
            } else {
                transform.scale = NE::Math::Vec3(0.f, 0.f, 0.f);
            }
            //if (!switched) {
            //    light.color = NE::Math::Vec3(0.7f, 0.4f, 0.f);
            //} else {
            //    light.color = NE::Math::Vec3(1.f, 1.f, 1.f);
            //}

            //switched = !switched;
        }

    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "Gears";
    }

    // Event handlers (required by interface)
    void OnCollisionEnter(NE::Scripting::Entity other) override {}
    void OnCollisionExit(NE::Scripting::Entity other) override {}
    void OnTriggerEnter(NE::Scripting::Entity other) override {}
    void OnTriggerExit(NE::Scripting::Entity other) override {}

private:
    // === Exposed Fields ===
    // These will automatically appear in the editor inspector
    bool isActive = true;
    //std::string objectName = "TestObject";

    bool switched = false;
};
