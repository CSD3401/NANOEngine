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

// UI components (UICanvas, UIRectTransform, UIImage, UIText, UIButton, UISlider, UIToggle, UIInputField, UIDropdown)
#include <ScriptSDK/UI.h>

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
                } else if constexpr (std::is_same_v<T, Component::UICanvas>) {
                    return NE::ECS::Query::HasUICanvas(entity);
                } else if constexpr (std::is_same_v<T, Component::UIRectTransform>) {
                    return NE::ECS::Query::HasUIRectTransform(entity);
                } else if constexpr (std::is_same_v<T, Component::UIImage>) {
                    return NE::ECS::Query::HasUIImage(entity);
                } else if constexpr (std::is_same_v<T, Component::UIText>) {
                    return NE::ECS::Query::HasUIText(entity);
                } else if constexpr (std::is_same_v<T, Component::UIButton>) {
                    return NE::ECS::Query::HasUIButton(entity);
                } else if constexpr (std::is_same_v<T, Component::UISlider>) {
                    return NE::ECS::Query::HasUISlider(entity);
                } else if constexpr (std::is_same_v<T, Component::UIToggle>) {
                    return NE::ECS::Query::HasUIToggle(entity);
                } else if constexpr (std::is_same_v<T, Component::UIInputField>) {
                    return NE::ECS::Query::HasUIInputField(entity);
                } else if constexpr (std::is_same_v<T, Component::UIDropdown>) {
                    return NE::ECS::Query::HasUIDropdown(entity);
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
                } else if constexpr (std::is_same_v<T, Component::UICanvas>) {
                    return NE::ECS::Command::GetUICanvas(entity);
                } else if constexpr (std::is_same_v<T, Component::UIRectTransform>) {
                    return NE::ECS::Command::GetUIRectTransform(entity);
                } else if constexpr (std::is_same_v<T, Component::UIImage>) {
                    return NE::ECS::Command::GetUIImage(entity);
                } else if constexpr (std::is_same_v<T, Component::UIText>) {
                    return NE::ECS::Command::GetUIText(entity);
                } else if constexpr (std::is_same_v<T, Component::UIButton>) {
                    return NE::ECS::Command::GetUIButton(entity);
                } else if constexpr (std::is_same_v<T, Component::UISlider>) {
                    return NE::ECS::Command::GetUISlider(entity);
                } else if constexpr (std::is_same_v<T, Component::UIToggle>) {
                    return NE::ECS::Command::GetUIToggle(entity);
                } else if constexpr (std::is_same_v<T, Component::UIInputField>) {
                    return NE::ECS::Command::GetUIInputField(entity);
                } else if constexpr (std::is_same_v<T, Component::UIDropdown>) {
                    return NE::ECS::Command::GetUIDropdown(entity);
                }
            }
        }
    }
}

// ============ ENGINE FEATURES (SDK COMPATIBILITY LAYER) ============

// Reflection system (header-only, now in SDK)
#include <ScriptSDK/Reflection.h>

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

// SCRIPT_FIELD macro - registers fields for editor exposure
// Usage: SCRIPT_FIELD(myFloatField, Float)
#define SCRIPT_FIELD(fieldName, fieldType) \
    Register##fieldType##Field(#fieldName, &this->fieldName)

// SCRIPT_COMPONENT_REF macro - registers component references for editor exposure
// Usage: SCRIPT_COMPONENT_REF(targetTransform, Transform)
#define SCRIPT_COMPONENT_REF(refName, componentType) \
    Register##componentType##Field(#refName, &this->refName)

// SCRIPT_PREFAB_REF macro - registers prefab references for editor exposure
// Usage: SCRIPT_PREFAB_REF(enemyPrefab)
#define SCRIPT_PREFAB_REF(refName) \
    RegisterPrefabRefField(#refName, &this->refName)

// SCRIPT_GAMEOBJECT_REF macro - registers GameObject references for editor exposure
// Usage: SCRIPT_GAMEOBJECT_REF(targetEntity)
#define SCRIPT_GAMEOBJECT_REF(refName) \
    RegisterGameObjectRefField(#refName, &this->refName)

// SCRIPT_FIELD_VECTOR macro - registers vector fields for editor exposure
// Usage: SCRIPT_FIELD_VECTOR(myIntList, Int)
#define SCRIPT_FIELD_VECTOR(fieldName, elementType) \
    Register##elementType##VectorField(#fieldName, &this->fieldName)

// SCRIPT_FIELD_ENUM macro - registers enum fields with dropdown options
// Usage: SCRIPT_FIELD_ENUM(myState, {"Idle", "Running", "Jumping"})
//#define SCRIPT_FIELD_ENUM(fieldName, options) \
//    RegisterEnumField(#fieldName, &this->fieldName, options)

// SCRIPT_FIELD_STRUCT macro - registers struct fields using reflection
// Usage: SCRIPT_FIELD_STRUCT(myStats)
#define SCRIPT_FIELD_STRUCT(fieldName) \
    RegisterStructField(#fieldName, &this->fieldName)

// SCRIPT_FIELD_LAYERMASK macro - registers LayerMask fields with layer picker
// Usage: SCRIPT_FIELD_LAYERMASK(collisionMask)
#define SCRIPT_FIELD_LAYERMASK(fieldName) \
    RegisterLayerMaskField(#fieldName, &this->fieldName)

// SCRIPT_FIELD_LAYERREF macro - registers LayerRef fields with layer dropdown
// Usage: SCRIPT_FIELD_LAYERREF(targetLayer)
#define SCRIPT_FIELD_LAYERREF(fieldName) \
    RegisterLayerRefField(#fieldName, &this->fieldName)

//=============================================================================
// ENUM FIELD REGISTRATION MACRO
//=============================================================================

/**
 * Register an enum field with dropdown support in the editor.
 * @param fieldName The member variable name (must be an enum type)
 * @param ... The enum option names as string literals
 *
 * Usage:
 * @code
 * enum class EnemyState { Idle, Patrol, Chase, Attack };
 * EnemyState currentState = EnemyState::Idle;
 *
 * // In Initialize():
 * SCRIPT_ENUM_FIELD(currentState, "Idle", "Patrol", "Chase", "Attack");
 * @endcode
 */
#ifndef SCRIPT_ENUM_FIELD
#define SCRIPT_ENUM_FIELD(fieldName, ...) \
    RegisterEnumField(#fieldName, &this->fieldName, {__VA_ARGS__})
#endif

 /**
  * Register a vector of enum fields with dropdown support in the editor.
  * @param fieldName The member variable name (must be std::vector<EnumType>)
  * @param ... The enum option names as string literals
  *
  * Usage:
  * @code
  * enum class EnemyState { Idle, Patrol, Chase };
  * std::vector<EnemyState> stateQueue;
  *
  * // In Initialize():
  * SCRIPT_ENUM_VECTOR_FIELD(stateQueue, "Idle", "Patrol", "Chase");
  * @endcode
  */
#ifndef SCRIPT_ENUM_VECTOR_FIELD
#define SCRIPT_ENUM_VECTOR_FIELD(fieldName, ...) \
    RegisterEnumVectorField(#fieldName, &this->fieldName, {__VA_ARGS__})
#endif

// ============ CONVENIENCE NAMESPACES (GLOBAL SCOPE) ============
// Similar pattern to Input, Events, Coroutines namespaces in ScriptAPI.h

/// ECS Query namespace - Read-only component queries
namespace Query {
    // Template-based component checks (type-safe and concise)
    template<typename T>
    inline bool HasComponent(uint32_t entity) {
        return NE::ECS::Query::HasComponent<T>(entity);
    }

    // Direct component existence checks
    inline bool HasTransform(uint32_t e) { return NE::ECS::Query::HasTransform(e); }
    inline bool HasRenderer(uint32_t e) { return NE::ECS::Query::HasRenderer(e); }
    inline bool HasLight(uint32_t e) { return NE::ECS::Query::HasLight(e); }
    inline bool HasRigidbody(uint32_t e) { return NE::ECS::Query::HasRigidbody(e); }
    inline bool HasCollider(uint32_t e) { return NE::ECS::Query::HasCollider(e); }
    inline bool HasAudioSource(uint32_t e) { return NE::ECS::Query::HasAudioSource(e); }
    inline bool HasScript(uint32_t e) { return NE::ECS::Query::HasScript(e); }
    inline bool HasAnimator(uint32_t e) { return NE::ECS::Query::HasAnimator(e); }
    inline bool HasCamera(uint32_t e) { return NE::ECS::Query::HasCamera(e); }

    // UI component existence checks
    inline bool HasUICanvas(uint32_t e) { return NE::ECS::Query::HasUICanvas(e); }
    inline bool HasUIRectTransform(uint32_t e) { return NE::ECS::Query::HasUIRectTransform(e); }
    inline bool HasUIImage(uint32_t e) { return NE::ECS::Query::HasUIImage(e); }
    inline bool HasUIText(uint32_t e) { return NE::ECS::Query::HasUIText(e); }
    inline bool HasUIButton(uint32_t e) { return NE::ECS::Query::HasUIButton(e); }
    inline bool HasUISlider(uint32_t e) { return NE::ECS::Query::HasUISlider(e); }
    inline bool HasUIToggle(uint32_t e) { return NE::ECS::Query::HasUIToggle(e); }
    inline bool HasUIInputField(uint32_t e) { return NE::ECS::Query::HasUIInputField(e); }
    inline bool HasUIDropdown(uint32_t e) { return NE::ECS::Query::HasUIDropdown(e); }

    // Read-only component getters
    inline const NE::ECS::Component::EntityMeta& GetEntityMeta(uint32_t e) {
        return NE::ECS::Query::GetEntityMeta(e);
    }
    inline const NE::ECS::Component::Transform& GetEntityTransform(uint32_t e) {
        return NE::ECS::Query::GetEntityTransform(e);
    }
    inline const NE::ECS::Component::Renderer& GetEntityRenderer(uint32_t e) {
        return NE::ECS::Query::GetEntityRenderer(e);
    }
    inline const NE::ECS::Component::Light& GetEntityLight(uint32_t e) {
        return NE::ECS::Query::GetEntityLight(e);
    }
    inline const NE::ECS::Component::Rigidbody& GetEntityRigidbody(uint32_t e) {
        return NE::ECS::Query::GetEntityRigidbody(e);
    }
    inline const NE::ECS::Component::Collider& GetEntityCollider(uint32_t e) {
        return NE::ECS::Query::GetEntityCollider(e);
    }
    inline const NE::ECS::Component::AudioSource& GetEntityAudioSource(uint32_t e) {
        return NE::ECS::Query::GetEntityAudioSource(e);
    }
    inline const NE::ECS::Component::Animator& GetEntityAnimator(uint32_t e) {
        return NE::ECS::Query::GetEntityAnimator(e);
    }
    inline const NE::ECS::Component::Camera& GetEntityCamera(uint32_t e) {
        return NE::ECS::Query::GetEntityCamera(e);
    }

    // UI component read-only getters
    inline const NE::ECS::Component::UICanvas& GetUICanvas(uint32_t e) {
        return NE::ECS::Query::GetUICanvas(e);
    }
    inline const NE::ECS::Component::UIRectTransform& GetUIRectTransform(uint32_t e) {
        return NE::ECS::Query::GetUIRectTransform(e);
    }
    inline const NE::ECS::Component::UIImage& GetUIImage(uint32_t e) {
        return NE::ECS::Query::GetUIImage(e);
    }
    inline const NE::ECS::Component::UIText& GetUIText(uint32_t e) {
        return NE::ECS::Query::GetUIText(e);
    }
    inline const NE::ECS::Component::UIButton& GetUIButton(uint32_t e) {
        return NE::ECS::Query::GetUIButton(e);
    }
    inline const NE::ECS::Component::UISlider& GetUISlider(uint32_t e) {
        return NE::ECS::Query::GetUISlider(e);
    }
    inline const NE::ECS::Component::UIToggle& GetUIToggle(uint32_t e) {
        return NE::ECS::Query::GetUIToggle(e);
    }
    inline const NE::ECS::Component::UIInputField& GetUIInputField(uint32_t e) {
        return NE::ECS::Query::GetUIInputField(e);
    }
    inline const NE::ECS::Component::UIDropdown& GetUIDropdown(uint32_t e) {
        return NE::ECS::Query::GetUIDropdown(e);
    }
}

/// ECS Command namespace - Mutable entity and component operations
namespace Command {
    // Entity lifecycle
    inline uint32_t CreateEntity() { return NE::ECS::Command::CreateEntity(); }
    inline void DestroyEntity(uint32_t e) { NE::ECS::Command::DestroyEntity(e); }

    // Component addition
    inline void AddLight(uint32_t e) { NE::ECS::Command::AddLightComponent(e); }
    inline void AddRenderer(uint32_t e) { NE::ECS::Command::AddRendererComponent(e); }
    inline void AddRigidbody(uint32_t e) { NE::ECS::Command::AddRigidbodyComponent(e); }
    inline void AddCollider(uint32_t e) { NE::ECS::Command::AddColliderComponent(e); }
    inline void AddAudioSource(uint32_t e) { NE::ECS::Command::AddAudioSourceComponent(e); }
    inline void AddScript(uint32_t e) { NE::ECS::Command::AddScriptComponent(e); }
    inline void AddCamera(uint32_t e) { NE::ECS::Command::AddCameraComponent(e); }
    inline void AddAnimator(uint32_t e) { NE::ECS::Command::AddAnimatorComponent(e); }

    // Template-based component checks and access (type-safe and concise)
    template<typename T>
    inline bool HasComponent(uint32_t entity) {
        return NE::ECS::Command::HasComponent<T>(entity);
    }

    template<typename T>
    inline T& GetComponent(uint32_t entity) {
        return NE::ECS::Command::GetComponent<T>(entity);
    }

    // Mutable component getters
    inline NE::ECS::Component::EntityMeta& GetEntityMeta(uint32_t e) {
        return NE::ECS::Command::GetEntityMeta(e);
    }
    inline NE::ECS::Component::Transform& GetEntityTransform(uint32_t e) {
        return NE::ECS::Command::GetEntityTransform(e);
    }
    inline NE::ECS::Component::Renderer& GetEntityRenderer(uint32_t e) {
        return NE::ECS::Command::GetEntityRenderer(e);
    }
    inline NE::ECS::Component::Light& GetEntityLight(uint32_t e) {
        return NE::ECS::Command::GetEntityLight(e);
    }
    inline NE::ECS::Component::Rigidbody& GetEntityRigidbody(uint32_t e) {
        return NE::ECS::Command::GetEntityRigidbody(e);
    }
    inline NE::ECS::Component::Collider& GetEntityCollider(uint32_t e) {
        return NE::ECS::Command::GetEntityCollider(e);
    }
    inline NE::ECS::Component::AudioSource& GetEntityAudioSource(uint32_t e) {
        return NE::ECS::Command::GetEntityAudioSource(e);
    }
    inline NE::ECS::Component::Animator& GetEntityAnimator(uint32_t e) {
        return NE::ECS::Command::GetEntityAnimator(e);
    }
    inline NE::ECS::Component::Camera& GetEntityCamera(uint32_t e) {
        return NE::ECS::Command::GetEntityCamera(e);
    }

    // UI mutable component getters
    inline NE::ECS::Component::UICanvas& GetUICanvas(uint32_t e) {
        return NE::ECS::Command::GetUICanvas(e);
    }
    inline NE::ECS::Component::UIRectTransform& GetUIRectTransform(uint32_t e) {
        return NE::ECS::Command::GetUIRectTransform(e);
    }
    inline NE::ECS::Component::UIImage& GetUIImage(uint32_t e) {
        return NE::ECS::Command::GetUIImage(e);
    }
    inline NE::ECS::Component::UIText& GetUIText(uint32_t e) {
        return NE::ECS::Command::GetUIText(e);
    }
    inline NE::ECS::Component::UIButton& GetUIButton(uint32_t e) {
        return NE::ECS::Command::GetUIButton(e);
    }
    inline NE::ECS::Component::UISlider& GetUISlider(uint32_t e) {
        return NE::ECS::Command::GetUISlider(e);
    }
    inline NE::ECS::Component::UIToggle& GetUIToggle(uint32_t e) {
        return NE::ECS::Command::GetUIToggle(e);
    }
    inline NE::ECS::Component::UIInputField& GetUIInputField(uint32_t e) {
        return NE::ECS::Command::GetUIInputField(e);
    }
    inline NE::ECS::Component::UIDropdown& GetUIDropdown(uint32_t e) {
        return NE::ECS::Command::GetUIDropdown(e);
    }

    // UICanvas helpers
    inline float GetUICanvasAlpha(uint32_t e) { return NE::ECS::Command::GetUICanvasAlpha(e); }
    inline void SetUICanvasAlpha(uint32_t e, float alpha) { NE::ECS::Command::SetUICanvasAlpha(e, alpha); }

    // UI image utilities
    inline bool SetUIImageTexture(uint32_t e, const char* uuid) {
        return NE::ECS::Command::SetUIImageTexture(e, uuid);
    }
    inline bool SetUIImageTextureAndMaterial(uint32_t e, const char* texUUID, const char* matUUID) {
        return NE::ECS::Command::SetUIImageTextureAndMaterial(e, texUUID, matUUID);
    }
    inline void SetUIImageColor(uint32_t e, float r, float g, float b, float a) {
        NE::ECS::Command::SetUIImageColor(e, r, g, b, a);
    }
    inline void SetUIImageFillAmount(uint32_t e, float fillAmount) {
        NE::ECS::Command::SetUIImageFillAmount(e, fillAmount);
    }

    // UIText helpers
    inline void SetUIText(uint32_t e, const char* text) { NE::ECS::Command::SetUIText(e, text); }
    inline void SetUITextColor(uint32_t e, float r, float g, float b, float a) { NE::ECS::Command::SetUITextColor(e, r, g, b, a); }
    inline const char* GetUITextString(uint32_t e) { return NE::ECS::Command::GetUITextString(e); }

    // UIButton helpers
    inline bool WasButtonClicked(uint32_t e) { return NE::ECS::Command::WasButtonClicked(e); }
    inline bool IsButtonHovered(uint32_t e) { return NE::ECS::Command::IsButtonHovered(e); }
    inline bool IsButtonPressed(uint32_t e) { return NE::ECS::Command::IsButtonPressed(e); }
    inline void SetButtonInteractable(uint32_t e, bool v) { NE::ECS::Command::SetButtonInteractable(e, v); }
    inline bool IsButtonInteractable(uint32_t e) { return NE::ECS::Command::IsButtonInteractable(e); }

    // UIToggle helpers
    inline bool IsToggleOn(uint32_t e) { return NE::ECS::Command::IsToggleOn(e); }
    inline void SetToggleOn(uint32_t e, bool v) { NE::ECS::Command::SetToggleOn(e, v); }
    inline bool ToggleValueChanged(uint32_t e) { return NE::ECS::Command::ToggleValueChanged(e); }
    inline void SetToggleInteractable(uint32_t e, bool v) { NE::ECS::Command::SetToggleInteractable(e, v); }

    // UISlider helpers
    inline float GetSliderValue(uint32_t e) { return NE::ECS::Command::GetSliderValue(e); }
    inline void SetSliderValue(uint32_t e, float v) { NE::ECS::Command::SetSliderValue(e, v); }
    inline float GetSliderNormalizedValue(uint32_t e) { return NE::ECS::Command::GetSliderNormalizedValue(e); }
    inline void SetSliderNormalizedValue(uint32_t e, float v) { NE::ECS::Command::SetSliderNormalizedValue(e, v); }
    inline void SetSliderMinMax(uint32_t e, float mn, float mx) { NE::ECS::Command::SetSliderMinMax(e, mn, mx); }
    inline bool SliderValueChanged(uint32_t e) { return NE::ECS::Command::SliderValueChanged(e); }
    inline void SetSliderInteractable(uint32_t e, bool v) { NE::ECS::Command::SetSliderInteractable(e, v); }

    // UIInputField helpers
    inline const char* GetInputFieldText(uint32_t e) { return NE::ECS::Command::GetInputFieldText(e); }
    inline void SetInputFieldText(uint32_t e, const char* text) { NE::ECS::Command::SetInputFieldText(e, text); }
    inline bool IsInputFieldFocused(uint32_t e) { return NE::ECS::Command::IsInputFieldFocused(e); }
    inline void SetInputFieldInteractable(uint32_t e, bool v) { NE::ECS::Command::SetInputFieldInteractable(e, v); }

    // UIDropdown helpers
    inline int GetDropdownSelectedIndex(uint32_t e) { return NE::ECS::Command::GetDropdownSelectedIndex(e); }
    inline void SetDropdownSelectedIndex(uint32_t e, int idx) { NE::ECS::Command::SetDropdownSelectedIndex(e, idx); }
    inline int GetDropdownOptionCount(uint32_t e) { return NE::ECS::Command::GetDropdownOptionCount(e); }
    inline void SetDropdownInteractable(uint32_t e, bool v) { NE::ECS::Command::SetDropdownInteractable(e, v); }

    // Script management
    inline std::vector<std::string> GetRegisteredScriptNames() {
        return NE::ECS::Command::GetRegisteredScriptNames();
    }
    inline bool SetEntityScript(uint32_t e, const std::string& scriptName) {
        return NE::ECS::Command::SetEntityScript(e, scriptName);
    }
    inline void RemoveEntityScript(uint32_t e) {
        NE::ECS::Command::RemoveEntityScript(e);
    }
    inline bool IsScriptRegistered(const std::string& scriptName) {
        return NE::ECS::Command::IsScriptRegistered(scriptName);
    }
}

// ============ GLOBAL TYPE ALIASES FOR CONVENIENCE ============
// Makes common types available without namespace qualification

// Core scripting types
using Vec3 = NE::Scripting::Vec3;
using Entity = NE::Scripting::Entity;
using IScript = NE::Scripting::IScript;
using RaycastHit = NE::Scripting::RaycastHit;
using LayerMask = NE::Scripting::LayerMask;
using LayerRef = NE::Scripting::LayerRef;
using GameObject = NE::Scripting::GameObject;

// Component reference types
using TransformRef = NE::Scripting::TransformRef;
using RigidbodyRef = NE::Scripting::RigidbodyRef;
using AudioSourceRef = NE::Scripting::AudioSourceRef;
using MaterialRef = NE::Scripting::MaterialRef;
using PrefabRef = NE::Scripting::PrefabRef;
using GameObjectRef = NE::Scripting::GameObjectRef;

// Coroutine handle type
using CoroutineHandle = NE::Scripting::CoroutineHandle;

// Component namespace shortcut (for type-safe template usage)
namespace Component = NE::ECS::Component;

// ============ LEGACY NAMESPACE (FOR BACKWARD COMPATIBILITY) ============
// Type aliases for convenience
namespace ChronoGame {
    // Alias the SDK types into ChronoGame namespace for easier use
    using Vec3 = NE::Scripting::Vec3;
    using Entity = NE::Scripting::Entity;
    using IScript = NE::Scripting::IScript;
    using RaycastHit = NE::Scripting::RaycastHit;
    using GameObject = NE::Scripting::GameObject;

    // Component reference types
    using TransformRef = NE::Scripting::TransformRef;
    using RigidbodyRef = NE::Scripting::RigidbodyRef;
    using AudioSourceRef = NE::Scripting::AudioSourceRef;
    using PrefabRef = NE::Scripting::PrefabRef;
    using GameObjectRef = NE::Scripting::GameObjectRef;
}
