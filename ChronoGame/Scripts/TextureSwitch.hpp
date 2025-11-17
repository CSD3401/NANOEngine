#pragma once
#include <iostream>
#include "EngineAPI.hpp"

void TextureSwitchActivate(int entity) {    
    
    NE::Scripting::CoroutineHandle h = Engine_CreateCoroutine();

    //NE::Renderer::Command::AssignMaterial(entity, "Assets/Unlit.nanomat");

    // Wait defined seconds
    Engine_AddWaitForSeconds(h, 5.f);

    //Engine_AddAction(h, [entity]() {NE::Renderer::Command::AssignMaterial(entity, "Assets/Basic.nanomat"); });

    Engine_StartCoroutine(h);


    
}

class TextureSwitch : public NE::Scripting::IScript {
public:
    TextureSwitch() {
        // Register all our fields using the simple macros
        SCRIPT_FIELD(isActive, Bool);
        SCRIPT_FIELD(objectName, String);

        std::cout << "[TextureSwitch] Created with fields registered" << std::endl;
    }

    void Initialize(NE::Scripting::Entity entity) override {


        //NANOEngine::Events::RegisterScriptEventListener("TimeSwapNow", [entity](void* data) {TextureSwitchActivate(entity); });
    }

    void Update(double deltaTime) override {
        if (!isActive) return;
        
        if (NE::InputManager::WasKeyPressed('E')) {
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
    void OnCollisionEnter(NE::Scripting::Entity other) override {}
    void OnCollisionExit(NE::Scripting::Entity other) override {}
    void OnTriggerEnter(NE::Scripting::Entity other) override {}
    void OnTriggerExit(NE::Scripting::Entity other) override {}

private:
    // === Exposed Fields ===
    // These will automatically appear in the editor inspector
    bool isActive = true;
    std::string objectName = "TestObject";

    bool switched = false;
};
