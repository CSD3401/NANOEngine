#pragma once
#include "EngineAPI.hpp"

/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Interactable_ is the parent class for all interactable objects in the game.
* It simply provides a virtual function Interact that can be overridden by child classes.
*/

class Misc_Grabber : public IScript {
public:
    Misc_Grabber() {
        SCRIPT_FIELD(isGrabbing, Bool);
        SCRIPT_FIELD(distance, Float);
        SCRIPT_FIELD(grabStrength, Float);
        SCRIPT_FIELD(damping, Float);
    }
    ~Misc_Grabber() override = default;

    // == Custom Methods ==

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override
    {
    }
    void Update(double deltaTime) override 
    {
        if (IsGrabbing() && Input::WasMouseReleased(GLFW_MOUSE_BUTTON_LEFT))
        {
            LetGo();
        }

        UpdateGrabbedObject(deltaTime);
    }
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_Grabber"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    void UpdateGrabbedObject(double deltaTime)
    {
        if (!isGrabbing)
            return;

        Vec3 cameraPos = TF_GetPosition(GetEntity());
        Vec3 forward = TF_GetForward(GetEntity());

        Vec3 targetPos = cameraPos + forward * distance;

        TransformRef selfTransform = GetTransformRef(currentlyGrabbing);
        Vec3 currentPos = GetPosition(selfTransform);

        Vec3 toTarget = targetPos - currentPos;
        float dist = toTarget.Length();

        if (dist < 0.001f)
            return;

        Vec3 dir = toTarget.Normalized();

        const float stiffness = 120.0f;
        const float damping = 8.0f;
        const float maxForce = 300.0f;

        Vec3 force = dir * (stiffness * dist);

        if (GetRigidbodyRef(currentlyGrabbing)) {
            Vec3 vel = RB_GetVelocity(currentlyGrabbing);
            force -= vel * damping;
        }

        float forceLen = force.Length();
        if (forceLen > maxForce) {
            force *= (maxForce / forceLen);
        }

        if (GetRigidbodyRef(currentlyGrabbing)) {
            //LOG_DEBUG("this uses add_force");

            RB_AddForce(force, currentlyGrabbing);
        }
    }

    void s()
    {
        if (isGrabbing)
        {
            //LOG_DEBUG("UPDATING GRABBED OBJECT");
            TransformRef t = GetTransformRef(GetEntity());
            TransformRef grabbedT = GetTransformRef(currentlyGrabbing);
            targetPosition = TF_GetPosition(t) + TF_GetForward(t) * distance;

            Vec3 displacement = targetPosition - TF_GetPosition(grabbedT);
            

            if (grabbedIsHeavy)
            {
                displacement.y = 0;
            }

            Vec3 force = displacement * grabStrength;
            Vec3 damper = RB_GetVelocity(currentlyGrabbing) * damping * -1.0f;

            RB_AddForce(force, currentlyGrabbing);
        }
    }

    bool IsGrabbing()
    {
        return isGrabbing;
    }

    Entity GetCurrentlyGrabbing() const
    {
        return currentlyGrabbing;
    }

    void Grab(Entity object, bool heavy, bool pressurePlates)
    {
        currentlyGrabbing = object;
        isGrabbing = true;

        timer = timerBuffer;
        RB_LockRotation(true, false, true, currentlyGrabbing);
        RB_SetUseGravity(false, currentlyGrabbing);

        grabbedIsHeavy = heavy;
        grabbedActivatesPressurePlates = pressurePlates;

        //tether
        //play sound
        GameObject obj(object);
        std::string grabbedMessage = "NAME OF GRABBED ENTITY: " + std::to_string(object);
        LOG_DEBUG(grabbedMessage);
    }
    
    void LetGo()
    {
        LOG_DEBUG("LETTING GO OF OBJECT");
        isGrabbing = false;

        RB_LockRotation(false, false, false, currentlyGrabbing);
        RB_SetUseGravity(true, currentlyGrabbing);

        RB_SetVelocity(Vec3(0, 0, 0), currentlyGrabbing);
        currentlyGrabbing = NULL;
    }

private:
    // no instance cause whats singleton lmao
    float distance;
    float grabStrength;
    float damping;
    float timerBuffer;

    Vec3 targetPosition;
    Entity currentlyGrabbing;
    bool isGrabbing = false;

    float defaultDrag;
    float defaultAngularDrag;
    float timer;
    bool grabbedIsHeavy = false;
    bool grabbedActivatesPressurePlates = false;

};