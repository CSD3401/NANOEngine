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
    }
    ~Misc_Grabber() override = default;

    // == Custom Methods ==
    virtual void Interact()
    {
        GameObjectRef obj;
    }

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override
    {
    }
    void Update(double deltaTime) override 
    {
        if (IsGrabbing() && Input::WasKeyReleased(GLFW_MOUSE_BUTTON_LEFT))
        {
            LetGo();
        }

        UpdateGrabbedObject();
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

    void UpdateGrabbedObject()
    {
        if (isGrabbing)
        {
            LOG_DEBUG("UPDATING GRABBED OBJECT");
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

    void Grab(Entity object, bool heavy, bool pressurePlates)
    {
        currentlyGrabbing = object;
        isGrabbing = true;

        timer = timerBuffer;
        RB_LockRotation(true, false, true, currentlyGrabbing);
        RB_SetUseGravity(false);

        grabbedIsHeavy = heavy;
        grabbedActivatesPressurePlates = pressurePlates;

        //tether
        //play sound
    }
    
    void LetGo()
    {
        isGrabbing = false;

        RB_LockRotation(false, false, false, currentlyGrabbing);

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