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

// ============ ENGINE INTERFACE (ALL FROM SDK) ============
// Scripts only need to include EngineAPI.hpp - this header handles all dependencies
// All includes are now from ScriptSDK/ - no internal engine paths!

// Math types (needed for component structures and Vec3 conversions)
#include <ScriptSDK/Math.h>

// Note: Vec3 conversion operators are implemented in Vec3.cpp and ScriptTypes.cpp
// They are automatically exported/imported via NANOENGINE_API and SCRIPT_API

// Component definitions (Transform, Light, Collider)
#include <ScriptSDK/Components.h>

// ECS and Renderer API (Command and Query functions)
#include <ScriptSDK/ECS.h>
#include <ScriptSDK/Renderer.h>

// SDK HEADERS LOADED:
// - Math.h → Math::Vec3, Math::Mat4 (with Scripting::Vec3 conversions)
// - Components.h → Transform, Light, Collider component types
// - ECS.h → NE::ECS::Command, NE::ECS::Query functions
// - Renderer.h → NE::Renderer::Command functions

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

// ============ ENGINE FEATURES (SDK COMPATIBILITY LAYER) ============

// Reflection system (header-only, now in SDK)
#include <ScriptSDK/Reflection.h>

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
