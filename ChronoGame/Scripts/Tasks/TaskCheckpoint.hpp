#pragma once
#include "../EngineAPI.hpp"

class TaskCheckpoint : public IScript {
public:
    TaskCheckpoint() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
    }

    ~TaskCheckpoint() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { (void)entity; }
    void Start() override {}

    void Update(double /*dt*/) override {
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "TaskCheckpoint"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override {
        if (isTriggered || !playerRef.IsValid() || other != playerRef.GetEntity()) {
            return;
        }

        isTriggered = true;
        Events::Send("TaskCheckpointCompleted");
    }

    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef playerRef;
    bool isTriggered = false;
};
