#pragma once
#include "EngineAPI.hpp"
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Interactable_ is the parent class for all interactable objects in the game.
* It simply provides a virtual function Interact that can be overridden by child classes.
*/

class Misc_Grabber : public IScript {
public:
    Misc_Grabber() {
    }
    ~Misc_Grabber() override = default;

    // == Custom Methods ==
    virtual void Interact()
    {
        GameObjectRef obj;
    }

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override
    {
    }
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_Grabber"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
};