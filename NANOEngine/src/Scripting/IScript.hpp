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

    // === Built-in field management system ===
    
    /**
     * Return a list of exposed field names.
     * This is now implemented in the base class.
     */
    virtual std::vector<std::string> GetExposedFieldNames() const;

    /**
     * Return the type token for a named field. Example tokens: "bool","int","float","vec3","string"
     * This is now implemented in the base class.
     */
    virtual std::string GetFieldType(const std::string& name) const;

    /**
     * Get the current field value as a string. The format for complex types (eg vec3) is up to the script,
     * but the Editor will use a simple whitespace-separated list for vec3: "x y z".
     * This is now implemented in the base class.
     */
    virtual std::string GetFieldValueAsString(const std::string& name) const;

    /**
     * Set the field value from a string. Return true if successful.
     * This is now implemented in the base class.
     */
    virtual bool SetFieldValueFromString(const std::string& name, const std::string& value);

protected:
    // === Protected field registration methods ===
    // Scripts should call these in their constructor to register fields
    
    /**
     * Register a float field for editor exposure.
     */
    void RegisterFloatField(const std::string& name, float* memberPtr);
    
    /**
     * Register an int field for editor exposure.
     */
    void RegisterIntField(const std::string& name, int* memberPtr);
    
    /**
     * Register a bool field for editor exposure.
     */
    void RegisterBoolField(const std::string& name, bool* memberPtr);
    
    /**
     * Register a string field for editor exposure.
     */
    void RegisterStringField(const std::string& name, std::string* memberPtr);
    
    /**
     * Register a Vec3 field for editor exposure.
     */
    void RegisterVec3Field(const std::string& name, NE::Math::Vec3* memberPtr);

private:
    // Forward declaration to hide implementation details from DLL interface
    class FieldRegistry;
    
    NE::ECS::Entity m_entity = 0;
    bool m_enabled = true;
    
    // Use PIMPL pattern to hide std containers from DLL interface
    FieldRegistry* m_fieldRegistry = nullptr;

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

// === Convenience macros for field registration ===
// Use these in your script constructor to easily register fields

#ifndef SCRIPT_REGISTER_FIELD
#define SCRIPT_REGISTER_FIELD(fieldName, fieldType) \
    Register##fieldType##Field(#fieldName, &this->fieldName)
#endif

// Specific type macros for cleaner code
#ifndef SCRIPT_FIELD
#define SCRIPT_FIELD(fieldName, fieldType) \
    SCRIPT_REGISTER_FIELD(fieldName, fieldType)
#endif

