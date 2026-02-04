#pragma once
#include "EngineAPI.hpp"
#include "Highlightable_.hpp"
#include "Interactable_.hpp"
#include "Player_Controller.hpp"

#define GLFW_MOUSE_BUTTON_LEFT 0
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Player_Raycast is responsible for point and click interactions.
* It needs to be attached to the player's camera to function properly.
*/

class Player_Raycast : public IScript {
public:
    Player_Raycast() :
        interval{ 0.1f },
        distance{ 5.0f },
        targetLayer{ 0 },
        timer{ 0.0f },
        storedHighlightable{ nullptr },
        storedInteractable{ nullptr }
    {
        SCRIPT_FIELD(interval, Float);
        SCRIPT_FIELD(distance, Float);
        SCRIPT_FIELD_LAYERREF(targetLayer);
    }
    ~Player_Raycast() override = default;

    // === Custom Methods ===
    void NoInteract()
    {
        if (storedHighlightable)
        {
            storedHighlightable->SetHighlight(false);
            storedHighlightable = nullptr;
        }

        if (storedInteractable)
        {
            storedInteractable = nullptr;
        }
    }

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {}

    void Update(double deltaTime) override {

        // === Raycast Interval ===
        timer += static_cast<float>(deltaTime);
        if (timer > interval)
        {
            timer = 0.0f;
            Vec3 origin = TF_GetPosition();
            Vec3 direction = TF_GetForward();
            RaycastHit raycastHit = Raycast(
                origin,
                direction,
                distance,
                targetLayer.ToMask());

            // Once we hit something, check for interactable and store it
            if (raycastHit.hasHit)
            {
                GameObject go = GameObject(raycastHit.entity);

                if (!go.IsValid())
                {
                    return;
                }
                Highlightable_* h = go.GetComponent<Highlightable_>();
                Interactable_* i = go.GetComponent<Interactable_>();

                // Only proceed if Highlightable component exists
                if (h)
                {
                    // Un-highlight previous highlightable if different
                    if (h != storedHighlightable)
                    {
                        if (storedHighlightable)
                        {
                            storedHighlightable->SetHighlight(false);
                        }

                        // Then we can set Highlight and store
                        h->SetHighlight(true);
                        storedHighlightable = h;
                    }
                }
                else
                {
                    NoInteract();
                }

                // Store if interactable exists
                if (i)
                {
                    storedInteractable = i;
                }
                else
                {
                    storedInteractable = nullptr;
                }
            }
            else // Raycast hit nothing
            {
                NoInteract();
            }
        }

        if (storedInteractable && Input::WasMousePressed(GLFW_MOUSE_BUTTON_LEFT))
        {
            LOG_DEBUG("Interacting with interactable");
            storedInteractable->Interact();
        }
    }
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Player_Raycast"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    // === Inspector Fields ===
    float interval;
    float distance;
    LayerRef targetLayer;

    // === Private Fields ===
    float timer;
    Highlightable_* storedHighlightable;
    Interactable_* storedInteractable;
};