#pragma once
#include "EngineAPI.hpp"
#include "Interactable_NoteCollector.hpp"

/*
* NoteCollector_Controller
* Press key to collect/return closest note.
*/

class NoteCollector_Controller : public IScript {
public:
    NoteCollector_Controller() = default;
    ~NoteCollector_Controller() override = default;

    // === Lifecycle Methods ===
    void Awake() override {}

    void Initialize(Entity entity) override {
        SCRIPT_GAMEOBJECT_REF(collectorRef);
        SCRIPT_FIELD(interactKey, Int);
    }

    void Start() override {}

    void Update(double deltaTime) override {
        if (Input::WasKeyPressed(interactKey)) {
            TriggerToggle();
        }
    }

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "NoteCollector_Controller";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef collectorRef;
    int interactKey = 'F';
    bool warnedMissingRef = false;
    bool warnedMissingComponent = false;

    void TriggerToggle() {
        if (!collectorRef.IsValid()) {
            if (!warnedMissingRef) {
                LOG_WARNING("NoteCollector_Controller: missing collector reference");
                warnedMissingRef = true;
            }
            return;
        }
        auto* collector = GameObject(collectorRef).GetComponent<Interactable_NoteCollector>();
        if (!collector) {
            if (!warnedMissingComponent) {
                LOG_WARNING("NoteCollector_Controller: entity has no Interactable_NoteCollector");
                warnedMissingComponent = true;
            }
            return;
        }
        collector->ToggleClosestNote();
    }
};
