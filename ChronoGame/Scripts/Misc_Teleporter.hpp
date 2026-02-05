#pragma once
#include "EngineAPI.hpp"
#include <map>
#include <vector>

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */

class Misc_Teleporter : public IScript {
public:
    Misc_Teleporter() { 
        SCRIPT_GAMEOBJECT_REF(player);
		SCRIPT_COMPONENT_REF(targetTransformRef, TransformRef);
    }

    ~Misc_Teleporter() override = default;

    // === Lifecycle Methods ===

    void Awake() override {
        // Called when the script component is first created
    }

    void Initialize(Entity entity) override {
        // Called to initialize the script with its entity
    }

    void Start() override {

    }

    void Update(double deltaTime) override {

    }

    void OnDestroy() override {
        // Called when the script is about to be destroyed
    }

    // === Optional Callbacks ===

    void OnEnable() override {

    }

    void OnDisable() override {
        // Called when the script is disabled
    }

    void OnValidate() override {
        // Called when a field value is changed in the editor
    }

    const char* GetTypeName() const override {
        return "Misc_Teleporter";
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { 
		CC_SetPosition(GetPosition(targetTransformRef), player.GetEntity());
    }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef player;
	TransformRef targetTransformRef;
};