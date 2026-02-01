/**
 * @file ECS.h
 * @brief SDK-level ECS (Entity Component System) interface
 *
 * This header provides access to ECS operations for scripts.
 * All functions are exported from NANOEngine.dll.
 *
 * IMPORTANT: This header is STANDALONE - no internal engine includes.
 * Declarations are duplicated from the engine and must be kept in sync.
 */

#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>
#include <typeindex>

// Forward declarations for components
namespace NE {
    namespace ECS {
        namespace Component {
            struct Transform;
            struct Renderer;
            struct Light;
            struct Rigidbody;
            struct Collider;
            struct EntityMeta;
            struct AudioSource;
            struct NativeScript;
            struct Animator;
            struct Camera;
            struct PhysicsBody;
            // UI Components
            struct UICanvas;
            struct UIRectTransform;
            struct UIImage;
        }
    }
}

namespace NE {
namespace ECS {

    //=========================================================================
    // ENTITY TYPE AND CONSTANTS
    //=========================================================================

    using Entity = uint32_t;
    static constexpr Entity MAX_ENTITIES = 2000;
    static constexpr Entity NO_ENTITY = 0xFFFFFFFF;  // std::numeric_limits<uint32_t>::max()

    //=========================================================================
    // QUERY NAMESPACE - Read-only component access
    //=========================================================================

    namespace Query {

        // Component handling
        __declspec(dllimport) uint64_t GetEntitySignature(uint32_t e);

        // Component getters (const access)
        __declspec(dllimport) const Component::EntityMeta& GetEntityMeta(uint32_t e);
        __declspec(dllimport) const Component::Transform& GetEntityTransform(uint32_t e);
        __declspec(dllimport) const Component::Renderer& GetEntityRenderer(uint32_t e);
        __declspec(dllimport) const Component::Light& GetEntityLight(uint32_t e);
        __declspec(dllimport) const Component::Rigidbody& GetEntityRigidbody(uint32_t e);
        __declspec(dllimport) const Component::Collider& GetEntityCollider(uint32_t e);
        __declspec(dllimport) const Component::AudioSource& GetEntityAudioSource(uint32_t e);
        __declspec(dllimport) const Component::NativeScript& GetEntityScript(uint32_t e);
        __declspec(dllimport) const Component::Animator& GetEntityAnimator(uint32_t e);
        __declspec(dllimport) const Component::Camera& GetEntityCamera(uint32_t e);

        // Component existence checks
        __declspec(dllimport) bool HasTransform(uint32_t e);
        __declspec(dllimport) bool HasRenderer(uint32_t e);
        __declspec(dllimport) bool HasLight(uint32_t e);
        __declspec(dllimport) bool HasRigidbody(uint32_t e);
        __declspec(dllimport) bool HasCollider(uint32_t e);
        __declspec(dllimport) bool HasAudioSource(uint32_t e);
        __declspec(dllimport) bool HasScript(uint32_t e);
        __declspec(dllimport) bool HasAnimator(uint32_t e);
        __declspec(dllimport) bool HasCamera(uint32_t e);
    }

    //=========================================================================
    // COMMAND NAMESPACE - Mutable component access
    //=========================================================================

    namespace Command {
        // Entity lifecycle
        __declspec(dllimport) uint32_t CreateEntity();
        __declspec(dllimport) void DestroyEntity(uint32_t e);

        // Component addition
        __declspec(dllimport) void AddLightComponent(uint32_t e);
        __declspec(dllimport) void AddRendererComponent(uint32_t e);
        __declspec(dllimport) void AddRigidbodyComponent(uint32_t e);
        __declspec(dllimport) void AddColliderComponent(uint32_t e);
        __declspec(dllimport) void AddAudioSourceComponent(uint32_t e);
        __declspec(dllimport) void AddScriptComponent(uint32_t e);
        __declspec(dllimport) void AddCameraComponent(uint32_t e);
        __declspec(dllimport) void AddAnimatorComponent(uint32_t e);

        // Component getters (mutable access)
        __declspec(dllimport) Component::EntityMeta& GetEntityMeta(uint32_t e);
        __declspec(dllimport) Component::Transform& GetEntityTransform(uint32_t e);
        __declspec(dllimport) Component::Renderer& GetEntityRenderer(uint32_t e);
        __declspec(dllimport) Component::Light& GetEntityLight(uint32_t e);
        __declspec(dllimport) Component::Rigidbody& GetEntityRigidbody(uint32_t e);
        __declspec(dllimport) Component::Collider& GetEntityCollider(uint32_t e);
        __declspec(dllimport) Component::AudioSource& GetEntityAudioSource(uint32_t e);
        __declspec(dllimport) Component::NativeScript& GetEntityScript(uint32_t e);
        __declspec(dllimport) Component::Camera& GetEntityCamera(uint32_t e);
        __declspec(dllimport) Component::Animator& GetEntityAnimator(uint32_t e);

        // Script management
        __declspec(dllimport) std::vector<std::string> GetRegisteredScriptNames();
        __declspec(dllimport) bool SetEntityScript(uint32_t e, const std::string& scriptName);
        __declspec(dllimport) void RemoveEntityScript(uint32_t e);
        __declspec(dllimport) bool IsScriptRegistered(const std::string& scriptName);
    }

} // namespace ECS
} // namespace NE
