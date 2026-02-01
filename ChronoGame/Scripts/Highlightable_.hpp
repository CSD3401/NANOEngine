#pragma once
#include "EngineAPI.hpp"
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Highlightable_ is the parent class for all highlightable objects in the game.
* It simply provides a virtual function SetHighlight that can be overridden by child classes.
*/

class Highlightable_ : public IScript {
public:
    Highlightable_() {}
    ~Highlightable_() override = default;

    // === Custom Methods ===
    virtual void SetHighlight(bool state) {}

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {}
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Highlightable_"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
};