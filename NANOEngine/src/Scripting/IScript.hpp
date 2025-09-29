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
     * Called when the script is first attached to an entity.
     * Use this for one-time initialization.
     * @param entity The entity this script is attached to
     */
    virtual void Initialize(NE::ECS::Entity entity) = 0;

    /**
     * Called every frame to update the script.
     * @param deltaTime Time elapsed since last frame in seconds
     */
    virtual void Update(double deltaTime) = 0;

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

private:
    NE::ECS::Entity m_entity = 0;
    bool m_enabled = true;

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

