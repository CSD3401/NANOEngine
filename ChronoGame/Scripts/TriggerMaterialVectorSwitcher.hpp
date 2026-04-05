#pragma once
#include "EngineAPI.hpp"

/**
 * TriggerMaterialVectorSwitcher
 * -------------------------------
 * Attach to a trigger-box entity.
 *
 * When the player enters the trigger:
 *   - sets `newMaterial` on each target entity (and optionally their children)
 *   - optionally reverts on trigger exit (requires caching originals)
 *
 * Targeting modes:
 *   - If `targetEntities` is non-empty: those are the roots that get material swapped.
 *   - If `targetEntities` is empty: swaps material on this script entity (and optionally children).
 *
 * Notes about where to attach the script:
 *   - If you leave `targetEntities` empty, then yes, attach it under the "past parent"
 *     (or whichever subtree you want affected) because the script will use its own hierarchy.
 *   - If you fill `targetEntities`, attach anywhere; only the listed entities are modified.
 */
class TriggerMaterialVectorSwitcher : public IScript {
public:
    TriggerMaterialVectorSwitcher() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_FIELD(oneShot, Bool);
        SCRIPT_FIELD(applyToChildren, Bool);
        SCRIPT_FIELD(revertOnExit, Bool);
        SCRIPT_FIELD(deferApplyByOneFrame, Bool);
        SCRIPT_FIELD(deferRevertByOneFrame, Bool);
        SCRIPT_COMPONENT_REF(newMaterial, MaterialRef);
        RegisterGameObjectRefVectorField("targetEntities", &targetEntities);
    }

    ~TriggerMaterialVectorSwitcher() override = default;

    void Awake() override {
        // No-op: we build caches in Start so inspector fields are available.
    }

    void Initialize(Entity /*entity*/) override {}

    void Start() override {
        m_switched = false;

        if (!newMaterial.IsValid()) {
            LOG_WARNING("TriggerMaterialVectorSwitcher: newMaterial is not assigned");
            return;
        }

        if (!revertOnExit) return;

        // Precompute impacted entity list and cache original materials for reversion.
        m_affectedEntities.clear();
        m_originalMaterials.clear();
        m_originalMaterials.reserve(256);

        if (!targetEntities.empty()) {
            for (auto& ref : targetEntities) {
                if (!ref.IsValid()) continue;
                CollectEntities(ref.GetEntity());
            }
        } else {
            CollectEntities(GetEntity());
        }

        // Cache originals by renderer material per affected entity.
        for (const Entity e : m_affectedEntities) {
            if (!Query::HasRenderer(e)) {
                m_originalMaterials.push_back(MaterialRef{});
                continue;
            }
            m_originalMaterials.push_back(GetMaterialRef(GetRendererRef(e)));
        }
    }

    void Update(double /*dt*/) override {}

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "TriggerMaterialVectorSwitcher"; }

    void OnCollisionEnter(Entity /*other*/) override {}
    void OnCollisionExit(Entity /*other*/) override {}
    void OnCollisionStay(Entity /*other*/) override {}

    void OnTriggerEnter(Entity other) override {
        if (oneShot && m_switched) return;
        if (!IsActiveInHierarchy()) return;

        // If playerRef is assigned, only react when that exact entity enters.
        if (playerRef.IsValid() && other != playerRef.GetEntity()) return;

        if (deferApplyByOneFrame) {
            ScheduleApplyNewMaterial();
        } else {
            ApplyNewMaterial();
        }
        m_switched = true;
    }

    void OnTriggerExit(Entity other) override {
        if (!revertOnExit) return;
        if (!m_switched) return;

        // If playerRef is assigned, only revert when the player exits.
        if (playerRef.IsValid() && other != playerRef.GetEntity()) return;
        if (deferRevertByOneFrame) {
            ScheduleRevertOriginalMaterials();
        } else {
            RevertOriginalMaterials();
        }
        m_switched = false;
    }

    void OnTriggerStay(Entity /*other*/) override {}

private:
    GameObjectRef playerRef;
    std::vector<GameObjectRef> targetEntities;

    MaterialRef newMaterial;

    bool oneShot = true;
    bool applyToChildren = true;
    bool revertOnExit = false;
    bool deferApplyByOneFrame = true;
    bool deferRevertByOneFrame = true;

    bool m_switched = false;

    // Used only when revertOnExit=true
    std::vector<Entity> m_affectedEntities;
    std::vector<MaterialRef> m_originalMaterials;

    void CollectEntities(Entity root) {
        m_affectedEntities.push_back(root);
        if (!applyToChildren) return;

        const size_t childCount = GetChildCount(root);
        for (size_t i = 0; i < childCount; ++i) {
            const Entity child = GetChild(i, root);
            CollectEntities(child);
        }
    }

    void ApplyNewMaterial() {
        if (!newMaterial.IsValid()) return;

        auto applyTo = [&](Entity root) {
            if (applyToChildren) {
                // Apply recursively without caching (fast enough for enter event).
                ApplyRecursive(root);
            } else {
                if (Query::HasRenderer(root)) {
                    SetMaterialRef(GetRendererRef(root), newMaterial);
                }
            }
        };

        if (!targetEntities.empty()) {
            for (auto& ref : targetEntities) {
                if (!ref.IsValid()) continue;
                applyTo(ref.GetEntity());
            }
        } else {
            applyTo(GetEntity());
        }
    }

    void ApplyRecursive(Entity e) {
        if (Query::HasRenderer(e)) {
            SetMaterialRef(GetRendererRef(e), newMaterial);
        }

        const size_t childCount = GetChildCount(e);
        for (size_t i = 0; i < childCount; ++i) {
            const Entity child = GetChild(i, e);
            ApplyRecursive(child);
        }
    }

    void RevertOriginalMaterials() {
        if (m_affectedEntities.empty()) return;
        if (m_originalMaterials.size() != m_affectedEntities.size()) return;

        for (size_t i = 0; i < m_affectedEntities.size(); ++i) {
            const Entity e = m_affectedEntities[i];
            if (!Query::HasRenderer(e)) continue;
            const MaterialRef& original = m_originalMaterials[i];
            if (!original.IsValid()) continue;
            SetMaterialRef(GetRendererRef(e), original);
        }
    }

    void ScheduleApplyNewMaterial() {
        // Defer by a frame so we apply after any `ChronoActivated` driven material overrides
        // (e.g. `Misc_MaterialSwitcher`) that may fire in the same update step.
        Coroutines::Handle h = Coroutines::Create();
        Coroutines::AddWait(h, 0.0f);
        Coroutines::AddAction(h, [this]() {
            if (!IsActiveInHierarchy()) return;
            ApplyNewMaterial();
        });
        Coroutines::Start(h);
    }

    void ScheduleRevertOriginalMaterials() {
        Coroutines::Handle h = Coroutines::Create();
        Coroutines::AddWait(h, 0.0f);
        Coroutines::AddAction(h, [this]() {
            if (!IsActiveInHierarchy()) return;
            RevertOriginalMaterials();
        });
        Coroutines::Start(h);
    }
};

