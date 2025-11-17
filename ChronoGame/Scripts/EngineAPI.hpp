#pragma once

/**
 * @file EngineAPI.hpp
 * @brief Compatibility header providing access to common engine features for scripts
 *
 * This header includes commonly needed engine functionality that is not part of the
 * core ScriptSDK but is often needed by game scripts (logging, input, events, etc.)
 */

// Core SDK - Always include this first
#include <ScriptSDK/ScriptAPI.h>

// ============ LOGGING AND COROUTINES (SDK COMPATIBILITY LAYER) ============
// Provide backward-compatible macros that use the SDK logging functions
// The SDK functions (SCRIPT_LOG_*) are available from ScriptAPI.h

// Backward-compatible aliases for logging macros
#define SPD_DEBUG(...)    SCRIPT_LOG_DEBUG(__VA_ARGS__)
#define SPD_INFO(...)     SCRIPT_LOG_INFO(__VA_ARGS__)
#define SPD_WARNING(...)  SCRIPT_LOG_WARNING(__VA_ARGS__)
#define SPD_ERROR(...)    SCRIPT_LOG_ERROR(__VA_ARGS__)
#define SPD_CRITICAL(...) SCRIPT_LOG_CRITICAL(__VA_ARGS__)

// Backward-compatible coroutine function names
namespace {
    inline NE::Scripting::CoroutineHandle Engine_CreateCoroutine() {
        return NE::Scripting::CreateCoroutine();
    }

    template<typename Func>
    inline void Engine_AddAction(NE::Scripting::CoroutineHandle handle, Func&& func) {
        NE::Scripting::AddCoroutineAction(handle, std::function<void()>(std::forward<Func>(func)));
    }

    inline void Engine_AddWaitForSeconds(NE::Scripting::CoroutineHandle handle, float seconds) {
        NE::Scripting::AddCoroutineWait(handle, seconds);
    }

    inline void Engine_StartCoroutine(NE::Scripting::CoroutineHandle handle) {
        NE::Scripting::StartCoroutine(handle);
    }
}

// ============ ENGINE INTERFACE ============
// Include necessary engine headers for component definitions and DLL exports
// Scripts only need to include EngineAPI.hpp - this header handles all engine dependencies
#include "../../NANOEngine/src/Math/Vec3.hpp"

// Note: Vec3 conversion operators are implemented in Vec3.cpp and ScriptTypes.cpp
// They are automatically exported/imported via NANOENGINE_API and SCRIPT_API

#include "../../NANOEngine/src/ECS/Components/Transform.hpp"
#include "../../NANOEngine/src/ECS/Components/Light.hpp"
#include "../../NANOEngine/src/ECS/Components/Collider.hpp"
#include "../../NANOEngine/src/EditorInterface/ECSExports.hpp"
#include "../../NANOEngine/src/EditorInterface/RendererExports.hpp"

// Note: NE::ECS::Command and NE::ECS::Query are now available from ECSExports.hpp
// Note: NE::Renderer::Command is now available from RendererExports.hpp

// ============ TEMPLATE WRAPPERS FOR ECS ============
// Convenience template functions for HasComponent and GetComponent
namespace NE {
    namespace ECS {
        namespace Query {
            template<typename T>
            inline bool HasComponent(uint32_t entity) {
                if constexpr (std::is_same_v<T, Component::Transform>) {
                    return HasTransform(entity);
                } else if constexpr (std::is_same_v<T, Component::Renderer>) {
                    return HasRenderer(entity);
                } else if constexpr (std::is_same_v<T, Component::Light>) {
                    return HasLight(entity);
                } else if constexpr (std::is_same_v<T, Component::Rigidbody>) {
                    return HasRigidbody(entity);
                } else if constexpr (std::is_same_v<T, Component::Collider>) {
                    return HasCollider(entity);
                } else if constexpr (std::is_same_v<T, Component::AudioSource>) {
                    return HasAudioSource(entity);
                } else if constexpr (std::is_same_v<T, Component::NativeScript>) {
                    return HasScript(entity);
                } else if constexpr (std::is_same_v<T, Component::Animator>) {
                    return HasAnimator(entity);
                } else if constexpr (std::is_same_v<T, Component::Camera>) {
                    return HasCamera(entity);
                }
                return false;
            }
        }

        namespace Command {
            template<typename T>
            inline bool HasComponent(uint32_t entity) {
                // Delegate to Query namespace for read-only checks
                return Query::HasComponent<T>(entity);
            }

            template<typename T>
            inline T& GetComponent(uint32_t entity) {
                if constexpr (std::is_same_v<T, Component::Transform>) {
                    return GetEntityTransform(entity);
                } else if constexpr (std::is_same_v<T, Component::Light>) {
                    return GetEntityLight(entity);
                } else if constexpr (std::is_same_v<T, Component::Collider>) {
                    return GetEntityCollider(entity);
                } else if constexpr (std::is_same_v<T, Component::Rigidbody>) {
                    return GetEntityRigidbody(entity);
                } else if constexpr (std::is_same_v<T, Component::Renderer>) {
                    return GetEntityRenderer(entity);
                } else if constexpr (std::is_same_v<T, Component::AudioSource>) {
                    return GetEntityAudioSource(entity);
                } else if constexpr (std::is_same_v<T, Component::NativeScript>) {
                    return GetEntityScript(entity);
                } else if constexpr (std::is_same_v<T, Component::Animator>) {
                    return GetEntityAnimator(entity);
                } else if constexpr (std::is_same_v<T, Component::Camera>) {
                    return GetEntityCamera(entity);
                }
            }
        }
    }
}

// ============ ENGINE FEATURES ============
// Input handling
#include "../../NANOEngine/src/Input/InputManager.hpp"

// Event system
#include "../../NANOEngine/src/Events/EventBus.hpp"

// Reflection system (for custom structs)
#include "../../NANOEngine/src/Core/Reflection.hpp"

// Exposed field registry (for advanced editor integration)
#include "../ExposedFieldRegistry.hpp"

// ============ SCRIPT FIELD MACROS ============
// Note: SCRIPT_FIELD is a legacy macro that is now a no-op for SDK scripts.
// Field registration is handled automatically by the engine's reflection system
// or can be done manually using ExposedFieldRegistry if needed.
//
// Field type tokens (kept for backward compatibility)
namespace ScriptFieldType {
    struct Float {};
    struct Int {};
    struct Bool {};
    struct String {};
    struct Vec3 {};
}

// Convenience aliases at global scope (for backward compatibility)
using Float = ScriptFieldType::Float;
using Int = ScriptFieldType::Int;
using Bool = ScriptFieldType::Bool;
using String = ScriptFieldType::String;
// Note: Vec3 type alias is in ChronoGame namespace below

// SCRIPT_FIELD macro - currently a no-op for SDK scripts
// Usage: SCRIPT_FIELD(myFloatField, Float)
#define SCRIPT_FIELD(fieldName, fieldType) \
    ((void)0)  // No-op: field registration is handled by reflection system

// SCRIPT_COMPONENT_REF macro - currently a no-op for SDK scripts
// Usage: SCRIPT_COMPONENT_REF(targetTransform, Transform)
#define SCRIPT_COMPONENT_REF(refName, componentType) \
    ((void)0)  // No-op: component references are managed automatically

// Type aliases for convenience
namespace ChronoGame {
    // Alias the SDK types into ChronoGame namespace for easier use
    using Vec3 = NE::Scripting::Vec3;
    using Entity = NE::Scripting::Entity;
    using IScript = NE::Scripting::IScript;
    using RaycastHit = NE::Scripting::RaycastHit;

    // Component reference types
    using TransformRef = NE::Scripting::TransformRef;
    using RigidbodyRef = NE::Scripting::RigidbodyRef;
    using AudioSourceRef = NE::Scripting::AudioSourceRef;
}
