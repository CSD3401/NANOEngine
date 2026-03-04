#pragma once
#include "EngineAPI.hpp"

/*
* Misc_TimeAnimationController
* Plays / stops an Animator based on the time-state events already used in the project.
*
* Requested behavior:
*   - Press 2 / present  -> play animation
*   - Press 1 / past     -> stop animation
*
* Event mapping already present in project:
*   - ChronoActivated   = past
*   - ChronoDeactivated = present
*   - TimePastEnabled   = past
*   - TimePastDisabled  = present
*
* Usage:
*   1. Attach this script to the animated object, OR assign targetObject.
*   2. Make sure the target entity has an Animator component.
*/
class Misc_TimeAnimationController : public IScript {
public:
    Misc_TimeAnimationController() {
        SCRIPT_GAMEOBJECT_REF(targetObject);
        SCRIPT_FIELD(playOnPresent, Bool);
        SCRIPT_FIELD(stopOnPast, Bool);
        SCRIPT_FIELD(applyStateOnStart, Bool);
        SCRIPT_FIELD(startInPresent, Bool);
    }

    ~Misc_TimeAnimationController() override = default;

    void Awake() override {
        RegisterEventListeners();
    }

    void Initialize(Entity entity) override {
        (void)entity;
    }

    void Start() override {
        listeningEnabled = true;
        CacheTargetEntity();

        if (!ValidateTarget()) {
            return;
        }

        if (applyStateOnStart) {
            if (startInPresent) {
                HandlePresent();
            }
            else {
                HandlePast();
            }
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
        return "Misc_TimeAnimationController";
    }

    void OnCollisionEnter(Entity) override {}
    void OnCollisionExit(Entity) override {}
    void OnCollisionStay(Entity) override {}
    void OnTriggerEnter(Entity) override {}
    void OnTriggerExit(Entity) override {}
    void OnTriggerStay(Entity) override {}

private:
    GameObjectRef targetObject;

    bool playOnPresent = true;
    bool stopOnPast = true;
    bool applyStateOnStart = false;
    bool startInPresent = false;

    bool listenersRegistered = false;
    bool listeningEnabled = false;
    Entity targetEntity = NE::Scripting::INVALID_ENTITY;

    void RegisterEventListeners() {
        if (listenersRegistered) {
            return;
        }

        Events::Listen("ChronoActivated", [this](void*) {
            if (!listeningEnabled) return;
            HandlePast();
        });

        Events::Listen("ChronoDeactivated", [this](void*) {
            if (!listeningEnabled) return;
            HandlePresent();
        });

        Events::Listen("TimePastEnabled", [this](void*) {
            if (!listeningEnabled) return;
            HandlePast();
        });

        Events::Listen("TimePastDisabled", [this](void*) {
            if (!listeningEnabled) return;
            HandlePresent();
        });

        listenersRegistered = true;
    }

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
            LOG_WARNING("Misc_TimeAnimationController: target entity is invalid");
            return false;
        }

        if (!Query::HasAnimator(targetEntity)) {
            LOG_WARNING("Misc_TimeAnimationController: target entity has no Animator component");
            return false;
        }

        return true;
    }

    void HandlePresent() {
        CacheTargetEntity();
        if (!ValidateTarget()) return;

        if (playOnPresent) {
            Anim_Play(targetEntity);
            LOG_INFO("Misc_TimeAnimationController: PRESENT -> Anim_Play on entity " << targetEntity);
        }
    }

    void HandlePast() {
        CacheTargetEntity();
        if (!ValidateTarget()) return;

        if (stopOnPast) {
            Anim_Stop(targetEntity);
            LOG_INFO("Misc_TimeAnimationController: PAST -> Anim_Stop on entity " << targetEntity);
        }
    }
};
