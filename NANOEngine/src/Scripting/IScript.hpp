#pragma once

#include <string>
#include <vector>
#include <sstream>
#include "../ECS/Core/ComponentManager.hpp"
#include "../Math/Vec3.hpp"
#include "../EngineState.hpp"  // For checking Edit vs Play mode

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

namespace NE::ECS::Component {
	struct Transform;
	struct Rigidbody;
	struct AudioSource;
}

//namespace NE::Math {
//    struct Vec3;
//}

//namespace NE::Core {
//  class Transform;
//    class GameObject;
//}

/**
 * Base interface that all game scripts must implement.
 * This interface is provided by the Engine DLL and used by Game DLLs.
 */
class ENGINE_API IScript {
public:
	/**
 * Component reference structure - stores direct pointer to component
	 */
	template<typename T>
	struct ComponentRef {
		T* componentPtr = nullptr;
		NE::ECS::Entity ownerEntity = 0; // Store which entity owns this component

		ComponentRef() = default;
		ComponentRef(T* ptr) : componentPtr(ptr) {}

		// Implicit conversion to bool for null checks
		operator bool() const {
			return componentPtr != nullptr;
		}

		// Access the component
		T* Get() const {
			return componentPtr;
		}

		// Arrow operator for convenient access
		T* operator->() const {
			return componentPtr;
		}

		// Dereference operator
		T& operator*() const {
			return *componentPtr;
		}

		// Set the component pointer and owner entity
		void Set(T* ptr, NE::ECS::Entity entity) {
			componentPtr = ptr;
			ownerEntity = entity;
		}

		// Legacy method - sets pointer without entity tracking
		void Set(T* ptr) {
			componentPtr = ptr;
			ownerEntity = 0;
		}

		// Get the component pointer
		T* GetPtr() const {
			return componentPtr;
		}

		// Get the owner entity ID
		NE::ECS::Entity GetOwnerEntity() const {
			return ownerEntity;
		}
	};

	/**
	 * Raycast hit information structure.
	 */
	struct RaycastHit {
		bool hasHit = false;           // Did the ray hit anything?
		NE::Math::Vec3 point;          // World position where ray hit
		NE::Math::Vec3 normal;         // Surface normal at hit point
		float distance = 0.0f;         // Distance from ray origin to hit point
		NE::ECS::Entity entity = 0;    // Entity that was hit
	};

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
	virtual void OnDestroy() { m_componentManager = nullptr; }

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
	 * Mark a component as dirty for serialization (Editor Mode only).
	 * This is automatically called by helper functions, but can be called manually
	 * when directly modifying component fields in Editor Mode scripts.
	 */
	template<typename T>
	void MarkComponentDirty();

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

	NE::ECS::Component::Transform& GetTransform(NE::ECS::Entity entt);

	// === Unity-Style Transform Direction Vectors ===

	/**
	 * Get the forward vector (blue axis) of the entity in world space.
	 * This vector points in the direction the entity is facing.
	 * Rotating the entity will change this direction.
	 * @return Normalized forward direction vector
	 */
	NE::Math::Vec3 GetForward() const;

	/**
	 * Get the right vector (red axis) of the entity in world space.
	 * This vector points to the right of the entity.
	 * @return Normalized right direction vector
	 */
	NE::Math::Vec3 GetRight() const;

	/**
	 * Get the up vector (green axis) of the entity in world space.
	 * This vector points upward from the entity.
	 * @return Normalized up direction vector
	 */
	NE::Math::Vec3 GetUp() const;

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

	/**
	 * Lock or unlock rotation axes on the rigidbody.
	 * Useful for preventing player characters from tipping over.
	 * @param lockX Lock rotation around X axis (pitch)
	 * @param lockY Lock rotation around Y axis (yaw)
	 * @param lockZ Lock rotation around Z axis (roll)
	 */
	void LockRotation(bool lockX, bool lockY, bool lockZ);

	/**
	 * Get the linear velocity of the rigidbody.
	 * @return Velocity as Vec3, or (0,0,0) if no Rigidbody component
	 */
	NE::Math::Vec3 GetVelocity() const;

	/**
	 * Set the linear velocity of the rigidbody.
	 * @param velocity New velocity
	 */
	void SetVelocity(const NE::Math::Vec3& velocity);

	/**
	 * Set the linear velocity of the rigidbody.
	 * @param x X velocity
	 * @param y Y velocity
	 * @param z Z velocity
	 */
	void SetVelocity(float x, float y, float z);

	/**
	 * Add force to the rigidbody (affected by mass).
	 * @param force Force vector to apply
	 */
	void AddForce(const NE::Math::Vec3& force);

	/**
	 * Add force to the rigidbody.
	 * @param x X force
	 * @param y Y force
	 * @param z Z force
	 */
	void AddForce(float x, float y, float z);

	/**
	 * Add impulse to the rigidbody (instant velocity change, affected by mass).
	 * @param impulse Impulse vector to apply
	 */
	void AddImpulse(const NE::Math::Vec3& impulse);

	/**
	 * Add impulse to the rigidbody.
	 * @param x X impulse
	 * @param y Y impulse
	 * @param z Z impulse
	 */
	void AddImpulse(float x, float y, float z);

	// === Physics Raycasting Functions ===
	// In IScript class:
	RaycastHit Raycast(const NE::Math::Vec3& origin, const NE::Math::Vec3& direction, float maxDistance, uint32_t layerMask = 0xFFFFFFFF) const;
	RaycastHit Raycast(float originX, float originY, float originZ, float dirX, float dirY, float dirZ, float maxDistance, uint32_t layerMask = 0xFFFFFFFF) const;
	std::vector<RaycastHit> RaycastAll(const NE::Math::Vec3& origin, const NE::Math::Vec3& direction, float maxDistance, uint32_t layerMask = 0xFFFFFFFF) const;

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

	// === Built-in field management system ===

	/**
	 * Return a list of exposed field names.
	 * This is now implemented in the base class.
	 */
	virtual std::vector<std::string> GetExposedFieldNames() const;

	/**
		* Return the type token for a named field. Example tokens: "bool","int","float","vec3","string","enum"
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

	// === Enum Field Support (Public for Editor access) ===

 /**
	 * Get the list of possible enum values for a field (editor support).
	 * Return empty vector if field is not an enum.
	 * @param fieldName Name of the field
  * @return Vector of enum option names (e.g., {"Grunt", "Elite", "Boss"})
	 */
	virtual std::vector<std::string> GetEnumOptions(const std::string& fieldName) const {
		(void)fieldName;
		return {};
	}

	/**
	   * Get the current enum value index for a field.
	   * @param fieldName Name of the enum field
	   * @return Current enum value as integer index
	*/
	virtual int GetEnumValue(const std::string& fieldName) const {
		(void)fieldName;
		return 0;
	}

	/**
	 * Set the enum value by index.
   * @param fieldName Name of the enum field
	 * @param value Enum value as integer index
	 */
	virtual void SetEnumValue(const std::string& fieldName, int value) {
		(void)fieldName;
		(void)value;
	}

	// === Array/Vector Field Support (Public for Editor access) ===

	/**
	 * Get the size of an array/vector field.
	 * @param fieldName Name of the array field
	 * @return Number of elements in the array
	 */
	virtual size_t GetArraySize(const std::string& fieldName) const {
		(void)fieldName;
		return 0;
	}

	/**
   * Get an array element as a string.
	 * @param fieldName Name of the array field
	 * @param index Index of the element
	 * @return Element value as string
   */
	virtual std::string GetArrayElement(const std::string& fieldName, size_t index) const {
		(void)fieldName;
		(void)index;
		return "";
	}

	/**
	 * Set an array element from a string.
	 * @param fieldName Name of the array field
	 * @param index Index of the element
	 * @param value New value as string
	 * @return true if successful
	 */
	virtual bool SetArrayElement(const std::string& fieldName, size_t index, const std::string& value) {
		(void)fieldName;
		(void)index;
		(void)value;
		return false;
	}

	/**
 * Add a new element to the end of an array/vector.
	 * @param fieldName Name of the array field
	 */
	virtual void AddArrayElement(const std::string& fieldName) {
		(void)fieldName;
	}

	/**
	 * Remove an element from an array/vector.
	 * @param fieldName Name of the array field
	 * @param index Index of the element to remove
	 */
	virtual void RemoveArrayElement(const std::string& fieldName, size_t index) {
		(void)fieldName;
		(void)index;
	}

	/**
	 * Check if Start() has been called on this script.
	 * Used internally by ScriptSystem.
	 * @return true if Start() has been called, false otherwise
	 */
	bool HasStarted() const { return m_hasStarted; }

	/**
	 * Internal method called by ScriptSystem to mark Start() as called.
	 * Should not be called by user code.
	 */
	void MarkStartCalled() { m_hasStarted = true; }

	/**
	 * Refresh all component references by resolving entity IDs to pointers.
	 * Called automatically when entering Play mode or after hot reload.
	 * This ensures references remain valid across scene transitions.
	 */
	void RefreshComponentReferences();

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

	/**
	 * Register a component reference field for editor exposure.
	 * This allows scripts to reference components from other entities.
	 * Example: RegisterComponentRefField<Transform>("playerTransform", &m_playerTransform);
	 * @param name Field name to display in editor
	 * @param memberPtr Pointer to the ComponentRef<T> member variable
	 */
	template<typename T>
	void RegisterComponentRefField(const std::string& name, ComponentRef<T>* memberPtr);

private:
	// Forward declaration to hide implementation details from DLL interface
	class FieldRegistry;

	NE::ECS::Entity m_entity = 0;
	bool m_enabled = true;
	bool m_hasStarted = false;

	// Use PIMPL pattern to hide std containers from DLL interface
	FieldRegistry* m_fieldRegistry = nullptr;

protected:
	NE::ECS::ComponentManager* m_componentManager = nullptr;
};

// Explicit template instantiation declarations for component reference registration
// These ensure the template specializations are exported from the Engine DLL
extern template ENGINE_API void IScript::RegisterComponentRefField<NE::ECS::Component::Transform>(const std::string&, ComponentRef<NE::ECS::Component::Transform>*);
extern template ENGINE_API void IScript::RegisterComponentRefField<NE::ECS::Component::Rigidbody>(const std::string&, ComponentRef<NE::ECS::Component::Rigidbody>*);
extern template ENGINE_API void IScript::RegisterComponentRefField<NE::ECS::Component::AudioSource>(const std::string&, ComponentRef<NE::ECS::Component::AudioSource>*);

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

// Component reference macro
#ifndef SCRIPT_COMPONENT_REF
#define SCRIPT_COMPONENT_REF(fieldName, ComponentType) \
    RegisterComponentRefField<NE::ECS::Component::ComponentType>(#fieldName, &this->fieldName)
#endif

template<typename T>
bool IScript::HasComponent() const {
	if (!m_componentManager) {
		return false;
	}
	return m_componentManager->HasComponent<T>(m_entity);
}

template<typename T>
void IScript::MarkComponentDirty() {
	// Only mark dirty in Edit mode - runtime changes should not be serialized
	if (NE::GetEngineState() != NE::EngineState::Edit) {
		return;
	}

	if (!m_componentManager || !m_componentManager->HasComponent<T>(m_entity)) {
		return;
	}

	auto& component = m_componentManager->GetComponent<T>(m_entity);
	
	// Use C++20 requires to check if the component has an isDirty field
	if constexpr (requires { component.isDirty; }) {
		component.isDirty = true;
	}
}