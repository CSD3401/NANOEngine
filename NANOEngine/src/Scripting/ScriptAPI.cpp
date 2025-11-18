/**
 * @file ScriptAPI.cpp
 * @brief Implementation of the clean scripting SDK API
 *
 * This file acts as an adapter/bridge between the public SDK API
 * (ScriptSDK headers) and the internal engine implementation (IScript).
 */

#include "../../include/ScriptSDK/ScriptAPI.h"
#include "../../include/ScriptSDK/ScriptMacros.h"
#include "ScriptContext.hpp"

// Internal engine headers (NOT exposed to scripts)
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/AudioSource.hpp"
#include "../Physics/PhysicsManager.hpp"
#include <Math/Vec3.hpp>
#include "../Core/SpdLogger.hpp"
#include "../Core/Couroutine.hpp"
#include "../Input/InputManager.hpp"
#include "../Events/EventBus.hpp"

#include <sstream>
#include <unordered_map>
#include <functional>
#include <cmath>

namespace NE {
namespace Scripting {

    //=========================================================================
    // TYPE CONVERSION UTILITIES (SDK ↔ Engine)
    //=========================================================================

    inline Math::Vec3 ToEngineVec3(const Vec3& v) {
        return Math::Vec3(v.x, v.y, v.z);
    }

    inline Vec3 ToSDKVec3(const Math::Vec3& v) {
        return Vec3(v.x, v.y, v.z);
    }

    // Vector normalization helper
    inline Vec3 Normalize(const Vec3& v) {
        float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (length > 0.0001f) {
            return Vec3(v.x / length, v.y / length, v.z / length);
        }
        return v;
    }

    // Context validation macro for consistent null-checking
    #define CHECK_CONTEXT_OR_RETURN(returnValue) \
        if (!m_context || !m_context->componentManager) return returnValue

    //=========================================================================
    // FIELD REGISTRY (PIMPL - Hide STL containers from DLL interface)
    //=========================================================================

    class IScript::FieldRegistry {
    public:
        struct FieldEntry {
            std::string typeToken;
            void* memberPtr;
            std::function<std::string()> getValue;
            std::function<bool(const std::string&)> setValue;
        };

        std::unordered_map<std::string, FieldEntry> fields;
    };

    //=========================================================================
    // IScript Implementation
    //=========================================================================

    IScript::~IScript() {
        delete m_fieldRegistry;
        // Note: ScriptContext is owned by the engine, don't delete it
    }

    void IScript::_LinkToEngine(ScriptContext* context) {
        m_context = context;

        // Initialize field registry if not already done
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }
    }

    void IScript::_RefreshComponentReferences() {
        // This would be implemented by the engine to update component pointers
        // after hot reload or scene changes
    }

    void IScript::SetEnabled(bool enabled) {
        if (m_enabled != enabled) {
            m_enabled = enabled;
            if (enabled) {
                OnEnable();
            } else {
                OnDisable();
            }
        }
    }

    //=========================================================================
    // Transform Operations
    //=========================================================================

    Vec3 IScript::GetPosition() const {
        CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(m_entity))
            return Vec3::Zero();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(m_entity);
        return ToSDKVec3(transform.position);
    }

    void IScript::SetPosition(const Vec3& pos) {
        CHECK_CONTEXT_OR_RETURN();

        if (m_context->componentManager->HasComponent<ECS::Component::Transform>(m_entity)) {
            auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(m_entity);
            transform.position = ToEngineVec3(pos);
            transform.isDirty = true;
        }
    }

    void IScript::SetPosition(float x, float y, float z) {
        SetPosition(Vec3(x, y, z));
    }

    Vec3 IScript::GetRotation() const {
        CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(m_entity))
            return Vec3::Zero();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(m_entity);
        return ToSDKVec3(transform.rotation);
    }

    void IScript::SetRotation(const Vec3& rot) {
        CHECK_CONTEXT_OR_RETURN();

        if (m_context->componentManager->HasComponent<ECS::Component::Transform>(m_entity)) {
            auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(m_entity);
            transform.rotation = ToEngineVec3(rot);
            transform.isDirty = true;
        }
    }

    void IScript::SetRotation(float x, float y, float z) {
        SetRotation(Vec3(x, y, z));
    }

    Vec3 IScript::GetScale() const {
        CHECK_CONTEXT_OR_RETURN(Vec3::One());

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(m_entity))
            return Vec3::One();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(m_entity);
        return ToSDKVec3(transform.scale);
    }

    void IScript::SetScale(const Vec3& scale) {
        CHECK_CONTEXT_OR_RETURN();

        if (m_context->componentManager->HasComponent<ECS::Component::Transform>(m_entity)) {
            auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(m_entity);
            transform.scale = ToEngineVec3(scale);
            transform.isDirty = true;
        }
    }

    void IScript::SetScale(float x, float y, float z) {
        SetScale(Vec3(x, y, z));
    }

    void IScript::SetScale(float uniformScale) {
        SetScale(Vec3(uniformScale, uniformScale, uniformScale));
    }

    void IScript::Translate(const Vec3& translation) {
        SetPosition(GetPosition() + translation);
    }

    void IScript::Translate(float x, float y, float z) {
        Translate(Vec3(x, y, z));
    }

    void IScript::Rotate(const Vec3& rotation) {
        SetRotation(GetRotation() + rotation);
    }

    void IScript::Rotate(float x, float y, float z) {
        Rotate(Vec3(x, y, z));
    }

    Vec3 IScript::GetForward() const {
        Vec3 rotation = GetRotation(); // (pitch, yaw, roll) in degrees

        // Convert degrees to radians
        float pitch = rotation.x * (3.14159265f / 180.0f);
        float yaw = rotation.y * (3.14159265f / 180.0f);

        // Calculate forward vector from Euler angles (Y-up, Z-forward, X-right)
        Vec3 forward;
        forward.x = std::cos(pitch) * std::sin(yaw);
        forward.y = -std::sin(pitch);
        forward.z = -std::cos(pitch) * std::cos(yaw);

        return Normalize(forward);
    }

    Vec3 IScript::GetRight() const {
        Vec3 rotation = GetRotation(); // (pitch, yaw, roll) in degrees

        // Convert degrees to radians
        float yaw = rotation.y * (3.14159265f / 180.0f);

        // Right vector is perpendicular to forward in XZ plane
        Vec3 right;
        right.x = std::cos(yaw);
        right.y = 0.0f;
        right.z = std::sin(yaw);

        return Normalize(right);
    }

    Vec3 IScript::GetUp() const {
        // Up is always world up in this simple implementation
        // For more complex scenarios, you might want to calculate it from forward and right
        return Vec3(0.0f, 1.0f, 0.0f);
    }

    //=========================================================================
    // Rigidbody Physics
    //=========================================================================

    bool IScript::HasRigidbody() const {
        // Check if entity has a physics body registered with PhysicsManager
        return Physics::PhysicsManager::EntityHasPhysicsBody(m_entity);
    }

    float IScript::GetMass() const {
        CHECK_CONTEXT_OR_RETURN(0.0f);

        if (!m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(m_entity))
            return 0.0f;

        return m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(m_entity).mass;
    }

    void IScript::SetMass(float mass) {
        CHECK_CONTEXT_OR_RETURN();

        if (m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(m_entity)) {
            auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(m_entity);
            rigidbody.mass = mass;
        }
    }

    bool IScript::GetUseGravity() const {
        CHECK_CONTEXT_OR_RETURN(false);

        if (!m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(m_entity))
            return false;

        return m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(m_entity).useGravity;
    }

    void IScript::SetUseGravity(bool use) {
        if (!Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(m_entity);
        Physics::PhysicsManager::SetGravityEnabled(bodyID, use);

        // Also update Rigidbody component if it exists
        if (m_context && m_context->componentManager &&
            m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(m_entity)) {
            auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(m_entity);
            rigidbody.useGravity = use;
        }
    }

    bool IScript::IsStatic() const {
        if (!m_context || !m_context->componentManager) return false;

        if (!m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(m_entity))
            return false;

        return m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(m_entity).isStatic;
    }

    void IScript::SetStatic(bool isStatic) {
        if (!m_context || !m_context->componentManager) return;

        if (m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(m_entity)) {
            auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(m_entity);
            rigidbody.isStatic = isStatic;
        }
    }

    void IScript::LockRotation(bool lockX, bool lockY, bool lockZ) {
        if (!Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(m_entity);
        Physics::PhysicsManager::LockRotation(bodyID, lockX, lockY, lockZ);
    }

    Vec3 IScript::GetVelocity() const {
        if (!Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) {
            return Vec3::Zero();
        }

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(m_entity);
        return ToSDKVec3(Physics::PhysicsManager::GetLinearVelocity(bodyID));
    }

    void IScript::SetVelocity(const Vec3& velocity) {
        if (!Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(m_entity);
        Physics::PhysicsManager::SetLinearVelocity(bodyID, ToEngineVec3(velocity));
    }

    void IScript::SetVelocity(float x, float y, float z) {
        SetVelocity(Vec3(x, y, z));
    }

    void IScript::AddForce(const Vec3& force) {
        if (!Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(m_entity);
        Physics::PhysicsManager::AddForce(bodyID, ToEngineVec3(force));
    }

    void IScript::AddForce(float x, float y, float z) {
        AddForce(Vec3(x, y, z));
    }

    void IScript::AddImpulse(const Vec3& impulse) {
        if (!Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(m_entity);
        Physics::PhysicsManager::AddImpulse(bodyID, ToEngineVec3(impulse));
    }

    void IScript::AddImpulse(float x, float y, float z) {
        AddImpulse(Vec3(x, y, z));
    }

    //=========================================================================
    // Physics Raycasting
    //=========================================================================

    RaycastHit IScript::Raycast(const Vec3& origin, const Vec3& direction, float maxDistance, uint32_t layerMask) const {
        RaycastHit result;

        if (!m_context || !m_context->componentManager) {
            result.hasHit = false;
            return result;
        }

        // Call PhysicsManager raycast with layer mask (static method)
        auto hit = Physics::PhysicsManager::Raycast(
            ToEngineVec3(origin),
            ToEngineVec3(direction),
            maxDistance,
            layerMask
        );

        // Convert PhysicsManager::RaycastHit to SDK RaycastHit
        result.hasHit = hit.hasHit;
        result.point = ToSDKVec3(hit.point);
        result.normal = ToSDKVec3(hit.normal);
        result.distance = hit.distance;
        result.entity = hit.entity;
        return result;
    }

    RaycastHit IScript::Raycast(float originX, float originY, float originZ,
                                 float dirX, float dirY, float dirZ,
                                 float maxDistance, uint32_t layerMask) const {
        return Raycast(Vec3(originX, originY, originZ),
                       Vec3(dirX, dirY, dirZ),
                       maxDistance,
                       layerMask);
    }

    std::vector<RaycastHit> IScript::RaycastAll(const Vec3& origin, const Vec3& direction,
                                                 float maxDistance, uint32_t layerMask) const {
        std::vector<RaycastHit> results;

        if (!m_context || !m_context->componentManager) {
            return results;
        }

        // Call PhysicsManager raycast with layer mask (static method)
        auto hits = Physics::PhysicsManager::RaycastAll(
            ToEngineVec3(origin),
            ToEngineVec3(direction),
            maxDistance,
            layerMask
        );

        // Convert PhysicsManager::RaycastHit to SDK RaycastHit
        results.reserve(hits.size());
        for (const auto& hit : hits) {
            RaycastHit result;
            result.hasHit = hit.hasHit;
            result.point = ToSDKVec3(hit.point);
            result.normal = ToSDKVec3(hit.normal);
            result.distance = hit.distance;
            result.entity = hit.entity;
            results.push_back(result);
        }

        return results;
    }

    //=========================================================================
    // Audio Source
    //=========================================================================

    bool IScript::HasAudioSource() const {
        if (!m_context || !m_context->componentManager) return false;
        return m_context->componentManager->HasComponent<ECS::Component::AudioSource>(m_entity);
    }

    void IScript::PlayAudio() {
        if (!HasAudioSource()) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);

        // If already playing and not paused, stop first
        if (audioSource.m_channel && audioSource.isPlaying && !audioSource.isPaused) {
            audioSource.m_channel->stop();
        }

        // Reset state - AudioSystem will handle actual playback
        audioSource.isPlaying = true;
        audioSource.isPaused = false;
        audioSource.m_hasPlayed = false; // Trigger playback in AudioSystem
    }

    void IScript::StopAudio() {
        if (!HasAudioSource()) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);

        if (audioSource.m_channel) {
            audioSource.m_channel->stop();
        }

        audioSource.isPlaying = false;
        audioSource.isPaused = false;
    }

    void IScript::PauseAudio() {
        if (!HasAudioSource()) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);

        if (audioSource.m_channel && audioSource.isPlaying) {
            audioSource.m_channel->setPaused(true);
            audioSource.isPaused = true;
        }
    }

    void IScript::ResumeAudio() {
        if (!HasAudioSource()) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);

        if (audioSource.m_channel && audioSource.isPaused) {
            audioSource.m_channel->setPaused(false);
            audioSource.isPaused = false;
        }
    }

    bool IScript::IsAudioPlaying() const {
        if (!HasAudioSource()) return false;
        const auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);
        return audioSource.isPlaying && !audioSource.isPaused;
    }

    float IScript::GetVolume() const {
        if (!HasAudioSource()) return 0.0f;
        auto& audio = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);
        return audio.volume;
    }

    void IScript::SetVolume(float volume) {
        if (!HasAudioSource()) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);
        audioSource.volume = volume;

        // Apply immediately if playing
        if (audioSource.m_channel) {
            audioSource.m_channel->setVolume(volume);
        }
    }

    float IScript::GetPitch() const {
        if (!HasAudioSource()) return 1.0f;
        auto& audio = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);
        return audio.pitch;
    }

    void IScript::SetPitch(float pitch) {
        if (!HasAudioSource()) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);
        audioSource.pitch = pitch;

        // Apply immediately if playing
        if (audioSource.m_channel) {
            audioSource.m_channel->setPitch(pitch);
        }
    }

    void IScript::SetAudioLoop(bool loop) {
        if (!HasAudioSource()) return;
        auto& audio = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(m_entity);
        audio.loop = loop;
    }

    //=========================================================================
    // Component References
    //=========================================================================

    TransformRef IScript::GetTransformRef(Entity entity) const {
        if (!m_context || !m_context->componentManager) return TransformRef();

        if (m_context->componentManager->HasComponent<ECS::Component::Transform>(entity)) {
            return TransformRef(entity);
        }
        return TransformRef();
    }

    RigidbodyRef IScript::GetRigidbodyRef(Entity entity) const {
        if (!m_context || !m_context->componentManager) return RigidbodyRef();

        if (m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(entity)) {
            return RigidbodyRef(entity);
        }
        return RigidbodyRef();
    }

    AudioSourceRef IScript::GetAudioSourceRef(Entity entity) const {
        if (!m_context || !m_context->componentManager) return AudioSourceRef();

        if (m_context->componentManager->HasComponent<ECS::Component::AudioSource>(entity)) {
            return AudioSourceRef(entity);
        }
        return AudioSourceRef();
    }

    // Component ref operations (for stored references)
    Vec3 IScript::GetPosition(const TransformRef& ref) const {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return Vec3::Zero();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        return ToSDKVec3(transform.position);
    }

    void IScript::SetPosition(const TransformRef& ref, const Vec3& pos) {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return;

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        transform.position = ToEngineVec3(pos);
        transform.isDirty = true;
    }

    void IScript::SetPosition(const TransformRef& ref, float x, float y, float z) {
        SetPosition(ref, Vec3(x, y, z));
    }

    Vec3 IScript::GetRotation(const TransformRef& ref) const {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return Vec3::Zero();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        return ToSDKVec3(transform.rotation);
    }

    void IScript::SetRotation(const TransformRef& ref, const Vec3& rot) {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return;

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        transform.rotation = ToEngineVec3(rot);
        transform.isDirty = true;
    }

    Vec3 IScript::GetScale(const TransformRef& ref) const {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return Vec3::One();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        return ToSDKVec3(transform.scale);
    }

    void IScript::SetScale(const TransformRef& ref, const Vec3& scale) {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return;

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        transform.scale = ToEngineVec3(scale);
        transform.isDirty = true;
    }

    Vec3 IScript::GetVelocity(const RigidbodyRef& ref) const {
        if (!ref.IsValid()) return Vec3::Zero();

        if (!Physics::PhysicsManager::EntityHasPhysicsBody(ref.GetEntity())) {
            return Vec3::Zero();
        }

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(ref.GetEntity());
        return ToSDKVec3(Physics::PhysicsManager::GetLinearVelocity(bodyID));
    }

    void IScript::SetVelocity(const RigidbodyRef& ref, const Vec3& velocity) {
        if (!ref.IsValid()) return;

        if (!Physics::PhysicsManager::EntityHasPhysicsBody(ref.GetEntity())) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(ref.GetEntity());
        Physics::PhysicsManager::SetLinearVelocity(bodyID, ToEngineVec3(velocity));
    }

    void IScript::AddForce(const RigidbodyRef& ref, const Vec3& force) {
        if (!ref.IsValid()) return;

        if (!Physics::PhysicsManager::EntityHasPhysicsBody(ref.GetEntity())) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(ref.GetEntity());
        Physics::PhysicsManager::AddForce(bodyID, ToEngineVec3(force));
    }

    //=========================================================================
    // Field Registration
    //=========================================================================

    // Helper function to reduce code duplication in field registration
    void IScript::RegisterFieldInternal(
        const std::string& name,
        const std::string& typeToken,
        void* memberPtr,
        std::function<std::string()> getValue,
        std::function<bool(const std::string&)> setValue)
    {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        FieldRegistry::FieldEntry entry;
        entry.typeToken = typeToken;
        entry.memberPtr = memberPtr;
        entry.getValue = std::move(getValue);
        entry.setValue = std::move(setValue);
        m_fieldRegistry->fields[name] = std::move(entry);
    }

    void IScript::RegisterFloatField(const std::string& name, float* memberPtr) {
        RegisterFieldInternal(
            name,
            "float",
            memberPtr,
            [memberPtr]() -> std::string { return std::to_string(*memberPtr); },
            [memberPtr](const std::string& value) -> bool {
                try {
                    *memberPtr = std::stof(value);
                    return true;
                } catch (...) {
                    return false;
                }
            }
        );
    }

    void IScript::RegisterIntField(const std::string& name, int* memberPtr) {
        RegisterFieldInternal(
            name,
            "int",
            memberPtr,
            [memberPtr]() -> std::string { return std::to_string(*memberPtr); },
            [memberPtr](const std::string& value) -> bool {
                try {
                    *memberPtr = std::stoi(value);
                    return true;
                } catch (...) {
                    return false;
                }
            }
        );
    }

    void IScript::RegisterBoolField(const std::string& name, bool* memberPtr) {
        RegisterFieldInternal(
            name,
            "bool",
            memberPtr,
            [memberPtr]() -> std::string { return *memberPtr ? "1" : "0"; },
            [memberPtr](const std::string& value) -> bool {
                if (value == "1" || value == "true") {
                    *memberPtr = true;
                    return true;
                }
                if (value == "0" || value == "false") {
                    *memberPtr = false;
                    return true;
                }
                return false;
            }
        );
    }

    void IScript::RegisterStringField(const std::string& name, std::string* memberPtr) {
        RegisterFieldInternal(
            name,
            "string",
            memberPtr,
            [memberPtr]() -> std::string { return *memberPtr; },
            [memberPtr](const std::string& value) -> bool {
                try {
                    *memberPtr = value;
                    return true;
                } catch (...) {
                    return false;
                }
            }
        );
    }

    void IScript::RegisterVec3Field(const std::string& name, Vec3* memberPtr) {
        RegisterFieldInternal(
            name,
            "vec3",
            memberPtr,
            [memberPtr]() -> std::string {
                std::ostringstream oss;
                oss << memberPtr->x << ' ' << memberPtr->y << ' ' << memberPtr->z;
                return oss.str();
            },
            [memberPtr](const std::string& value) -> bool {
                try {
                    std::istringstream iss(value);
                    float x, y, z;
                    if (!(iss >> x >> y >> z)) {
                        return false;
                    }
                    memberPtr->x = x;
                    memberPtr->y = y;
                    memberPtr->z = z;
                    return true;
                } catch (...) {
                    return false;
                }
            }
        );
    }

    void IScript::RegisterTransformRefField(const std::string& name, TransformRef* memberPtr) {
        RegisterFieldInternal(
            name,
            "transformref",
            memberPtr,
            [memberPtr]() -> std::string { return std::to_string(memberPtr->GetEntity()); },
            [this, memberPtr](const std::string& value) -> bool {
                try {
                    Entity entity = static_cast<Entity>(std::stoul(value));
                    *memberPtr = GetTransformRef(entity);
                    return true;
                } catch (...) {
                    return false;
                }
            }
        );
    }

    void IScript::RegisterRigidbodyRefField(const std::string& name, RigidbodyRef* memberPtr) {
        RegisterFieldInternal(
            name,
            "rigidbodyref",
            memberPtr,
            [memberPtr]() -> std::string { return std::to_string(memberPtr->GetEntity()); },
            [this, memberPtr](const std::string& value) -> bool {
                try {
                    Entity entity = static_cast<Entity>(std::stoul(value));
                    *memberPtr = GetRigidbodyRef(entity);
                    return true;
                } catch (...) {
                    return false;
                }
            }
        );
    }

    void IScript::RegisterAudioSourceRefField(const std::string& name, AudioSourceRef* memberPtr) {
        RegisterFieldInternal(
            name,
            "audiosourceref",
            memberPtr,
            [memberPtr]() -> std::string { return std::to_string(memberPtr->GetEntity()); },
            [this, memberPtr](const std::string& value) -> bool {
                try {
                    Entity entity = static_cast<Entity>(std::stoul(value));
                    *memberPtr = GetAudioSourceRef(entity);
                    return true;
                } catch (...) {
                    return false;
                }
            }
        );
    }

    //=========================================================================
    // Field Query Interface
    //=========================================================================

    std::vector<std::string> IScript::GetExposedFieldNames() const {
        if (!m_fieldRegistry) {
            return {};
        }

        std::vector<std::string> names;
        names.reserve(m_fieldRegistry->fields.size());
        for (const auto& [name, entry] : m_fieldRegistry->fields) {
            names.push_back(name);
        }
        return names;
    }

    std::string IScript::GetFieldType(const std::string& name) const {
        if (!m_fieldRegistry) {
            return {};
        }

        auto it = m_fieldRegistry->fields.find(name);
        if (it != m_fieldRegistry->fields.end()) {
            return it->second.typeToken;
        }
        return {};
    }

    std::string IScript::GetFieldValueAsString(const std::string& name) const {
        if (!m_fieldRegistry) {
            return {};
        }

        auto it = m_fieldRegistry->fields.find(name);
        if (it != m_fieldRegistry->fields.end()) {
            return it->second.getValue();
        }
        return {};
    }

    bool IScript::SetFieldValueFromString(const std::string& name, const std::string& value) {
        if (!m_fieldRegistry) {
            return false;
        }

        auto it = m_fieldRegistry->fields.find(name);
        if (it != m_fieldRegistry->fields.end()) {
            return it->second.setValue(value);
        }
        return false;
    }

    // Virtual methods with default implementations for optional override
    std::vector<std::string> IScript::GetEnumOptions(const std::string& fieldName) const {
        (void)fieldName;
        return {};
    }

    int IScript::GetEnumValue(const std::string& fieldName) const {
        (void)fieldName;
        return 0;
    }

    void IScript::SetEnumValue(const std::string& fieldName, int value) {
        (void)fieldName;
        (void)value;
    }

    size_t IScript::GetArraySize(const std::string& fieldName) const {
        (void)fieldName;
        return 0;
    }

    std::string IScript::GetArrayElement(const std::string& fieldName, size_t index) const {
        (void)fieldName;
        (void)index;
        return "";
    }

    bool IScript::SetArrayElement(const std::string& fieldName, size_t index, const std::string& value) {
        (void)fieldName;
        (void)index;
        (void)value;
        return false;
    }

    void IScript::AddArrayElement(const std::string& fieldName) {
        (void)fieldName;
    }

    void IScript::RemoveArrayElement(const std::string& fieldName, size_t index) {
        (void)fieldName;
        (void)index;
    }

    //=========================================================================
    // LOGGING API IMPLEMENTATION (SDK → Engine bridge)
    //=========================================================================

    void Log(LogLevel level, const std::string& message, const std::string& file, int line) {
        // Convert SDK LogLevel to engine SpdLogLevel
        SpdLogLevel engineLevel;
        switch (level) {
            case LogLevel::Debug:    engineLevel = SpdLogLevel::Debug; break;
            case LogLevel::Info:     engineLevel = SpdLogLevel::Info; break;
            case LogLevel::Warning:  engineLevel = SpdLogLevel::Warning; break;
            case LogLevel::Error:    engineLevel = SpdLogLevel::Error; break;
            case LogLevel::Critical: engineLevel = SpdLogLevel::Critical; break;
            default:                 engineLevel = SpdLogLevel::Info; break;
        }

        // Forward to engine logger
        SpdLogger::GetInstance().Log(engineLevel, message, file, line);
    }

    //=========================================================================
    // COROUTINE API IMPLEMENTATION (SDK → Engine bridge)
    //=========================================================================

    CoroutineHandle CreateCoroutine() {
        return Engine_CreateCoroutine();
    }

    void AddCoroutineAction(CoroutineHandle handle, std::function<void()> action) {
        Engine_AddActionCpp(handle, action);
    }

    void AddCoroutineWait(CoroutineHandle handle, float seconds) {
        Engine_AddWaitForSeconds(handle, seconds);
    }

    void StartCoroutine(CoroutineHandle handle) {
        Engine_StartCoroutine(handle);
    }

    //=========================================================================
    // INPUT API IMPLEMENTATION (SDK → Engine bridge)
    //=========================================================================

    bool IsKeyDown(int key) {
        return NE::InputManager::IsKeyDown(key);
    }

    bool WasKeyPressed(int key) {
        return NE::InputManager::WasKeyPressed(key);
    }

    bool WasKeyReleased(int key) {
        return NE::InputManager::WasKeyReleased(key);
    }

    bool IsMouseDown(int button) {
        return NE::InputManager::IsMouseDown(button);
    }

    bool WasMousePressed(int button) {
        return NE::InputManager::WasMousePressed(button);
    }

    bool WasMouseReleased(int button) {
        return NE::InputManager::WasMouseReleased(button);
    }

    std::pair<double, double> MousePos() {
        return NE::InputManager::MousePos();
    }

    std::pair<double, double> MouseDelta() {
        return NE::InputManager::MouseDelta();
    }

    std::pair<double, double> ScrollDelta() {
        return NE::InputManager::ScrollDelta();
    }

    void SetMouseLocked(bool locked) {
        NE::InputManager::SetMouseLocked(locked);
    }

    bool IsMouseLocked() {
        return NE::InputManager::IsMouseLocked();
    }

    //=========================================================================
    // EVENT API IMPLEMENTATION (SDK → Engine bridge)
    //=========================================================================

    void SendScriptEvent(const char* eventName, void* data) {
        NANOEngine::Events::SendScriptEvent(eventName, data);
    }

    void RegisterScriptEventListener(const char* eventName, std::function<void(void*)> callback) {
        NANOEngine::Events::RegisterScriptEventListener(eventName, callback);
    }

    void ClearScriptEventListeners() {
        NANOEngine::Events::ClearScriptEventListeners();
    }

} // namespace Scripting
} // namespace NE
