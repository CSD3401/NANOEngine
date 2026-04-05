#pragma once
#include "EngineAPI.hpp"

/*
* Misc_SolvedMaterialOverride
* Forces a "solved" material set (e.g. Variation 6) once a puzzle is solved.
*
* Designed to be used alongside Misc_MaterialSwitcher WITHOUT modifying it:
* - Before solved: your normal past/present switcher controls visuals.
* - After solved: this script re-applies solved materials on:
*     - the configured solved event
*     - ChronoActivated / ChronoDeactivated (so it stays correct across time swaps)
*
* Setup:
* - Attach to the same entity you have Misc_MaterialSwitcher on (or a parent).
* - Set solvedEventName to an event your puzzle sends on success (recommended).
* - Assign solvedPastMaterial / solvedPresentMaterial (Variation 6 equivalents).
*/

class Misc_SolvedMaterialOverride : public IScript {
public:
    Misc_SolvedMaterialOverride() {
        SCRIPT_COMPONENT_REF(solvedPastMaterial, MaterialRef);
        SCRIPT_COMPONENT_REF(solvedPresentMaterial, MaterialRef);
        SCRIPT_FIELD(solvedEventName, String);
    }
    ~Misc_SolvedMaterialOverride() override = default;

    void Awake() override {}

    void Initialize(Entity entity) override { (void)entity; }
    void Start() override {
        // Register in Start (not Awake) so inspector fields are guaranteed loaded.
        if (m_eventsRegistered) return;

        // Mirror Misc_MaterialSwitcher semantics:
        // ChronoActivated -> Past
        // ChronoDeactivated -> Present
        Events::Listen("ChronoActivated", [this](void*) {
            m_isPast = true;
            if (m_isSolved) ApplySolvedMaterial();
            });
        Events::Listen("ChronoDeactivated", [this](void*) {
            m_isPast = false;
            if (m_isSolved) ApplySolvedMaterial();
            });

        // Prefer a per-puzzle solved event to avoid responding to unrelated puzzles.
        if (!solvedEventName.empty()) {
            Events::Listen(solvedEventName.c_str(), [this](void*) {
                if (m_isSolved) return;
                m_isSolved = true;
                ApplySolvedMaterial();
                });
        } else {
            LOG_WARNING("Misc_SolvedMaterialOverride: solvedEventName is empty (will never apply on solve)");
        }

        m_eventsRegistered = true;
    }
    void Update(double deltaTime) override { (void)deltaTime; }
    void OnDestroy() override {}

    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_SolvedMaterialOverride"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    MaterialRef solvedPastMaterial{};
    MaterialRef solvedPresentMaterial{};
    std::string solvedEventName{};

    bool m_eventsRegistered = false;
    bool m_isSolved = false;
    bool m_isPast = false;

    void ApplySolvedMaterial() {
        const MaterialRef& mat = m_isPast ? solvedPastMaterial : solvedPresentMaterial;
        if (!mat.IsValid()) {
            LOG_WARNING(m_isPast
                ? "Misc_SolvedMaterialOverride: missing solvedPastMaterial"
                : "Misc_SolvedMaterialOverride: missing solvedPresentMaterial");
            return;
        }

        const Entity targetEntity = GetEntity();
        bool appliedAny = ApplyMaterialRecursive(targetEntity, mat);

        if (!appliedAny) {
            LOG_WARNING("Misc_SolvedMaterialOverride: no Renderer found to apply material");
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

    bool ApplyMaterialRecursive(Entity entity, const MaterialRef& material) {
        bool appliedAny = ApplyMaterialToEntity(entity, material);

        const size_t childCount = GetChildCount(entity);
        for (size_t i = 0; i < childCount; ++i) {
            const Entity child = GetChild(i, entity);
            appliedAny = ApplyMaterialRecursive(child, material) || appliedAny;
        }
        return appliedAny;
    }
};

