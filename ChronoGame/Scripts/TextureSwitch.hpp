#pragma once
#include <iostream>
#include "EngineAPI.hpp"

void TextureSwitchActivate(int entity) {

    Coroutines::Handle h = Coroutines::Create();

    //NE::Renderer::Command::AssignMaterial(entity, "Assets/Unlit.nanomat");

    // Wait defined seconds
    Coroutines::AddWait(h, 5.f);

    //Coroutine::AddAction(h, [entity]() {NE::Renderer::Command::AssignMaterial(entity, "Assets/Basic.nanomat"); });

    Coroutines::Start(h);



}

class TextureSwitch : public IScript {
public:
    TextureSwitch() {
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
                NE::Renderer::Command::AssignMaterial(GetEntity(), "41e072ab-c276-4cf3-8b95-6c92401fcdec");
            } else {
                NE::Renderer::Command::AssignMaterial(GetEntity(), "ad9dd997-3747-4fe2-8abe-723a6d7fc27f");
            }

            switched = !switched;
        }

    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "TextureSwitch";
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
