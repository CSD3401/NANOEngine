#pragma once
#include "EngineAPI.hpp"

/*
* Misc_MaterialSwitcher
* Switches this entity's material between past and present states.
*/

class Misc_MaterialSwitcher : public IScript {
public:
    Misc_MaterialSwitcher() = default;
    ~Misc_MaterialSwitcher() override = default;

    // === Lifecycle Methods ===
    void Awake() override {}

    void Initialize(Entity entity) override {
        SCRIPT_COMPONENT_REF(pastMaterial, MaterialRef);
        SCRIPT_COMPONENT_REF(presentMaterial, MaterialRef);
    }

    void Start() override {}

    void Update(double deltaTime) override {}

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}

    void OnDisable() override {}

    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Misc_MaterialSwitcher";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    // === Public Actions ===
    void ShowPast() {
        ApplyMaterial(pastMaterial, true);
    }

    void ShowPresent() {
        ApplyMaterial(presentMaterial, false);
    }

private:
    MaterialRef pastMaterial;
    MaterialRef presentMaterial;

    void ApplyMaterial(const MaterialRef& material, bool isPast) {
        if (!material.IsValid()) {
            LOG_WARNING(isPast
                ? "Misc_MaterialSwitcher: missing past material reference"
                : "Misc_MaterialSwitcher: missing present material reference");
            return;
        }

        const Entity targetEntity = GetEntity();
        bool appliedAny = false;
        const size_t childCount = GetChildCount(targetEntity);
        if (childCount > 0) {
            for (size_t i = 0; i < childCount; ++i) {
                const Entity child = GetChild(i, targetEntity);
                appliedAny = ApplyMaterialToEntity(child, material) || appliedAny;
            }
        } else {
            appliedAny = ApplyMaterialToEntity(targetEntity, material);
        }

        if (!appliedAny) {
            LOG_WARNING("Misc_MaterialSwitcher: no Renderer found to apply material");
        }
    }

    bool ApplyMaterialToEntity(Entity entity, const MaterialRef& material) {
        if (!Query::HasRenderer(entity)) {
            return false;
        }
        if (!material.IsValid()) {
            return false;
        }
        SetMaterialRef(GetRendererRef(entity), material);
        return true;
    }
};
