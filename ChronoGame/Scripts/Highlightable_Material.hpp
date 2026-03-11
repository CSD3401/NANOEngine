#pragma once
#include "EngineAPI.hpp"
#include "Highlightable_.hpp"
#include "Misc_Manager.hpp"
/*
* === WORK IN PROGRESS ===
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Highlightable_ is the parent class for all highlightable objects in the game.
* It simply provides a virtual function SetHighlight that can be overridden by child classes.
*/

class Highlightable_Material : public Highlightable_ {
public:
    Highlightable_Material() {}
    ~Highlightable_Material() override = default;

    // === Custom Methods ===
    void SetHighlight(bool state) override
    {
        if (state) {
			NE::Renderer::Command::AssignMaterial(
                GetEntity(),
                highlightMaterial);
        }
        else {
            NE::Renderer::Command::AssignMaterial(
                GetEntity(),
                defaultMaterial);
        }
    }

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        // Store highlight material from Manager
        auto m = GameObject::FindObjectsOfType<Misc_Manager>();
        if (m.size() == 0) {
            LOG_ERROR("No managers found!");
        }
        else if (m.size() > 1) {
            LOG_WARNING("Multiple managers found!");
        }
        else {
            highlightMaterial = m.begin()->GetComponent<Misc_Manager>()->GetHighlightMaterial();
        }

        // Store default material from this entity
		defaultMaterial = GetMaterialRef(GetRendererRef(GetEntity()));
    }
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Highlightable_Material"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    MaterialRef defaultMaterial{};
    MaterialRef highlightMaterial{};
};