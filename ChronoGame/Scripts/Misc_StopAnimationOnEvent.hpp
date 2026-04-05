#pragma once
#include "EngineAPI.hpp"

/*
* Misc_StopAnimationOnEvent
* Stops an object's Animator when a specific event is received.
*
* Usage:
*   1. Attach this script to the animated object, or assign targetObject.
*   2. Make sure the target entity has an Animator component.
*   3. Set stopEventName to the event you want to listen for.
*   4. Fire that event elsewhere with Events::Send("YourEventName");
*
* Example event names already used in this project:
*   - "PuzzleSolved1"
*   - "MirrorPuzzleSolved"
*   - "SequencerSolved"
*   - "ChronoActivated"
*   - "ChronoDeactivated"
*/
class Misc_StopAnimationOnEvent : public IScript {
public:
    Misc_StopAnimationOnEvent() {
        SCRIPT_GAMEOBJECT_REF(targetObject);
        SCRIPT_FIELD(stopEventName, String);
        SCRIPT_FIELD(stopOnlyOnce, Bool);
        SCRIPT_FIELD(stopImmediatelyOnStart, Bool);
    }

    ~Misc_StopAnimationOnEvent() override = default;

    void Awake() override {}

    void Initialize(Entity entity) override {
        (void)entity;
    }

    void Start() override {
        listeningEnabled = true;
        CacheTargetEntity();

        if (stopEventName.empty()) {
            LOG_ERROR("Misc_StopAnimationOnEvent: stopEventName is empty on " + GetEntityName());
            return;
        }

        if (!ValidateTarget()) {
            return;
        }

        Events::Listen(stopEventName.c_str(), [this](void* data) {
            (void)data;

            if (!listeningEnabled) return;
            if (stopOnlyOnce && hasStopped) return;

            StopAnimation();
        });

        LOG_INFO("Misc_StopAnimationOnEvent: Listening for event '" + stopEventName + "' on " + GetEntityName());

        if (stopImmediatelyOnStart) {
            StopAnimation();
        }
    }

    void Update(double) override {}

    void OnDestroy() override {
        listeningEnabled = false;
    }

    void OnEnable() override {
        listeningEnabled = true;
    }

    void OnDisable() override {
        listeningEnabled = false;
    }

    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Misc_StopAnimationOnEvent";
    }

    void OnCollisionEnter(Entity) override {}
    void OnCollisionExit(Entity) override {}
    void OnCollisionStay(Entity) override {}
    void OnTriggerEnter(Entity) override {}
    void OnTriggerExit(Entity) override {}
    void OnTriggerStay(Entity) override {}

private:
    GameObjectRef targetObject;

    std::string stopEventName = "";
    bool stopOnlyOnce = true;
    bool stopImmediatelyOnStart = false;

    bool listeningEnabled = false;
    bool hasStopped = false;
    Entity targetEntity = NE::Scripting::INVALID_ENTITY;

    void CacheTargetEntity() {
        if (targetObject.IsValid()) {
            targetEntity = targetObject.GetEntity();
        }
        else {
            targetEntity = GetEntity();
        }
    }

    bool ValidateTarget() {
        if (targetEntity == NE::Scripting::INVALID_ENTITY) {
            LOG_WARNING("Misc_StopAnimationOnEvent: target entity is invalid");
            return false;
        }

        if (!Query::HasAnimator(targetEntity)) {
            LOG_WARNING("Misc_StopAnimationOnEvent: target entity has no Animator component");
            return false;
        }

        return true;
    }

    void StopAnimation() {
        CacheTargetEntity();
        if (!ValidateTarget()) return;

        Anim_Stop(targetEntity);
        hasStopped = true;

        LOG_INFO("Misc_StopAnimationOnEvent: Stopped animation on entity " << targetEntity
                 << " after receiving event '" << stopEventName << "'");
    }
};
