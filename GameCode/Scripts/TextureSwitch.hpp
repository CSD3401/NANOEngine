#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
#include "ECS/Components/Transform.hpp"
#include "Events/EventBus.hpp"
#include "EditorInterface/RendererExports.hpp"
#include "Core/Couroutine.hpp"
#include <Math/Vec3.hpp>

void TextureSwitchActivate(int entity) {    
    
    CoroutineHandle h = Engine_CreateCoroutine();

    NE::Renderer::Command::AssignMaterial(entity, "Assets/Unlit.nanomat");

    // Wait defined seconds
    Engine_AddWaitForSeconds(h, 5.f);

    Engine_AddAction(h, [entity]() {NE::Renderer::Command::AssignMaterial(entity, "Assets/Basic.nanomat"); });

    Engine_StartCoroutine(h);


    
}

class TextureSwitch : public IScript {
public:
    TextureSwitch() {
        // Register all our fields using the simple macros
        SCRIPT_FIELD(isActive, Bool);
        SCRIPT_FIELD(objectName, String);

        std::cout << "[TextureSwitch] Created with fields registered" << std::endl;
    }

    void Initialize(NE::ECS::Entity entity) override {


        NANOEngine::Events::RegisterScriptEventListener("TimeSwapNow", [this](void* data) {TextureSwitchActivate(this->GetEntity()); });
    }

    void Update(double deltaTime) override {
        if (!isActive) return;


        
    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "TextureSwitch";
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
};
