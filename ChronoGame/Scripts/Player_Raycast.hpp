#pragma once
#include "EngineAPI.hpp"
#include "Highlightable_.hpp"
#include "Interactable_.hpp"
#include "Player_Controller.hpp"

#define GLFW_MOUSE_BUTTON_LEFT 0
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Player_Raycast is responsible for point and click interactions.
* It grabs the raycast direction from the player controller and
* looks for Interactable_ scripts in the hit.
*/

class Player_Raycast : public IScript {
public:
    Player_Raycast() {}
    ~Player_Raycast() override = default;

    // === Custom Methods ===
    void NoInteract()
    {
        if (storedEntity)
        {
            GameObject storedObject = GameObject(storedEntity);
            Highlightable_* h = storedObject.GetComponent<Highlightable_>();
            if (h) { h->SetHighlight(false); }
            storedEntity = 0;
        }
    }

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {
        auto v = GameObject::FindObjectsOfType<Player_Controller>();
        if (v.size() == 0) {
            LOG_ERROR("No player controllers found!");
        }
        else if (v.size() > 1) {
            LOG_WARNING("Multiple player controllers found!");
        }
        else {
            playerController = v.begin()->GetComponent<Player_Controller>();
        }
    }
    void Update(double deltaTime) override {

        // === Raycast Interval ===
        timer += static_cast<float>(deltaTime);
        if (timer > interval)
        {
            timer = 0.0f;
            Vec3 origin = TF_GetWorldPosition(cameraEntity);
            Vec3 direction = playerController->GetRaycastForward();
            RaycastHit raycastHit = Raycast(
                origin, 
                direction, 
                distance, 
                targetLayer.ToMask());

            // Once we hit something, check for interactable and store it
            if (raycastHit.hasHit)
            {
                GameObject go = GameObject(raycastHit.entity);
                Highlightable_* h = go.GetComponent<Highlightable_>();

                // Only proceed if Highlightable component exists and we are hitting another entity
                if (h && raycastHit.entity != storedEntity)
                {
                    // By virtue of only storing entities with Highlightable component,
                    // we can safe assume that the storedEntity already has one
                    GameObject(storedEntity).GetComponent<Highlightable_>()->SetHighlight(false);

                    // Then we can set Highlight and store
                    h->SetHighlight(true);
                    storedEntity = raycastHit.entity;
                }
                else
                {
                    NoInteract();
                }
            }
            else
            {
                NoInteract();
            }
        }

        if (storedEntity)
        {
            if (Input::WasKeyPressed(GLFW_MOUSE_BUTTON_LEFT))
            {
                GameObject storedObject = GameObject(storedEntity);
                Interactable_* i = storedObject.GetComponent<Interactable_>();
                if (i) { i->Interact(); }
            }
        }
    }
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Player_Raycast"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    float interval;
    float distance;
    LayerRef targetLayer;

    float timer;
    Entity storedEntity;

    Entity cameraEntity;
    Player_Controller* playerController;
};