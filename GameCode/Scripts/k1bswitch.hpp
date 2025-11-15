#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
#include "ECS/Components/Transform.hpp"
#include "Events/EventBus.hpp"
#include "EditorInterface/RendererExports.hpp"
#include "Core/Couroutine.hpp"
#include <Math/Vec3.hpp>


class k1bswitch : public IScript {
public:
    k1bswitch() {
        // Register all our fields using the simple macros
        SCRIPT_FIELD(isActive, Bool);
        SCRIPT_FIELD(objectName, String);

        std::cout << "[TextureSwitch] Created with fields registered" << std::endl;
    }

    void Initialize(NE::ECS::Entity entity) override {


        //NANOEngine::Events::RegisterScriptEventListener("TimeSwapNow", [entity](void* data) {TextureSwitchActivate(entity); });
    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        if (NE::InputManager::WasKeyPressed('E')) {
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
    void OnCollisionEnter(NE::ECS::Entity other) override {}
    void OnCollisionExit(NE::ECS::Entity other) override {}
    void OnTriggerEnter(NE::ECS::Entity other) override {}
    void OnTriggerExit(NE::ECS::Entity other) override {}

private:
    // === Exposed Fields ===
    // These will automatically appear in the editor inspector
    bool isActive = true;
    std::string objectName = "TestObject";

    bool switched = false;
};
