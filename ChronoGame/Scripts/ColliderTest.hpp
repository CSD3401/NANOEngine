#pragma once
#include "EngineAPI.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class ColliderTest : public IScript {
public:
    ColliderTest() {
        // Register any editable fields here
        // Example: SCRIPT_FIELD(speed, float);
        // Example: SCRIPT_FIELD_VECTOR(blingstring, String);;
    }

    ~ColliderTest() override = default;

    // === Lifecycle Methods ===

    void Awake() override {
        // Called when the script component is first created
    }

    void Initialize(Entity entity) override {
        // Called to initialize the script with its entity
    }

    void Start() override {
        // Called when the script is enabled and play mode starts
    }

    void Update(double deltaTime) override {

        float moveSpeed = 5.f;
        float jumpForce = 5.f;
        // Update state based on input
         // === Movement controls ===
        // Input Direction
        // Movement
        Vec3 velocity = RB_GetVelocity();
        bool isGrounded = CC_IsGrounded();

        if (Input::IsKeyDown('A')) { // A 
            velocity.x = -moveSpeed;
        }
        else if (Input::IsKeyDown('D')) { // D 
            velocity.x = moveSpeed;
        }
        else {
            velocity.x = 0;
        }

        RB_SetVelocity(velocity);

        // Jump (only if grounded)
        static bool wasJumpKeyDown = false;
        bool isJumpKeyDown = Input::IsKeyDown(32);
        if (isGrounded && isJumpKeyDown && !wasJumpKeyDown)
        {
            RB_AddImpulse(0.0f, jumpForce, 0.0f);
        }
           
    }

    void OnDestroy() override {
        // Called when the script is about to be destroyed
    }

    // === Optional Callbacks ===

    void OnEnable() override {
        // Called when the script is enabled
    }

    void OnDisable() override {
        // Called when the script is disabled
    }

    void OnValidate() override {
        // Called when a field value is changed in the editor
    }

    const char* GetTypeName() const override {
        return "Example_Template";
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override {
        printf("[COLLISION ENTER] Entity %d collided with Entity %d\n", GetEntity(), other);
    }

    void OnCollisionExit(Entity other) override {
        printf("[COLLISION EXIT] Entity %d stopped colliding with Entity %d\n", GetEntity(), other);
    }

    void OnTriggerEnter(Entity other) override {
        printf("[TRIGGER ENTER] Entity %d entered trigger of Entity %d\n", GetEntity(), other);
    }

    void OnTriggerExit(Entity other) override {
        printf("[TRIGGER EXIT] Entity %d exited trigger of Entity %d\n", GetEntity(), other);
    }

private:
    // Add your private member variables here
    // Example: float speed = 5.0f;

};