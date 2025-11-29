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
#include "ScriptContextFactory.hpp"

// Internal engine headers (NOT exposed to scripts)
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/AudioSource.hpp"
#include "../ECS/Components/EntityMeta.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/Camera.hpp"
#include "../ECS/Components/NativeScript.hpp"
#include "../Physics/PhysicsManager.hpp"
#include <Math/Vec3.hpp>
#include "../Core/SpdLogger.hpp"
#include "../Core/Couroutine.hpp"
#include "../Input/InputManager.hpp"
#include "../Events/EventBus.hpp"
#include "../EngineState.hpp"  // Include EngineState for dirty flag logic
#include "../Engine.hpp"  // Include Engine for MarkSceneDirty()
#include "../Tween/TweenManager.hpp"  // Include TweenManager for tween API

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

    //=========================================================================
    // LUID ↔ ENTITY CONVERSION UTILITIES
    //=========================================================================

    /**
     * Get LUID from Entity ID.
     * Returns 0 if entity doesn't exist or has no EntityMeta component.
     */
    inline uint64_t GetLUIDFromEntity(Entity entity, ECS::ComponentManager* componentManager) {
        if (!componentManager || entity == INVALID_ENTITY) {
            return 0;
        }

        if (!componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
            return 0;
        }

        return componentManager->GetComponent<ECS::Component::EntityMeta>(entity).luid;
    }

    /**
     * Get Entity ID from LUID.
     * Returns INVALID_ENTITY if LUID not found.
     * This iterates through all entities, so it's not super fast - use sparingly.
     */
    inline Entity GetEntityFromLUID(uint64_t luid, ECS::ComponentManager* componentManager, ECS::EntityManager* entityManager) {
        if (!componentManager || !entityManager || luid == 0) {
            return INVALID_ENTITY;
        }

        // Iterate through all active entities to find matching LUID
        const auto& usedEntities = entityManager->GetUsedEntities();
        for (Entity entity : usedEntities) {
            if (componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
                const auto& meta = componentManager->GetComponent<ECS::Component::EntityMeta>(entity);
                if (meta.luid == luid) {
                    return entity;
                }
            }
        }

        return INVALID_ENTITY;
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

            // Array operation callbacks for vector fields
            std::function<size_t()> getSize;
            std::function<std::string(size_t)> getElement;
            std::function<bool(size_t, const std::string&)> setElement;
            std::function<void()> addElement;
            std::function<void(size_t)> removeElement;

            // Enum support
            std::vector<std::string> enumOptions;
            std::function<int()> getEnumValue;
            std::function<void(int)> setEnumValue;

            // LayerMask support
            std::function<uint32_t()> getLayerMaskValue;
            std::function<void(uint32_t)> setLayerMaskValue;
        };

        std::unordered_map<std::string, FieldEntry> fields;
    };

    //=========================================================================
    // IScript Implementation
    //=========================================================================

    IScript::~IScript() {
        delete m_fieldRegistry;
        // Properly clean up ScriptContext to avoid memory leak
        if (m_context) {
            DestroyScriptContext(m_context);
            m_context = nullptr;
        }
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

    Vec3 IScript::GetPosition(Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

        // Use m_entity if entity is DEFAULT_ENTITY_PARAM
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
            return Vec3::Zero();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
        return ToSDKVec3(transform.localPosition);
    }

    Vec3 IScript::GetWorldPosition(Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
            return Vec3::Zero();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
        Math::Mat4 m = transform.worldMatrix;
        Math::Vec3 worldPos = m.GetTranslation();
        return ToSDKVec3(worldPos);
    }

    void IScript::SetPosition(const Vec3& pos, Entity entity) {
        CHECK_CONTEXT_OR_RETURN();

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity)) {
            auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
            transform.localPosition = ToEngineVec3(pos);
            transform.isDirty = true;
        }
    }

    void IScript::SetPosition(float x, float y, float z, Entity entity) {
        SetPosition(Vec3(x, y, z), entity);
    }

    Vec3 IScript::GetRotation(Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
            return Vec3::Zero();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
        return ToSDKVec3(transform.localRotationEuler);
    }

    void IScript::SetRotation(const Vec3& rot, Entity entity) {
        CHECK_CONTEXT_OR_RETURN();

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity)) {
            auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
            transform.localRotationEuler = ToEngineVec3(rot);
            transform.isDirty = true;
        }
    }

    void IScript::SetRotation(float x, float y, float z, Entity entity) {
        SetRotation(Vec3(x, y, z), entity);
    }

    Vec3 IScript::GetScale(Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(Vec3::One());

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
            return Vec3::One();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
        return ToSDKVec3(transform.localScale);
    }

    void IScript::SetScale(const Vec3& scale, Entity entity) {
        CHECK_CONTEXT_OR_RETURN();

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity)) {
            auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
            transform.localScale = ToEngineVec3(scale);
            transform.isDirty = true;
        }
    }

    void IScript::SetScale(float x, float y, float z, Entity entity) {
        SetScale(Vec3(x, y, z), entity);
    }

    void IScript::SetScale(float uniformScale, Entity entity) {
        SetScale(Vec3(uniformScale, uniformScale, uniformScale), entity);
    }

    void IScript::Translate(const Vec3& translation, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        SetPosition(GetPosition(targetEntity) + translation, targetEntity);
    }

    void IScript::Translate(float x, float y, float z, Entity entity) {
        Translate(Vec3(x, y, z), entity);
    }

    void IScript::Rotate(const Vec3& rotation, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        SetRotation(GetRotation(targetEntity) + rotation, targetEntity);
    }

    void IScript::Rotate(float x, float y, float z, Entity entity) {
        Rotate(Vec3(x, y, z), entity);
    }

    Vec3 IScript::GetForward(Entity entity) const {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        Vec3 rotation = GetRotation(targetEntity); // (pitch, yaw, roll) in degrees

        float pitch = rotation.x * (3.14159265f / 180.0f);
        float yaw = rotation.y * (3.14159265f / 180.0f);

        Vec3 forward;
        forward.x = std::cos(pitch) * std::cos(yaw);
        forward.y = std::sin(pitch);
        forward.z = std::cos(pitch) * std::sin(yaw);

        return forward.Normalized();
    }

    Vec3 IScript::GetRight(Entity entity) const {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        Vec3 rotation = GetRotation(targetEntity); // (pitch, yaw, roll) in degrees

        // Convert degrees to radians
        float yaw = rotation.y * (3.14159265f / 180.0f);

        // Right vector is perpendicular to forward in XZ plane
        Vec3 right;
        right.x = std::cos(yaw);
        right.y = 0.0f;
        right.z = std::sin(yaw);

        return Normalize(right);
    }

    Vec3 IScript::GetUp(Entity entity) const {
        // Up is always world up in this simple implementation
        // For more complex scenarios, you might want to calculate it from forward and right
        return Vec3(0.0f, 1.0f, 0.0f);
    }

    //=========================================================================
    // Hierarchy Operations
    //=========================================================================

    Entity IScript::GetParent(Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(INVALID_ENTITY);

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
            return INVALID_ENTITY;

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
        return transform.parent;
    }

    size_t IScript::GetChildCount(Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(0);

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
            return 0;

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
        return transform.children.size();
    }

    Entity IScript::GetChild(size_t index, Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(INVALID_ENTITY);

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
            return INVALID_ENTITY;

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);

        if (index >= transform.children.size())
            return INVALID_ENTITY;

        return transform.children[index];
    }

    std::vector<Entity> IScript::GetChildren(Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(std::vector<Entity>());

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
            return std::vector<Entity>();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
        return transform.children;
    }

    //=========================================================================
    // Rigidbody Physics
    //=========================================================================

    bool IScript::HasRigidbody(Entity entity) const {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        return Physics::PhysicsManager::EntityHasPhysicsBody(targetEntity);
    }

    float IScript::GetMass(Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(0.0f);

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity))
            return 0.0f;

        return m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity).mass;
    }

    void IScript::SetMass(float mass, Entity entity) {
        CHECK_CONTEXT_OR_RETURN();

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity)) {
            auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity);
            rigidbody.mass = mass;
        }
    }

    bool IScript::GetUseGravity(Entity entity) const {
        CHECK_CONTEXT_OR_RETURN(false);

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity))
            return false;

        return m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity).useGravity;
    }

    void IScript::SetUseGravity(bool use, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!Physics::PhysicsManager::EntityHasPhysicsBody(targetEntity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(targetEntity);
        Physics::PhysicsManager::SetGravityEnabled(bodyID, use);

        // Also update Rigidbody component if it exists
        if (m_context && m_context->componentManager &&
            m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity)) {
            auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity);
            rigidbody.useGravity = use;
        }
    }

    bool IScript::IsStatic(Entity entity) const {
        if (!m_context || !m_context->componentManager) return false;

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity))
            return false;

        return m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity).isStatic;
    }

    void IScript::SetStatic(bool isStatic, Entity entity) {
        if (!m_context || !m_context->componentManager) return;

        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity)) {
            auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity);
            rigidbody.isStatic = isStatic;
        }
    }

    void IScript::LockRotation(bool lockX, bool lockY, bool lockZ, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!Physics::PhysicsManager::EntityHasPhysicsBody(targetEntity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(targetEntity);
        Physics::PhysicsManager::LockRotation(bodyID, lockX, lockY, lockZ);
    }

    Vec3 IScript::GetVelocity(Entity entity) const {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!Physics::PhysicsManager::EntityHasPhysicsBody(targetEntity)) {
            return Vec3::Zero();
        }

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(targetEntity);
        return ToSDKVec3(Physics::PhysicsManager::GetLinearVelocity(bodyID));
    }

    void IScript::SetVelocity(const Vec3& velocity, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!Physics::PhysicsManager::EntityHasPhysicsBody(targetEntity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(targetEntity);
        Physics::PhysicsManager::SetLinearVelocity(bodyID, ToEngineVec3(velocity));
    }

    void IScript::SetVelocity(float x, float y, float z, Entity entity) {
        SetVelocity(Vec3(x, y, z), entity);
    }

    void IScript::AddForce(const Vec3& force, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!Physics::PhysicsManager::EntityHasPhysicsBody(targetEntity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(targetEntity);
        Physics::PhysicsManager::AddForce(bodyID, ToEngineVec3(force));
    }

    void IScript::AddForce(float x, float y, float z, Entity entity) {
        AddForce(Vec3(x, y, z), entity);
    }

    void IScript::AddImpulse(const Vec3& impulse, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

        if (!Physics::PhysicsManager::EntityHasPhysicsBody(targetEntity)) return;

        uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(targetEntity);
        Physics::PhysicsManager::AddImpulse(bodyID, ToEngineVec3(impulse));
    }

    void IScript::AddImpulse(float x, float y, float z, Entity entity) {
        AddImpulse(Vec3(x, y, z), entity);
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

    bool IScript::HasAudioSource(Entity entity) const {
        if (!m_context || !m_context->componentManager) return false;
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        return m_context->componentManager->HasComponent<ECS::Component::AudioSource>(targetEntity);
    }

    void IScript::PlayAudio(Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);

        // If already playing and not paused, stop first
        if (audioSource.m_channel && audioSource.isPlaying && !audioSource.isPaused) {
            audioSource.m_channel->stop();
        }

        // Reset state - AudioSystem will handle actual playback
        audioSource.isPlaying = true;
        audioSource.isPaused = false;
        audioSource.m_hasPlayed = false; // Trigger playback in AudioSystem
    }

    void IScript::StopAudio(Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);

        if (audioSource.m_channel) {
            audioSource.m_channel->stop();
        }

        audioSource.isPlaying = false;
        audioSource.isPaused = false;
    }

    void IScript::PauseAudio(Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);

        if (audioSource.m_channel && audioSource.isPlaying) {
            audioSource.m_channel->setPaused(true);
            audioSource.isPaused = true;
        }
    }

    void IScript::ResumeAudio(Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);

        if (audioSource.m_channel && audioSource.isPaused) {
            audioSource.m_channel->setPaused(false);
            audioSource.isPaused = false;
        }
    }

    bool IScript::IsAudioPlaying(Entity entity) const {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return false;
        const auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
        return audioSource.isPlaying && !audioSource.isPaused;
    }

    float IScript::GetVolume(Entity entity) const {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return 0.0f;
        auto& audio = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
        return audio.volume;
    }

    void IScript::SetVolume(float volume, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
        audioSource.volume = volume;

        // Apply immediately if playing
        if (audioSource.m_channel) {
            audioSource.m_channel->setVolume(volume);
        }
    }

    float IScript::GetPitch(Entity entity) const {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return 1.0f;
        auto& audio = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
        return audio.pitch;
    }

    void IScript::SetPitch(float pitch, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return;
        auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
        audioSource.pitch = pitch;

        // Apply immediately if playing
        if (audioSource.m_channel) {
            audioSource.m_channel->setPitch(pitch);
        }
    }

    void IScript::SetAudioLoop(bool loop, Entity entity) {
        Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
        if (!HasAudioSource(targetEntity)) return;
        auto& audio = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
        audio.loop = loop;
    }

    //=========================================================================
    // CAMERA OPERATIONS
    //=========================================================================

    bool IScript::HasCamera() const {
        if (!m_context || !m_context->componentManager) return false;
        return m_context->componentManager->HasComponent<ECS::Component::Camera>(m_entity);
    }

    float IScript::GetCameraFOV() const {
        if (!HasCamera()) return 45.0f;
        const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        return camera.fovY;
    }

    void IScript::SetCameraFOV(float fov) {
        if (!HasCamera()) return;
        auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        camera.fovY = fov;
        camera.isDirty = true; // Mark camera projection as needing rebuild
    }

    float IScript::GetCameraAspectRatio() const {
        if (!HasCamera()) return 16.0f / 9.0f;
        const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        return camera.aspectRatio;
    }

    void IScript::SetCameraAspectRatio(float aspectRatio) {
        if (!HasCamera()) return;
        auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        camera.aspectRatio = aspectRatio;
        camera.isDirty = true;
    }

    float IScript::GetCameraNearPlane() const {
        if (!HasCamera()) return 0.1f;
        const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        return camera.nearPlane;
    }

    void IScript::SetCameraNearPlane(float nearPlane) {
        if (!HasCamera()) return;
        auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        camera.nearPlane = nearPlane;
        camera.isDirty = true;
    }

    float IScript::GetCameraFarPlane() const {
        if (!HasCamera()) return 1000.0f;
        const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        return camera.farPlane;
    }

    void IScript::SetCameraFarPlane(float farPlane) {
        if (!HasCamera()) return;
        auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        camera.farPlane = farPlane;
        camera.isDirty = true;
    }

    bool IScript::IsCameraMain() const {
        if (!HasCamera()) return false;
        const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        return camera.isMain;
    }

    void IScript::SetCameraMain(bool isMain) {
        if (!HasCamera()) return;
        auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        camera.isMain = isMain;
    }

    bool IScript::IsCameraActive() const {
        if (!HasCamera()) return false;
        const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        return camera.isActive;
    }

    void IScript::SetCameraActive(bool isActive) {
        if (!HasCamera()) return;
        auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
        camera.isActive = isActive;
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

    //=========================================================================
    // Material UUID Registry (Maps material IDs to UUIDs)
    //=========================================================================

    namespace {
        struct MaterialRegistry {
            std::unordered_map<uint32_t, std::string> idToUUID;
            std::unordered_map<std::string, uint32_t> uuidToID;
            uint32_t nextID = 1; // Start from 1, 0 is reserved for invalid

            uint32_t GetOrCreateID(const std::string& uuid) {
                if (uuid.empty()) return 0;

                auto it = uuidToID.find(uuid);
                if (it != uuidToID.end()) {
                    return it->second;
                }

                // Create new ID
                uint32_t id = nextID++;
                idToUUID[id] = uuid;
                uuidToID[uuid] = id;
                return id;
            }

            std::string GetUUID(uint32_t id) const {
                if (id == 0) return "";
                auto it = idToUUID.find(id);
                return (it != idToUUID.end()) ? it->second : "";
            }
        };

        MaterialRegistry& GetMaterialRegistry() {
            static MaterialRegistry registry;
            return registry;
        }
    }

    MaterialRef IScript::GetMaterialRef(const std::string& materialUUID) const {
        if (materialUUID.empty()) return MaterialRef();

        // Get or create an ID for this material UUID
        uint32_t materialID = GetMaterialRegistry().GetOrCreateID(materialUUID);
        return MaterialRef(materialID);
    }

    //=========================================================================
    // Global helper function for material UUID conversion (exported for SDK)
    //=========================================================================

    /// Get material UUID from MaterialRef (accessible from other modules)
    SCRIPT_API std::string GetMaterialUUIDFromRef(const MaterialRef& materialRef) {
        if (!materialRef.IsValid()) return "";
        return GetMaterialRegistry().GetUUID(materialRef.GetEntity());
    }

    //=========================================================================
    // Prefab UUID Registry (Maps prefab IDs to UUIDs)
    //=========================================================================

    namespace {
        struct PrefabRegistry {
            std::unordered_map<uint32_t, std::string> idToPath;
            std::unordered_map<std::string, uint32_t> PathToID;
            uint32_t nextID = 1; // Start from 1, 0 is reserved for invalid

            uint32_t GetOrCreateID(const std::string& path) {
                if (path.empty()) return 0;

                auto it = PathToID.find(path);
                if (it != PathToID.end()) {
                    return it->second;
                }

                // Create new ID
                uint32_t id = nextID++;
                idToPath[id] = path;
                PathToID[path] = id;
                return id;
            }

            std::string GetPath(uint32_t id) const {
                if (id == 0) return "";
                auto it = idToPath.find(id);
                return (it != idToPath.end()) ? it->second : "";
            }
        };

        PrefabRegistry& GetPrefabRegistry() {
            static PrefabRegistry registry;
            return registry;
        }
    }

    PrefabRef IScript::GetPrefabRef(const std::string& prefabPath) const {
        if (prefabPath.empty()) return PrefabRef();

        // Get or create an ID for this prefab Path
        uint32_t prefabID = GetPrefabRegistry().GetOrCreateID(prefabPath);
        return PrefabRef(prefabID);
    }

    /// Get prefab Path from PrefabRef (accessible from other modules)
    SCRIPT_API std::string GetPrefabPathFromRef(const PrefabRef& prefabRef) {
        if (!prefabRef.IsValid()) return "";
        return GetPrefabRegistry().GetPath(prefabRef.GetEntity());
    }

    //=========================================================================
    // Prefab Instantiation
    //=========================================================================

    Entity IScript::InstantiatePrefab(const PrefabRef& prefabRef, const Vec3& position, const Vec3& rotation) {
        if (!prefabRef.IsValid()) {
            SPD_ERROR("[PrefabRef] Cannot instantiate: Invalid prefab reference");
            return INVALID_ENTITY;
        }

        std::string prefabPath = GetPrefabRegistry().GetPath(prefabRef.GetEntity());
        return InstantiatePrefab(prefabPath, position, rotation);
    }

    Entity IScript::InstantiatePrefab(const std::string& prefabPath, const Vec3& position, const Vec3& rotation) {
        if (prefabPath.empty()) {
            SPD_ERROR("[PrefabRef] Cannot instantiate: Empty prefab path");
            return INVALID_ENTITY;
        }

        if (!m_context || !m_context->componentManager) {
            SPD_ERROR("[PrefabRef] Cannot instantiate: Invalid script context");
            return INVALID_ENTITY;
        }

        try {
            // Check if this is a UUID or a file path
            std::vector<uint32_t> newEntities;
            newEntities = DeserializePrefab(prefabPath);

            if (newEntities.empty()) {
                SPD_ERROR("[PrefabRef] Failed to instantiate prefab: " << prefabPath);
                return INVALID_ENTITY;
            }

            // The root entity is the first entity in the list
            Entity rootEntity = newEntities[0];
            if (m_context->componentManager->HasComponent<ECS::Component::Transform>(rootEntity)) {
                auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(rootEntity);
                transform.localPosition = ToEngineVec3(position);
                transform.localRotationEuler = ToEngineVec3(rotation);
                transform.isDirty = true;
            }

            SPD_INFO("[PrefabRef] Successfully instantiated prefab {} at position ({}, {}, {})",
                prefabPath, position.x, position.y, position.z);

            return rootEntity;

        } catch (const std::exception& e) {
            SPD_ERROR("[PrefabRef] Exception during instantiation: {}", e.what());
            return INVALID_ENTITY;
        } catch (...) {
            SPD_ERROR("[PrefabRef] Unknown exception during prefab instantiation");
            return INVALID_ENTITY;
        }
    }

    // Component ref operations (for stored references)
    Vec3 IScript::GetPosition(const TransformRef& ref) const {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return Vec3::Zero();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        return ToSDKVec3(transform.localPosition);
    }

    void IScript::SetPosition(const TransformRef& ref, const Vec3& pos) {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return;

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        transform.localPosition = ToEngineVec3(pos);
        transform.isDirty = true;
    }

    void IScript::SetPosition(const TransformRef& ref, float x, float y, float z) {
        SetPosition(ref, Vec3(x, y, z));
    }

    Vec3 IScript::GetRotation(const TransformRef& ref) const {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return Vec3::Zero();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        return ToSDKVec3(transform.localRotationEuler);
    }

    void IScript::SetRotation(const TransformRef& ref, const Vec3& rot) {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return;

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        transform.localRotationEuler = ToEngineVec3(rot);
        transform.isDirty = true;
    }

    Vec3 IScript::GetScale(const TransformRef& ref) const {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return Vec3::One();

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        return ToSDKVec3(transform.localScale);
    }

    void IScript::SetScale(const TransformRef& ref, const Vec3& scale) {
        if (!ref.IsValid() || !m_context || !m_context->componentManager) return;

        auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
        transform.localScale = ToEngineVec3(scale);
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

    // Helper to mark a field as containing entity references (needs LUID conversion)
    void IScript::MarkFieldAsEntityReference(const std::string& name) {
        if (!m_context || !m_context->componentManager) return;

        // Access the NativeScript component and mark this field as an entity reference
        if (m_context->componentManager->HasComponent<NE::ECS::Component::NativeScript>(m_entity)) {
            auto& scriptComp = m_context->componentManager->GetComponent<NE::ECS::Component::NativeScript>(m_entity);
            scriptComp.EntityReferenceFields.insert(name);
        }
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
        MarkFieldAsEntityReference(name);  // Track for LUID conversion during scene serialization
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
        MarkFieldAsEntityReference(name);  // Track for LUID conversion during scene serialization
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
        MarkFieldAsEntityReference(name);  // Track for LUID conversion during scene serialization
    }

    void IScript::RegisterMaterialRefField(const std::string& name, MaterialRef* memberPtr) {
        RegisterFieldInternal(
            name,
            "materialref",
            memberPtr,
            [memberPtr]() -> std::string {
                // For MaterialRef, the ownerEntity field stores a material ID
                // Use the material registry to convert ID back to UUID
                if (!memberPtr->IsValid()) {
                    return "";
                }
                return GetMaterialRegistry().GetUUID(memberPtr->GetEntity());
            },
            [this, memberPtr, name](const std::string& value) -> bool {
                try {
                    SPD_DEBUG("[MaterialRef] Setting field " << name);

                    // Empty string means no material
                    if (value.empty()) {
                        *memberPtr = MaterialRef();
                        return true;
                    }

                    // Create MaterialRef from UUID
                    MaterialRef newRef = GetMaterialRef(value);
                    if (!newRef.IsValid()) {
                        SPD_ERROR("[MaterialRef] Failed to create valid MaterialRef from UUID: {}", value);
                        return false;
                    }

                    *memberPtr = newRef;
                    SPD_DEBUG("[MaterialRef] Successfully assigned material to field '{}'", name);
                    return true;
                } catch (const std::exception& e) {
                    SPD_ERROR("[MaterialRef] setValue exception for field '{}': {}", name, e.what());
                    return false;
                } catch (...) {
                    SPD_ERROR("[MaterialRef] setValue unknown exception for field '{}'", name);
                    return false;
                }
            }
        );
    }

    void IScript::RegisterPrefabRefField(const std::string& name, PrefabRef* memberPtr) {
        RegisterFieldInternal(
            name,
            "prefabref",
            memberPtr,
            [memberPtr]() -> std::string {
                // For PrefabRef, the ownerEntity field stores a prefab ID
                // Use the prefab registry to convert ID back to UUID
                if (!memberPtr->IsValid()) {
                    return "";
                }
                return GetPrefabRegistry().GetPath(memberPtr->GetEntity());
            },
            [this, memberPtr, name](const std::string& value) -> bool {
                try {
                    SPD_DEBUG("[PrefabRef] Setting field '{}' to '{}'", name, value.empty() ? "<empty>" : value);

                    // Empty string means no prefab
                    if (value.empty()) {
                        *memberPtr = PrefabRef();
                        return true;
                    }

                    // Create PrefabRef from Path
                    PrefabRef newRef = GetPrefabRef(value);
                    if (!newRef.IsValid()) {
                        SPD_ERROR("[PrefabRef] Failed to create valid PrefabRef from Path: " << value);
                        return false;
                    }

                    *memberPtr = newRef;
                    SPD_DEBUG("[PrefabRef] Successfully assigned prefab to field " << name);
                    return true;
                } catch (const std::exception& e) {
                    SPD_ERROR("[PrefabRef] setValue exception for field " << name << ": " << e.what());
                    return false;
                } catch (...) {
                    SPD_ERROR("[PrefabRef] setValue unknown exception for field " << name);
                    return false;
                }
            }
        );
    }

    void IScript::RegisterMaterialRefVectorField(const std::string& name, std::vector<MaterialRef>* memberPtr) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        FieldRegistry::FieldEntry entry;
        entry.typeToken = "vector<materialref>";
        entry.memberPtr = memberPtr;

        // getValue: Serialize entire vector as "size uuid1 uuid2 ..."
        entry.getValue = [this, memberPtr]() -> std::string {
            std::ostringstream oss;
            oss << memberPtr->size();
            for (const auto& ref : *memberPtr) {
                oss << " ";
                if (ref.IsValid()) {
                    oss << GetMaterialRegistry().GetUUID(ref.GetEntity());
                }
            }
            return oss.str();
        };

        // setValue: Deserialize entire vector from "size uuid1 uuid2 ..."
        entry.setValue = [this, memberPtr](const std::string& value) -> bool {
            try {
                std::istringstream iss(value);
                size_t size;
                iss >> size;

                memberPtr->clear();
                memberPtr->reserve(size);

                for (size_t i = 0; i < size; ++i) {
                    std::string uuid;
                    iss >> uuid;
                    if (uuid.empty()) {
                        memberPtr->push_back(MaterialRef());
                    } else {
                        memberPtr->push_back(GetMaterialRef(uuid));
                    }
                }
                return true;
            } catch (...) {
                return false;
            }
        };

        // Array operations
        entry.getSize = [memberPtr]() -> size_t {
            return memberPtr->size();
        };

        entry.getElement = [this, memberPtr](size_t index) -> std::string {
            if (index >= memberPtr->size()) return "";
            const auto& materialRef = (*memberPtr)[index];
            if (!materialRef.IsValid()) return "";
            // Return UUID for display in editor
            return GetMaterialRegistry().GetUUID(materialRef.GetEntity());
        };

        entry.setElement = [this, memberPtr](size_t index, const std::string& value) -> bool {
            if (index >= memberPtr->size()) return false;

            if (value.empty()) {
                (*memberPtr)[index] = MaterialRef();
                return true;
            }

            // Convert UUID to MaterialRef
            MaterialRef newRef = GetMaterialRef(value);
            (*memberPtr)[index] = newRef;
            return newRef.IsValid();
        };

        entry.addElement = [memberPtr]() -> void {
            memberPtr->push_back(MaterialRef());
        };

        entry.removeElement = [memberPtr](size_t index) -> void {
            if (index < memberPtr->size()) {
                memberPtr->erase(memberPtr->begin() + index);
            }
        };

        m_fieldRegistry->fields[name] = std::move(entry);
    }

    void IScript::RegisterPrefabRefVectorField(const std::string& name, std::vector<PrefabRef>* memberPtr) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        FieldRegistry::FieldEntry entry;
        entry.typeToken = "vector<prefabref>";
        entry.memberPtr = memberPtr;

        // getValue: Serialize entire vector
        entry.getValue = [this, memberPtr]() -> std::string {
            std::ostringstream oss;
            oss << memberPtr->size();
            for (const auto& ref : *memberPtr) {
                oss << " ";
                if (ref.IsValid()) {
                    oss << GetPrefabRegistry().GetPath(ref.GetEntity());
                }
            }
            return oss.str();
        };

        // setValue: Deserialize entire vector from "size uuid1 uuid2 ..."
        entry.setValue = [this, memberPtr](const std::string& value) -> bool {
            try {
                std::istringstream iss(value);
                size_t size;
                iss >> size;

                memberPtr->clear();
                memberPtr->reserve(size);

                for (size_t i = 0; i < size; ++i) {
                    std::string path;
                    iss >> path;
                    if (path.empty()) {
                        memberPtr->push_back(PrefabRef());
                    } else {
                        memberPtr->push_back(GetPrefabRef(path));
                    }
                }
                return true;
            } catch (...) {
                return false;
            }
        };

        // Array operations
        entry.getSize = [memberPtr]() -> size_t {
            return memberPtr->size();
        };

        entry.getElement = [this, memberPtr](size_t index) -> std::string {
            if (index >= memberPtr->size()) return "";
            const auto& prefabRef = (*memberPtr)[index];
            if (!prefabRef.IsValid()) return "";
            // Return Path for display in editor
            return GetPrefabRegistry().GetPath(prefabRef.GetEntity());
        };

        entry.setElement = [this, memberPtr](size_t index, const std::string& value) -> bool {
            if (index >= memberPtr->size()) return false;

            if (value.empty()) {
                (*memberPtr)[index] = PrefabRef();
                return true;
            }

            // Convert Path to PrefabRef
            PrefabRef newRef = GetPrefabRef(value);
            (*memberPtr)[index] = newRef;
            return newRef.IsValid();
        };

        entry.addElement = [memberPtr]() -> void {
            memberPtr->push_back(PrefabRef());
        };

        entry.removeElement = [memberPtr](size_t index) -> void {
            if (index < memberPtr->size()) {
                memberPtr->erase(memberPtr->begin() + index);
            }
        };

        m_fieldRegistry->fields[name] = std::move(entry);
    }

    void IScript::RegisterIntVectorField(const std::string& name, std::vector<int>* memberPtr) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        FieldRegistry::FieldEntry entry;
        entry.typeToken = "vector<int>";
        entry.memberPtr = memberPtr;

        // getValue: Serialize entire vector as "size val1 val2 ..."
        entry.getValue = [memberPtr]() -> std::string {
            std::ostringstream oss;
            oss << memberPtr->size();
            for (const auto& val : *memberPtr) {
                oss << " " << val;
            }
            return oss.str();
        };

        // setValue: Deserialize entire vector
        entry.setValue = [memberPtr](const std::string& value) -> bool {
            try {
                std::istringstream iss(value);
                size_t size;
                iss >> size;

                memberPtr->clear();
                memberPtr->reserve(size);

                for (size_t i = 0; i < size; ++i) {
                    int val;
                    if (!(iss >> val)) return false;
                    memberPtr->push_back(val);
                }
                return true;
            } catch (...) {
                return false;
            }
        };

        // Array operations
        entry.getSize = [memberPtr]() -> size_t {
            return memberPtr->size();
        };

        entry.getElement = [memberPtr](size_t index) -> std::string {
            if (index >= memberPtr->size()) return "";
            return std::to_string((*memberPtr)[index]);
        };

        entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
            if (index >= memberPtr->size()) return false;
            try {
                (*memberPtr)[index] = std::stoi(value);
                return true;
            } catch (...) {
                return false;
            }
        };

        entry.addElement = [memberPtr]() -> void {
            memberPtr->push_back(0);
        };

        entry.removeElement = [memberPtr](size_t index) -> void {
            if (index < memberPtr->size()) {
                memberPtr->erase(memberPtr->begin() + index);
            }
        };

        m_fieldRegistry->fields[name] = std::move(entry);
    }

    void IScript::RegisterFloatVectorField(const std::string& name, std::vector<float>* memberPtr) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        FieldRegistry::FieldEntry entry;
        entry.typeToken = "vector<float>";
        entry.memberPtr = memberPtr;

        // getValue: Serialize entire vector as "size val1 val2 ..."
        entry.getValue = [memberPtr]() -> std::string {
            std::ostringstream oss;
            oss << memberPtr->size();
            for (const auto& val : *memberPtr) {
                oss << " " << val;
            }
            return oss.str();
        };

        // setValue: Deserialize entire vector
        entry.setValue = [memberPtr](const std::string& value) -> bool {
            try {
                std::istringstream iss(value);
                size_t size;
                iss >> size;

                memberPtr->clear();
                memberPtr->reserve(size);

                for (size_t i = 0; i < size; ++i) {
                    float val;
                    if (!(iss >> val)) return false;
                    memberPtr->push_back(val);
                }
                return true;
            } catch (...) {
                return false;
            }
        };

        // Array operations
        entry.getSize = [memberPtr]() -> size_t {
            return memberPtr->size();
        };

        entry.getElement = [memberPtr](size_t index) -> std::string {
            if (index >= memberPtr->size()) return "";
            return std::to_string((*memberPtr)[index]);
        };

        entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
            if (index >= memberPtr->size()) return false;
            try {
                (*memberPtr)[index] = std::stof(value);
                return true;
            } catch (...) {
                return false;
            }
        };

        entry.addElement = [memberPtr]() -> void {
            memberPtr->push_back(0.0f);
        };

        entry.removeElement = [memberPtr](size_t index) -> void {
            if (index < memberPtr->size()) {
                memberPtr->erase(memberPtr->begin() + index);
            }
        };

        m_fieldRegistry->fields[name] = std::move(entry);
    }

    void IScript::RegisterBoolVectorField(const std::string& name, std::vector<bool>* memberPtr) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        FieldRegistry::FieldEntry entry;
        entry.typeToken = "vector<bool>";
        entry.memberPtr = memberPtr;

        // getValue: Serialize entire vector as "size val1 val2 ..."
        entry.getValue = [memberPtr]() -> std::string {
            std::ostringstream oss;
            oss << memberPtr->size();
            for (size_t i = 0; i < memberPtr->size(); ++i) {
                oss << " " << ((*memberPtr)[i] ? "1" : "0");
            }
            return oss.str();
        };

        // setValue: Deserialize entire vector
        entry.setValue = [memberPtr](const std::string& value) -> bool {
            try {
                std::istringstream iss(value);
                size_t size;
                iss >> size;

                memberPtr->clear();
                memberPtr->reserve(size);

                for (size_t i = 0; i < size; ++i) {
                    std::string val;
                    if (!(iss >> val)) return false;
                    memberPtr->push_back(val == "1" || val == "true");
                }
                return true;
            } catch (...) {
                return false;
            }
        };

        // Array operations
        entry.getSize = [memberPtr]() -> size_t {
            return memberPtr->size();
        };

        entry.getElement = [memberPtr](size_t index) -> std::string {
            if (index >= memberPtr->size()) return "";
            return (*memberPtr)[index] ? "1" : "0";
        };

        entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
            if (index >= memberPtr->size()) return false;
            (*memberPtr)[index] = (value == "1" || value == "true");
            return true;
        };

        entry.addElement = [memberPtr]() -> void {
            memberPtr->push_back(false);
        };

        entry.removeElement = [memberPtr](size_t index) -> void {
            if (index < memberPtr->size()) {
                memberPtr->erase(memberPtr->begin() + index);
            }
        };

        m_fieldRegistry->fields[name] = std::move(entry);
    }

    void IScript::RegisterStringVectorField(const std::string& name, std::vector<std::string>* memberPtr) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        FieldRegistry::FieldEntry entry;
        entry.typeToken = "vector<string>";
        entry.memberPtr = memberPtr;

        // getValue: Serialize entire vector as "size val1 val2 ..."
        // Strings are encoded with length prefix to handle spaces: "2 5:hello 5:world"
        entry.getValue = [memberPtr]() -> std::string {
            std::ostringstream oss;
            oss << memberPtr->size();
            for (const auto& str : *memberPtr) {
                oss << " " << str.length() << ":" << str;
            }
            return oss.str();
        };

        // setValue: Deserialize entire vector
        entry.setValue = [memberPtr](const std::string& value) -> bool {
            try {
                std::istringstream iss(value);
                size_t size;
                iss >> size;

                memberPtr->clear();
                memberPtr->reserve(size);

                for (size_t i = 0; i < size; ++i) {
                    size_t len;
                    char colon;
                    if (!(iss >> len >> colon) || colon != ':') return false;

                    // Read the exact number of characters (including spaces)
                    iss.ignore(1); // Skip the space after colon
                    std::string str(len, '\0');
                    if (len > 0) {
                        iss.read(&str[0], len);
                        if (iss.gcount() != static_cast<std::streamsize>(len)) return false;
                    }
                    memberPtr->push_back(str);
                }
                return true;
            } catch (...) {
                return false;
            }
        };

        // Array operations
        entry.getSize = [memberPtr]() -> size_t {
            return memberPtr->size();
        };

        entry.getElement = [memberPtr](size_t index) -> std::string {
            if (index >= memberPtr->size()) return "";
            return (*memberPtr)[index];
        };

        entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
            if (index >= memberPtr->size()) return false;
            (*memberPtr)[index] = value;
            return true;
        };

        entry.addElement = [memberPtr]() -> void {
            memberPtr->push_back("");
        };

        entry.removeElement = [memberPtr](size_t index) -> void {
            if (index < memberPtr->size()) {
                memberPtr->erase(memberPtr->begin() + index);
            }
        };

        m_fieldRegistry->fields[name] = std::move(entry);
    }

    void IScript::RegisterEntityVectorField(const std::string& name, std::vector<Entity>* memberPtr) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        FieldRegistry::FieldEntry entry;
        entry.typeToken = "vector<entity>";
        entry.memberPtr = memberPtr;

        // getValue: Serialize entire vector as "size id1 id2 ..."
        entry.getValue = [memberPtr]() -> std::string {
            std::ostringstream oss;
            oss << memberPtr->size();
            for (const auto& entity : *memberPtr) {
                oss << " " << entity;
            }
            return oss.str();
        };

        // setValue: Deserialize entire vector from "size id1 id2 ..."
        entry.setValue = [memberPtr](const std::string& value) -> bool {
            try {
                std::istringstream iss(value);
                size_t size;
                iss >> size;

                memberPtr->clear();
                memberPtr->reserve(size);

                for (size_t i = 0; i < size; ++i) {
                    uint32_t entityId;
                    if (!(iss >> entityId)) return false;
                    memberPtr->push_back(static_cast<Entity>(entityId));
                }
                return true;
            } catch (...) {
                return false;
            }
        };

        // Array operations
        entry.getSize = [memberPtr]() -> size_t {
            return memberPtr->size();
        };

        entry.getElement = [memberPtr](size_t index) -> std::string {
            if (index >= memberPtr->size()) return "";
            return std::to_string((*memberPtr)[index]);
        };

        entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
            if (index >= memberPtr->size()) return false;
            try {
                (*memberPtr)[index] = static_cast<Entity>(std::stoul(value));
                return true;
            } catch (...) {
                return false;
            }
        };

        entry.addElement = [memberPtr]() -> void {
            memberPtr->push_back(NO_ENTITY);
        };

        entry.removeElement = [memberPtr](size_t index) -> void {
            if (index < memberPtr->size()) {
                memberPtr->erase(memberPtr->begin() + index);
            }
        };

        m_fieldRegistry->fields[name] = std::move(entry);
        MarkFieldAsEntityReference(name);  // Track for LUID conversion during scene serialization
    }

    void IScript::RegisterLayerMaskField(const std::string& name, LayerMask* memberPtr) {
        RegisterFieldInternal(
            name,
            "layermask",
            memberPtr,
            // getValue: Return mask as string
            [memberPtr]() -> std::string {
                return std::to_string(memberPtr->mask);
            },
            // setValue: Set mask from string
            [memberPtr](const std::string& value) -> bool {
                try {
                    memberPtr->mask = std::stoul(value);
                    return true;
                } catch (...) {
                    return false;
                }
            }
        );

        // Set LayerMask callbacks for editor access
        SetFieldLayerMaskCallbacks(name,
            [memberPtr]() -> uint32_t {
                return memberPtr->mask;
            },
            [memberPtr](uint32_t value) {
                memberPtr->mask = value;
            }
        );
    }

    //=========================================================================
    // Helper Methods for Template Functions
    //=========================================================================

    void IScript::SetFieldEnumOptions(const std::string& name, const std::vector<std::string>& options) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        auto it = m_fieldRegistry->fields.find(name);
        if (it != m_fieldRegistry->fields.end()) {
            it->second.enumOptions = options;
        }
    }

    void IScript::SetFieldEnumCallbacks(const std::string& name,
        std::function<int()> getEnumValue,
        std::function<void(int)> setEnumValue) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        auto it = m_fieldRegistry->fields.find(name);
        if (it != m_fieldRegistry->fields.end()) {
            it->second.getEnumValue = getEnumValue;
            it->second.setEnumValue = setEnumValue;
        }
    }

    void IScript::SetFieldLayerMaskCallbacks(const std::string& name,
        std::function<uint32_t()> getLayerMaskValue,
        std::function<void(uint32_t)> setLayerMaskValue) {
        if (!m_fieldRegistry) {
            m_fieldRegistry = new FieldRegistry();
        }

        auto it = m_fieldRegistry->fields.find(name);
        if (it != m_fieldRegistry->fields.end()) {
            it->second.getLayerMaskValue = getLayerMaskValue;
            it->second.setLayerMaskValue = setLayerMaskValue;
        }
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
        if (!m_fieldRegistry) return {};

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && !it->second.enumOptions.empty()) {
            return it->second.enumOptions;
        }
        return {};
    }

    int IScript::GetEnumValue(const std::string& fieldName) const {
        if (!m_fieldRegistry) return 0;

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && it->second.getEnumValue) {
            return it->second.getEnumValue();
        }
        return 0;
    }

    void IScript::SetEnumValue(const std::string& fieldName, int value) {
        if (!m_fieldRegistry) return;

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && it->second.setEnumValue) {
            it->second.setEnumValue(value);
        }
    }

    uint32_t IScript::GetLayerMaskValue(const std::string& fieldName) const {
        if (!m_fieldRegistry) return 0;

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && it->second.getLayerMaskValue) {
            return it->second.getLayerMaskValue();
        }
        return 0;
    }

    void IScript::SetLayerMaskValue(const std::string& fieldName, uint32_t value) {
        if (!m_fieldRegistry) return;

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && it->second.setLayerMaskValue) {
            it->second.setLayerMaskValue(value);
        }
    }

    size_t IScript::GetArraySize(const std::string& fieldName) const {
        if (!m_fieldRegistry) return 0;

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && it->second.getSize) {
            return it->second.getSize();
        }
        return 0;
    }

    std::string IScript::GetArrayElement(const std::string& fieldName, size_t index) const {
        if (!m_fieldRegistry) return "";

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && it->second.getElement) {
            return it->second.getElement(index);
        }
        return "";
    }

    bool IScript::SetArrayElement(const std::string& fieldName, size_t index, const std::string& value) {
        if (!m_fieldRegistry) return false;

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && it->second.setElement) {
            return it->second.setElement(index, value);
        }
        return false;
    }

    void IScript::AddArrayElement(const std::string& fieldName) {
        if (!m_fieldRegistry) return;

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && it->second.addElement) {
            it->second.addElement();
        }
    }

    void IScript::RemoveArrayElement(const std::string& fieldName, size_t index) {
        if (!m_fieldRegistry) return;

        auto it = m_fieldRegistry->fields.find(fieldName);
        if (it != m_fieldRegistry->fields.end() && it->second.removeElement) {
            it->second.removeElement(index);
        }
    }

    template<typename T>
    void IScript::MarkComponentDirty() {
        // Only mark dirty in Edit mode - runtime changes should not be serialized
        if (NE::GetEngineState() != NE::EngineState::Edit) {
            return;
        }

        if (!m_context->componentManager || !m_context->componentManager->HasComponent<T>(m_entity)) {
            return;
        }

        auto& component = m_context->componentManager->GetComponent<T>(m_entity);

        // Use C++20 requires to check if the component has an isDirty field
        if constexpr (requires { component.isDirty; }) {
            component.isDirty = true;
        }
    }

    // === Entity Active State Functions ===

    bool IScript::IsActive(Entity e) const {
        if (!m_context->componentManager) return false;

        if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(e))
            return true; // Default to active if no EntityMeta

        return m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(e).isActive;
    }

    void IScript::SetActive(bool active, Entity entity) {
        if (!m_context->componentManager) return;

        Entity e = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

    if (m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(e)) {
            auto& meta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(e);

    // Only update if changed
    if (meta.isActive != active) {
         meta.isActive = active;

 // 1. Update rendering visibility
    if (m_context->componentManager->HasComponent<NE::ECS::Component::Renderer>(e)) {
        auto& renderer = m_context->componentManager->GetComponent<NE::ECS::Component::Renderer>(e);
 renderer.visible = active && IsActiveInHierarchy();
      }

     // 2. Update physics state
      if (NE::Physics::PhysicsManager::EntityHasPhysicsBody(e)) {
        uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(e);

    if (active && IsActiveInHierarchy()) {
               // Reactivate physics body only if parent hierarchy is also active
    NE::Physics::PhysicsManager::ActivateBody(bodyID);
      }
               else {
    // Deactivate physics body (stops collision and physics simulation)
      NE::Physics::PhysicsManager::DeactivateBody(bodyID);
          }
 }

                // 3. Update script enabled state (NEW!)
           // When entity becomes inactive in hierarchy, the ScriptSystem will skip Update()
       // No need to manually disable here - the hierarchy check in ScriptSystem handles it

                // 4. Recursively propagate to all children (Unity-style)
                if (m_context->componentManager->HasComponent<NE::ECS::Component::Transform>(e)) {
         auto& transform = m_context->componentManager->GetComponent<NE::ECS::Component::Transform>(e);
      PropagateActiveStateToChildren(transform.children, active);
   }

    // Mark scene dirty ONLY when called from a running script in Edit mode
    // Not during scene deserialization or Play mode
  if (NE::GetEngineState() == NE::EngineState::Edit && m_hasStarted) {
          NE::MarkSceneDirty();
}
        }
        }
    }

    bool IScript::IsActiveInHierarchy() const {
        if (!m_context || !m_context->componentManager) return false;

        // Check if this entity is active
        if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(m_entity)) {
            return true; // Default to active if no EntityMeta
        }

        auto& meta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(m_entity);
        if (!meta.isActive) {
            return false; // This entity is disabled
        }

        // Check if any parent in the hierarchy is disabled
        if (!m_context->componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity)) {
            return true; // No parent, just check self
        }

        auto& transform = m_context->componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity);
        if (transform.parent == NE::ECS::Component::INVALID_PARENT) {
            return true; // No parent, entity is active
        }

        // Recursively check parent active state
        Entity currentParent = transform.parent;
        while (currentParent != NE::ECS::Component::INVALID_PARENT) {
            if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(currentParent)) {
                break; // Parent has no EntityMeta, assume active
            }

            auto& parentMeta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(currentParent);
            if (!parentMeta.isActive) {
                return false; // Parent is disabled, so this entity is inactive in hierarchy
            }

            // Move up the hierarchy
            if (!m_context->componentManager->HasComponent<NE::ECS::Component::Transform>(currentParent)) {
                break; // No transform on parent, we're done
            }

            auto& parentTransform = m_context->componentManager->GetComponent<NE::ECS::Component::Transform>(currentParent);
            currentParent = parentTransform.parent;
        }

        return true; // All parents are active
    }

    void IScript::PropagateActiveStateToChildren(const std::vector<uint32_t>& children, bool parentActive) const {
        if (!m_context || !m_context->componentManager) return;

        for (Entity childEntity : children) {
            // Get child's own isActive state
            if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(childEntity)) {
                continue;
            }

            auto& childMeta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(childEntity);
            
            // Determine effective active state: parent must be active AND child must be active
            bool effectiveActive = parentActive && childMeta.isActive;

            // Update child's rendering
            if (m_context->componentManager->HasComponent<NE::ECS::Component::Renderer>(childEntity)) {
                auto& renderer = m_context->componentManager->GetComponent<NE::ECS::Component::Renderer>(childEntity);
                renderer.visible = effectiveActive;
            }

          // Update child's physics
      if (NE::Physics::PhysicsManager::EntityHasPhysicsBody(childEntity)) {
        uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(childEntity);

  if (effectiveActive) {
  NE::Physics::PhysicsManager::ActivateBody(bodyID);
           }
            else {
  NE::Physics::PhysicsManager::DeactivateBody(bodyID);
    }
     }

  // Recursively propagate to grandchildren
            if (m_context->componentManager->HasComponent<NE::ECS::Component::Transform>(childEntity)) {
         auto& childTransform = m_context->componentManager->GetComponent<NE::ECS::Component::Transform>(childEntity);
  PropagateActiveStateToChildren(childTransform.children, effectiveActive);
   }
        }
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

    //=========================================================================
    // TWEEN API IMPLEMENTATION (Wrapper to adapt lambdas to TweenManager)
    //=========================================================================

    // Wrapper objects that adapt lambda callbacks to member function pointers
    // These are lightweight adapters - TweenManager handles all the actual tweening logic

    // Wrapper for lambda-based tweens (receives normalized time 0-1)
    struct LambdaTweenWrapper {
        std::function<void(float)> callback;
        Entity entity;

        void SetValue(float value) {
            if (callback) {
                callback(value);
            }
        }
    };

    // Wrapper for Vec3 tweens
    struct Vec3TweenWrapper {
        std::function<void(const Vec3&)> callback;
        Entity entity;

        void SetValue(const Vec3& value) {
            if (callback) {
                callback(value);
            }
        }
    };

    // Wrapper for float tweens
    struct FloatTweenWrapper {
        std::function<void(float)> callback;
        Entity entity;

        void SetValue(float value) {
            if (callback) {
                callback(value);
            }
        }
    };

    // Global tween wrapper tracking for cleanup
    static std::unordered_map<TweenHandle, void*> s_tweenWrappers;
    static TweenHandle s_nextTweenHandle = 1;

    // Helper to convert SDK TweenType to engine TweenType
    inline ::TweenType ToEngineTweenType(TweenType type) {
        return static_cast<::TweenType>(static_cast<int>(type));
    }

    TweenHandle StartTweenLambda(
        std::function<void(float)> updateFunc,
        float duration,
        TweenType type,
        Entity entity)
    {
        // Create wrapper and call TweenManager::StartTween
        auto* wrapper = new LambdaTweenWrapper{updateFunc, entity};

        TweenManager::Get().StartTween(
            wrapper,
            &LambdaTweenWrapper::SetValue,
            0.0f,
            1.0f,
            duration,
            ToEngineTweenType(type)
        );

        TweenHandle handle = s_nextTweenHandle++;
        s_tweenWrappers[handle] = wrapper;

        return handle;
    }

    TweenHandle StartTweenVec3(
        std::function<void(const Vec3&)> setter,
        const Vec3& start,
        const Vec3& end,
        float duration,
        TweenType type,
        Entity entity)
    {
        // Create wrapper and call TweenManager::StartTween
        auto* wrapper = new Vec3TweenWrapper{setter, entity};

        TweenManager::Get().StartTween(
            wrapper,
            &Vec3TweenWrapper::SetValue,
            start,
            end,
            duration,
            ToEngineTweenType(type)
        );

        TweenHandle handle = s_nextTweenHandle++;
        s_tweenWrappers[handle] = wrapper;

        return handle;
    }

    TweenHandle StartTweenFloat(
        std::function<void(float)> setter,
        float start,
        float end,
        float duration,
        TweenType type,
        Entity entity)
    {
        // Create wrapper and call TweenManager::StartTween
        auto* wrapper = new FloatTweenWrapper{setter, entity};

        TweenManager::Get().StartTween(
            wrapper,
            &FloatTweenWrapper::SetValue,
            start,
            end,
            duration,
            ToEngineTweenType(type)
        );

        TweenHandle handle = s_nextTweenHandle++;
        s_tweenWrappers[handle] = wrapper;

        return handle;
    }

    bool CheckEntityTween(Entity entity) {
        // Check if any wrapper belongs to this entity
        for (const auto& [handle, wrapperPtr] : s_tweenWrappers) {
            // Try each wrapper type
            auto* lambdaWrapper = static_cast<LambdaTweenWrapper*>(wrapperPtr);
            if (TweenManager::Get().CheckTween(lambdaWrapper) && lambdaWrapper->entity == entity) {
                return true;
            }
        }
        return false;
    }

    void StopTween(TweenHandle handle) {
        auto it = s_tweenWrappers.find(handle);
        if (it != s_tweenWrappers.end()) {
            // The wrapper will be cleaned up automatically by TweenManager when tween becomes inactive
            // We just remove our handle tracking
            s_tweenWrappers.erase(it);
        }
    }

    void StopEntityTweens(Entity entity) {
        // Remove all wrapper handles for this entity
        // TweenManager will clean up the actual tweens when they become inactive
        for (auto it = s_tweenWrappers.begin(); it != s_tweenWrappers.end();) {
            void* wrapperPtr = it->second;

            // Check wrapper entity (simplified type check)
            bool shouldErase = false;
            if (auto* wrapper = static_cast<LambdaTweenWrapper*>(wrapperPtr)) {
                if (wrapper->entity == entity) {
                    shouldErase = true;
                }
            }

            if (shouldErase) {
                it = s_tweenWrappers.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ClearAllTweens() {
        // Use TweenManager's Clean() function to clear all tweens
        TweenManager::Get().Clean();
        s_tweenWrappers.clear();
    }

} // namespace Scripting
} // namespace NE
