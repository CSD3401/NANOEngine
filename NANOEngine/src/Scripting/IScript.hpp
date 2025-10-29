#pragma once

#include <string>
#include <vector>
#include "../ECS/Core/ComponentManager.hpp"

// Export macros for when Engine is built as DLL
#ifdef NANOENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif

// Forward declarations
namespace NE::ECS {
    using Entity = unsigned int;
}

namespace NE::Math {
  struct Vec3;
}

//namespace NE::Core {
//    class Transform;
//    class GameObject;
//}

/**
 * Base interface that all game scripts must implement.
 * This interface is provided by the Engine DLL and used by Game DLLs.
 */
class ENGINE_API IScript {
public:
    virtual ~IScript();

    /**
     * Called when the script is first created, even if disabled.
     * Use this for initialization that needs to happen regardless of enabled state.
     * Called before Initialize().
     */
    virtual void Awake() {}

    /**
     * Called when the script is first attached to an entity.
     * Use this for one-time initialization.
     * @param entity The entity this script is attached to
     */
    virtual void Initialize(NE::ECS::Entity entity) = 0;

    /**
     * Called before the first Update() call, only if the script is enabled.
     * Use this for initialization that should only happen when active.
     */
    virtual void Start() {}

    /**
     * Called every frame to update the script.
     * @param deltaTime Time elapsed since last frame in seconds
     */
    virtual void Update(double deltaTime) = 0;

    /**
     * Called when script values are changed in the editor (editor-only).
  * Use this to validate or respond to inspector changes.
     */
 virtual void OnValidate() {}

    /**
     * Called when the script is being destroyed.
     * Use this for cleanup operations.
     */
    virtual void OnDestroy() { delete m_componentManager; }

    /**
     * Called when the script is enabled.
     * Scripts start enabled by default.
     */
    virtual void OnEnable() {}

    /**
     * Called when the script is disabled.
     */
    virtual void OnDisable() {}

    /**
     * Get the entity this script is attached to.
     * @return Entity ID
     */
    NE::ECS::Entity GetEntity() const;

    /**
     * Check if the script is currently enabled.
     * @return true if enabled, false otherwise
     */
    bool IsEnabled() const { return m_enabled; }

    /**
     * Enable or disable the script.
     * Disabled scripts don't receive Update calls.
     * @param enabled New enabled state
     */
    void SetEnabled(bool enabled) {
        if (m_enabled != enabled) {
            m_enabled = enabled;
            if (enabled) {
                OnEnable();
            }
            else {
                OnDisable();
            }
        }
    }

    /**
     * Get a user-friendly name for this script type.
     * Override this to provide a custom name for editor display.
     * @return Script type name
     */
    virtual const char* GetTypeName() const { return "IScript"; }

    // === Event System (Optional) ===

    /**
     * Called when this entity collides with another entity.
     * Only called if both entities have collision components.
     * @param other The other entity involved in the collision
     */
    virtual void OnCollisionEnter(NE::ECS::Entity other) = 0;

    /**
     * Called when this entity stops colliding with another entity.
     * @param other The other entity that we stopped colliding with
     */
    virtual void OnCollisionExit(NE::ECS::Entity other) = 0;

    /**
     * Called when this entity triggers another entity.
     * Triggers are collision areas that don't block movement.
     * @param other The other entity that entered our trigger
     */
    virtual void OnTriggerEnter(NE::ECS::Entity other) = 0;

    /**
     * Called when this entity stops triggering another entity.
     * @param other The other entity that exited our trigger
     */
    virtual void OnTriggerExit(NE::ECS::Entity other) = 0;

    /**
     * Set the entity this script is attached to.
     * This should only be called by the scripting system.
     * @param entity The entity ID
     */
    void SetEntity(NE::ECS::Entity entity) { m_entity = entity; }

    /**
     * Links the script to the engine's core systems.
     * Called by the ScriptSystem immediately after creation.
     */
    void LinkToEngine(NE::ECS::ComponentManager* componentManager);

    /**
     * Gets another component attached to the same entity as this script.
     * @return A pointer to the component, or nullptr if not found.
     */
    template<typename T>
    T* GetComponent() const;

    /**
     * Check if the entity has a specific component.
     * @return true if the component exists, false otherwise
     */
    template<typename T>
    bool HasComponent() const;

    // === Unity-Style Transform Helper Functions ===

    /**
     * Get the current position of the entity.
     * @return Position as Vec3, or (0,0,0) if no Transform component
     */
    NE::Math::Vec3 GetPosition() const;

    /**
     * Set the position of the entity.
     * @param pos New position
     */
    void SetPosition(const NE::Math::Vec3& pos);

    /**
     * Set the position of the entity.
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     */
    void SetPosition(float x, float y, float z);

    /**
     * Get the current rotation of the entity.
     * @return Rotation as Vec3 (euler angles), or (0,0,0) if no Transform component
     */
    NE::Math::Vec3 GetRotation() const;

    /**
     * Set the rotation of the entity.
     * @param rot New rotation (euler angles)
     */
    void SetRotation(const NE::Math::Vec3& rot);

    /**
     * Set the rotation of the entity.
     * @param x X rotation (pitch)
     * @param y Y rotation (yaw)
     * @param z Z rotation (roll)
     */
    void SetRotation(float x, float y, float z);

    /**
     * Get the current scale of the entity.
     * @return Scale as Vec3, or (1,1,1) if no Transform component
     */
    NE::Math::Vec3 GetScale() const;

    /**
     * Set the scale of the entity.
     * @param scale New scale
     */
    void SetScale(const NE::Math::Vec3& scale);

    /**
     * Set the scale of the entity.
     * @param x X scale
     * @param y Y scale
     * @param z Z scale
     */
    void SetScale(float x, float y, float z);

    /**
     * Set uniform scale on all axes.
     * @param uniformScale Scale value for all axes
     */
    void SetScale(float uniformScale);

    /**
     * Move the entity by a translation vector.
     * @param translation Movement vector
     */
    void Translate(const NE::Math::Vec3& translation);

    /**
     * Move the entity by specified amounts on each axis.
     * @param x X movement
     * @param y Y movement
     * @param z Z movement
     */
    void Translate(float x, float y, float z);

    /**
     * Rotate the entity by euler angles.
     * @param rotation Rotation to apply (euler angles)
     */
    void Rotate(const NE::Math::Vec3& rotation);

    /**
     * Rotate the entity by specified amounts on each axis.
     * @param x X rotation
     * @param y Y rotation
     * @param z Z rotation
     */
    void Rotate(float x, float y, float z);

    // === Unity-Style Rigidbody Helper Functions ===

    /**
     * Check if the entity has a Rigidbody component.
     * @return true if Rigidbody exists, false otherwise
     */
    bool HasRigidbody() const;

    /**
     * Get the mass of the rigidbody.
     * @return Mass value, or 0 if no Rigidbody component
     */
    float GetMass() const;

    /**
     * Set the mass of the rigidbody.
     * @param mass New mass value
     */
    void SetMass(float mass);

    /**
     * Check if the rigidbody uses gravity.
     * @return true if gravity enabled, false otherwise
     */
    bool GetUseGravity() const;

    /**
     * Enable or disable gravity for the rigidbody.
     * @param use Whether to use gravity
     */
    void SetUseGravity(bool use);

    /**
     * Check if the rigidbody is static.
     * @return true if static, false otherwise
     */
    bool IsStatic() const;

    /**
     * Set whether the rigidbody is static.
     * @param isStatic Whether the body should be static
     */
    void SetStatic(bool isStatic);

    // === Unity-Style AudioSource Helper Functions ===

    /**
     * Check if the entity has an AudioSource component.
   * @return true if AudioSource exists, false otherwise
     */
  bool HasAudioSource() const;

    /**
     * Play the audio clip attached to this entity's AudioSource.
     */
    void PlayAudio();

    /**
     * Stop the audio playing on this entity's AudioSource.
     */
    void StopAudio();

    /**
     * Pause the audio playing on this entity's AudioSource.
     */
    void PauseAudio();

    /**
 * Resume the audio playing on this entity's AudioSource.
     */
    void ResumeAudio();

    /**
     * Check if the AudioSource is currently playing.
     * @return true if playing, false otherwise
     */
    bool IsAudioPlaying() const;

    /**
     * Get the volume of the AudioSource.
     * @return Volume (0.0 to 1.0), or 0 if no AudioSource
     */
    float GetVolume() const;

    /**
     * Set the volume of the AudioSource.
 * @param volume Volume (0.0 to 1.0)
     */
  void SetVolume(float volume);

    /**
     * Get the pitch of the AudioSource.
     * @return Pitch multiplier, or 1.0 if no AudioSource
     */
    float GetPitch() const;

    /**
     * Set the pitch of the AudioSource.
     * @param pitch Pitch multiplier
 */
    void SetPitch(float pitch);

    /**
     * Set whether the audio should loop.
     * @param loop Whether to loop
     */
    void SetAudioLoop(bool loop);

    // === Runtime editable scripting fields support ===
    // Scripts that want to expose editable fields to the editor can override
    // these methods. We keep the API string-based to avoid reflection across DLLs.

    /**
     * Return a list of exposed field names.
     */
    virtual std::vector<std::string> GetExposedFieldNames() const { return {}; }

    /**
     * Return the type token for a named field. Example tokens: "bool","int","float","vec3","string"
     */
    virtual std::string GetFieldType(const std::string& name) const { (void)name; return std::string(); }

    /**
     * Get the current field value as a string. The format for complex types (eg vec3) is up to the script,
     * but the Editor will use a simple whitespace-separated list for vec3: "x y z".
     */
    virtual std::string GetFieldValueAsString(const std::string& name) const { (void)name; return std::string(); }

    /**
     * Set the field value from a string. Return true if successful.
     */
    virtual bool SetFieldValueFromString(const std::string& name, const std::string& value) { (void)name; (void)value; return false; }

    /**
     * Check if Start() has been called on this script.
     * @return true if Start() has been called, false otherwise
     */
    bool HasStarted() const { return m_hasStarted; }

    /**
   * Internal method called by ScriptSystem to mark Start() as called.
     * Should not be called by user code.
     */
    void MarkStartCalled() { m_hasStarted = true; }

private:
    NE::ECS::Entity m_entity = 0;
    bool m_enabled = true;
    bool m_hasStarted = false;

protected:
    NE::ECS::ComponentManager* m_componentManager = nullptr;

};

template<typename T>
T* IScript::GetComponent() const {
    if (!m_componentManager) {
        return nullptr;
    }
    // Assumes ComponentManager has a method like this
    return &m_componentManager->GetComponent<T>(m_entity);
}

template<typename T>
bool IScript::HasComponent() const {
    if (!m_componentManager) {
        return false;
    }
    return m_componentManager->HasComponent<T>(m_entity);
}

