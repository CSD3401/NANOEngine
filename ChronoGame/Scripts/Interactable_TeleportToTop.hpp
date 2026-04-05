#pragma once
#include <algorithm>
#include <cmath>
#include "EngineAPI.hpp"
#include "Interactable_.hpp"
#include "Player_Controller.hpp"

/*
* Smoothly moves the player to the *top* of this object's collider when interacted with.
*
* Usage:
*  - Attach this script to any object that has a Collider component.
*  - Ensure the object is detectable by Player_Raycast (same layer mask).
*
* By default, this script auto-finds the player by looking for an entity with
* Player_Controller. You can also assign the player manually via the inspector.
*
* Interaction:
*  - Default: Player_Raycast + **left click** on the object (Interact()).
*  - Optional: enable `useSpaceKeyInsteadOfMouse` — press **Space** when within
*    `spaceKeyMaxDistance` (raycast click is ignored for this object).
*/

#ifndef GLFW_KEY_SPACE
#define GLFW_KEY_SPACE 32
#endif

class Interactable_TeleportToTop : public Interactable_ {
public:
    Interactable_TeleportToTop()
        : extraClearanceY(0.15f)
        , assumePlayerPivotAtCenter(true)
        , snapXZToObject(true)
        , liftSpeedY(3.5f)
        , useCharacterControllerMove(true)
        , isLifting(false)
        , targetY(0.0f)
        , cachedPlayerControllerWasEnabled(true)
    {
        SCRIPT_GAMEOBJECT_REF(player);
        SCRIPT_FIELD(extraClearanceY, Float);
        SCRIPT_FIELD(assumePlayerPivotAtCenter, Bool);
        SCRIPT_FIELD(snapXZToObject, Bool);
        SCRIPT_FIELD(liftSpeedY, Float);
        SCRIPT_FIELD(useCharacterControllerMove, Bool);
        SCRIPT_FIELD(climbAudioName, String);
        SCRIPT_FIELD(useSpaceKeyInsteadOfMouse, Bool);
        SCRIPT_FIELD(spaceKeyMaxDistance, Float);
    }

    ~Interactable_TeleportToTop() override = default;

    // === Interactable_ ===
    void Interact() override
    {
        if (useSpaceKeyInsteadOfMouse)
            return;

        TryStartLift();
    }

    void TryStartLift()
    {
        // If we're already in the middle of a lift, ignore additional clicks.
        if (isLifting)
            return;

        ResolvePlayerRef();
        if (!player.IsValid())
        {
            LOG_ERROR("Interactable_TeleportToTop: Player reference invalid (could not find Player_Controller).");
            return;
        }

        const Entity targetEntity = GetEntity();

        if (!Query::HasCollider(targetEntity))
        {
            LOG_WARNING("Interactable_TeleportToTop: Target has no Collider component.");
            return;
        }

        // === Compute top of *this* collider (world Y) ===
        const auto& col = Query::GetEntityCollider(targetEntity);

        const Vec3 objPos = TF_GetPosition(targetEntity);
        const Vec3 objScale = TF_GetScale(targetEntity);
        const float objHalfHeight = ComputeColliderHalfHeightWorld(col, objScale);
        const float topY = objPos.y + objHalfHeight;

        // === Compute how much to lift player so they stand on the top surface ===
        float playerLift = 0.0f;
        if (assumePlayerPivotAtCenter)
        {
            playerLift = ComputePlayerHalfHeightWorld();
        }

        // === Prepare the lift ===
        const Entity playerEntity = player.GetEntity();

        // Optionally snap the player horizontally to the object first (so the lift lands on top).
        if (snapXZToObject)
        {
            Vec3 snapPos = TF_GetPosition(playerEntity);
            snapPos.x = objPos.x;
            snapPos.z = objPos.z;
            TF_SetPosition(snapPos, playerEntity);
        }

        targetY = topY + playerLift + extraClearanceY;
        isLifting = true;

        // Play climb audio
        if (!climbAudioName.empty())
            PlayAudio("event:/" + climbAudioName);

        // Temporarily disable the player controller so it doesn't fight us (CC_Move / gravity / input).
        CacheAndDisablePlayerController();
    }

    bool IsPlayerWithinInteractRange() const
    {
        if (!player.IsValid())
            return false;
        const Vec3 p = TF_GetPosition(player.GetEntity());
        const Vec3 o = TF_GetPosition(GetEntity());
        const float dx = p.x - o.x;
        const float dy = p.y - o.y;
        const float dz = p.z - o.z;
        const float distSq = dx * dx + dy * dy + dz * dz;
        const float r = spaceKeyMaxDistance > 0.0f ? spaceKeyMaxDistance : 3.0f;
        return distSq <= r * r;
    }

    // === Lifecycle ===
    void Awake() override {}
    void Initialize(Entity entity) override { (void)entity; }
    void Start() override {}
    void Update(double deltaTime) override
    {
        if (useSpaceKeyInsteadOfMouse && !isLifting && Input::WasKeyPressed(GLFW_KEY_SPACE))
        {
            ResolvePlayerRef();
            if (player.IsValid() && IsPlayerWithinInteractRange())
                TryStartLift();
        }

        if (!isLifting)
            return;

        if (!player.IsValid())
        {
            isLifting = false;
            RestorePlayerController();
            return;
        }

        const Entity playerEntity = player.GetEntity();
        Vec3 currentPos = TF_GetPosition(playerEntity);
        const float remaining = targetY - currentPos.y;

        // Close enough → finish.
        if (remaining <= 0.001f)
        {
            currentPos.y = targetY;
            TF_SetPosition(currentPos, playerEntity);
            isLifting = false;

            // Stop climb audio
            if (!climbAudioName.empty())
                StopAudio("event:/" + climbAudioName);

            RestorePlayerController();
            return;
        }

        const float dt = static_cast<float>(deltaTime);
        const float step = std::max(0.0f, std::min(liftSpeedY * dt, remaining));

        if (useCharacterControllerMove)
        {
            // Use CharacterController move to respect collisions.
            CC_Move(Vec3{ 0.0f, step, 0.0f }, playerEntity);
        }
        else
        {
            // Directly adjust transform.
            currentPos.y += step;
            TF_SetPosition(currentPos, playerEntity);
        }
    }
    void OnDestroy() override {}

    void OnEnable() override
    {
        // Try to auto-resolve if not assigned.
        ResolvePlayerRef();
    }

    void OnDisable() override
    {
        if (!isLifting)
            return;
        isLifting = false;
        if (!climbAudioName.empty())
            StopAudio("event:/" + climbAudioName);
        RestorePlayerController();
    }
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Interactable_TeleportToTop"; }

private:
    // === Inspector Fields ===
    GameObjectRef player;
    float extraClearanceY;
    bool assumePlayerPivotAtCenter;
    bool snapXZToObject;
    float liftSpeedY;
    bool useCharacterControllerMove;

    // === Inspector Fields (audio) ===
    std::string climbAudioName = "CLIMB_METAL";

    bool useSpaceKeyInsteadOfMouse = false;
    float spaceKeyMaxDistance = 3.0f;

    // === Runtime State ===
    bool isLifting;
    float targetY;
    bool cachedPlayerControllerWasEnabled;

    void CacheAndDisablePlayerController()
    {
        cachedPlayerControllerWasEnabled = true;

        GameObject playerGO(player.GetEntity());
        if (!playerGO.IsValid())
            return;

        Player_Controller* pc = playerGO.GetComponent<Player_Controller>();
        if (!pc)
            return;

        cachedPlayerControllerWasEnabled = pc->IsEnabled();
        pc->ResetMovementOnly();
        pc->SetEnabled(false);
    }

    void RestorePlayerController()
    {
        if (!player.IsValid())
            return;
        GameObject playerGO(player.GetEntity());
        if (!playerGO.IsValid())
            return;
        Player_Controller* pc = playerGO.GetComponent<Player_Controller>();
        if (pc)
        {
            pc->ResetMovementOnly();
            pc->SetEnabled(cachedPlayerControllerWasEnabled);
        }
    }

    // === Helpers ===
    void ResolvePlayerRef()
    {
        if (player.IsValid())
            return;

        auto players = GameObject::FindObjectsOfType<Player_Controller>();
        if (players.size() == 0)
        {
            return;
        }

        if (players.size() > 1)
        {
            LOG_WARNING("Interactable_TeleportToTop: Multiple Player_Controller found; using the first one.");
        }

        player.SetEntity(players.begin()->GetEntityId());
    }

    static float AbsMax3(float a, float b, float c)
    {
        return std::max(std::fabs(a), std::max(std::fabs(b), std::fabs(c)));
    }

    static float ComputeColliderHalfHeightWorld(const NE::ECS::Component::Collider& col, const Vec3& worldScale)
    {
        const float sy = std::fabs(worldScale.y);
        const float sMax = AbsMax3(worldScale.x, worldScale.y, worldScale.z);

        using Shape = NE::ECS::Component::Collider::ShapeType;
        switch (col.shapeType)
        {
        case Shape::Box:
            return std::fabs(col.halfExtents.y) * sy;
        case Shape::Sphere:
            // Sphere radius should scale by the largest axis to stay conservative.
            return std::fabs(col.radius) * sMax;
        case Shape::Capsule:
            // Assume "height" is total capsule height (Unity-style): top from center is height/2.
            return std::fabs(col.height) * 0.5f * sy;
        case Shape::Mesh:
            // Fallback: use halfExtents if available.
            return std::fabs(col.halfExtents.y) * sy;
        case Shape::None:
        default:
            return 0.0f;
        }
    }

    float ComputePlayerHalfHeightWorld() const
    {
        const Entity playerEntity = player.GetEntity();

        if (!Query::HasCollider(playerEntity))
        {
            // If the player has no collider component, just don't apply any extra lift.
            return 0.0f;
        }

        const auto& col = Query::GetEntityCollider(playerEntity);
        const Vec3 scale = TF_GetScale(playerEntity);
        return ComputeColliderHalfHeightWorld(col, scale);
    }
};