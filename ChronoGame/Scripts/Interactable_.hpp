#pragma once
#include "EngineAPI.hpp"
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Interactable_ is the parent class for all interactable objects in the game.
* It simply provides a virtual function Interact that can be overridden by child classes.
*/

class Interactable_ : public IScript {
public:
    Interactable_() {}
    ~Interactable_() override = default;

	// === Custom Methods ===
	virtual void Interact() {}

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
    const char* GetTypeName() const override { return "Interactable_"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {
		//LOG_INFO("Collision Enter with entity ID: " << other);
    }
    void OnCollisionExit(Entity other) override {
		LOG_INFO("Collision Exit with entity ID: " << other);
    }
    void OnCollisionStay(Entity other) override {
		//LOG_INFO("Collision Stay with entity ID: " << other);
    }
    void OnTriggerEnter(Entity other) override {
		//LOG_INFO("Trigger Enter with entity ID: " << other);
    }
    void OnTriggerExit(Entity other) override {
		LOG_INFO("Trigger Exit with entity ID: " << other);
    }
    void OnTriggerStay(Entity other) override {
		//LOG_INFO("Trigger Stay with entity ID: " << other);
    }

private:
};