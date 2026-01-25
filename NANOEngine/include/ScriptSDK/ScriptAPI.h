/**
 * @file ScriptAPI.h
 * @brief Main interface for NANOEngine game scripts
 *
 * This is the primary header that game scripts should include. It provides
 * a complete scripting interface with ZERO dependencies on engine internals.
 *
 * Usage in game scripts:
 * @code
 * #include <ScriptSDK/ScriptAPI.h>
 *
 * class MyScript : public NE::Scripting::IScript {
 *     void Initialize(NE::Scripting::Entity entity) override { ... }
 *     void Update(double deltaTime) override { ... }
 * };
 * @endcode
 */

#pragma once

#include "ScriptTypes.h"
#include "Reflection.h"
#include <functional>
#include <sstream>

namespace NE {
namespace Scripting {

    // Forward declaration of internal context (PIMPL pattern)
    class ScriptContext;

    //=========================================================================
    // BASE SCRIPT INTERFACE
    //=========================================================================

    /**
     * @class IScript
     * @brief Base interface for all game scripts
     *
     * All user scripts must inherit from this class and implement the
     * pure virtual methods. The engine manages script lifetime and calls
     * lifecycle methods at appropriate times.
     *
     * This interface is completely standalone - it uses only types defined
     * in ScriptTypes.h, with no engine header dependencies.
     */
    class SCRIPT_API IScript {
    public:
        virtual ~IScript();

        //=====================================================================
        // LIFECYCLE METHODS (Override these in your scripts)
        //=====================================================================

        /**
         * Called when the script is first created, even if disabled.
         * Use for initialization needed regardless of enabled state.
         * Called before Initialize().
         */
        virtual void Awake() {}

        /**
         * Called when script is attached to an entity.
         * Use for one-time initialization.
         * @param entity The entity this script is attached to
         */
        virtual void Initialize(Entity entity) = 0;

        /**
         * Called before first Update(), only if script is enabled.
         * Use for initialization that should only happen when active.
         */
        virtual void Start() {}

        /**
         * Called every frame to update the script.
         * @param deltaTime Time elapsed since last frame (seconds)
         */
        virtual void Update(double deltaTime) = 0;

        /**
         * Called when script values change in editor (editor-only).
         * Use to validate or respond to inspector changes.
         */
        virtual void OnValidate() {}

        /**
         * Called when the script is being destroyed.
         * Use for cleanup operations.
         */
        virtual void OnDestroy() {}

        /**
         * Called when the script is enabled.
         */
        virtual void OnEnable() {}

        /**
         * Called when the script is disabled.
         */
        virtual void OnDisable() {}

        //=====================================================================
        // Dirty Scripts Stuff - Anson Pls Check
        //=====================================================================
        /**
        * Mark a component as dirty for serialization (Editor Mode only).
        * This is automatically called by helper functions, but can be called manually
        * when directly modifying component fields in Editor Mode scripts.
        */
        template<typename T>
        void MarkComponentDirty();

        /**
        * Check if the entity is currently active.
        * Inactive entities don't update scripts or render.
        * @return true if active, false otherwise
        */
        bool IsActive(Entity e) const;

        /**
         * Check if the entity is active in the hierarchy.
         * An entity is active in hierarchy if both it and all its parents are active.
         * This is Unity-style behavior where parent active state affects children.
         * @return true if active in hierarchy, false otherwise
         */
        bool IsActiveInHierarchy() const;

        /**
         * Set the active state of the entity.
         * Inactive entities stop updating scripts and rendering.
         * Useful for hiding/disabling game objects.
         * @param active New active state
         * @param entity Target entity (default: this entity)
         */
        void SetActive(bool active, Entity entity = DEFAULT_ENTITY_PARAM);

        //=====================================================================
        // COLLISION/TRIGGER EVENTS
        //=====================================================================

        /**
         * Called when this entity collides with another entity.
         * @param other The other entity involved in collision
         */
        virtual void OnCollisionEnter(Entity other) = 0;

        /**
         * Called when this entity stops colliding with another entity.
         * @param other The other entity
         */
        virtual void OnCollisionExit(Entity other) = 0;

        /**
         * Called when this entity triggers another entity.
         * @param other The other entity that entered trigger
         */
        virtual void OnTriggerEnter(Entity other) = 0;

        /**
         * Called when this entity stops triggering another entity.
         * @param other The other entity that exited trigger
         */
        virtual void OnTriggerExit(Entity other) = 0;

        //=====================================================================
        // ENTITY & SCRIPT STATE
        //=====================================================================

        /**
         * Get the entity this script is attached to.
         * @return Entity ID
         */
        Entity GetEntity() const { return m_entity; }

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
        void SetEnabled(bool enabled);

        /**
         * Get a user-friendly name for this script type.
         * Override to provide custom name for editor display.
         * @return Script type name
         */
        virtual const char* GetTypeName() const { return "IScript"; }

        //=====================================================================
        // ENTITY METADATA
        //=====================================================================

        /**
         * Get the name of an entity.
         * @param entity Entity to query (defaults to this script's entity)
         * @return Entity name string
         */
        std::string GetEntityName(Entity entity = DEFAULT_ENTITY_PARAM) const;

        /**
         * Set the name of an entity.
         * @param name New name for the entity
         * @param entity Entity to modify (defaults to this script's entity)
         */
        void SetEntityName(const std::string& name, Entity entity = DEFAULT_ENTITY_PARAM);

        /**
         * Get the layer of an entity.
         * @param entity Entity to query (defaults to this script's entity)
         * @return Layer index (0-255)
         */
        uint8_t GetLayer(Entity entity = DEFAULT_ENTITY_PARAM) const;

        /**
         * Set the layer of an entity.
         * @param layer New layer index (0-255)
         * @param entity Entity to modify (defaults to this script's entity)
         */
        void SetLayer(uint8_t layer, Entity entity = DEFAULT_ENTITY_PARAM);

        /**
         * Check if an entity is part of a prefab instance.
         * @param entity Entity to query (defaults to this script's entity)
         * @return true if entity is part of a prefab
         */
        bool IsPrefabInstance(Entity entity = DEFAULT_ENTITY_PARAM) const;

        /**
         * Check if an entity is a prefab root.
         * @param entity Entity to query (defaults to this script's entity)
         * @return true if entity is the root of a prefab instance
         */
        bool IsPrefabRoot(Entity entity = DEFAULT_ENTITY_PARAM) const;

        //=====================================================================
        // TRANSFORM OPERATIONS (TF_*)
        // All functions support optional Entity parameter:
        // - If not specified, operates on this script's entity (m_entity)
        // - If specified, operates on the target entity
        //=====================================================================

        // Position
        Vec3 TF_GetPosition(Entity entity = DEFAULT_ENTITY_PARAM) const;
        Vec3 TF_GetLocalPosition(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void TF_SetPosition(const Vec3& pos, Entity entity = DEFAULT_ENTITY_PARAM);
        void TF_SetPosition(float x, float y, float z, Entity entity = DEFAULT_ENTITY_PARAM);

        // Rotation (Euler angles in degrees)
        Vec3 TF_GetRotation(Entity entity = DEFAULT_ENTITY_PARAM) const;
        Vec3 TF_GetLocalRotation(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void TF_SetRotation(const Vec3& rot, Entity entity = DEFAULT_ENTITY_PARAM);
        void TF_SetRotation(float x, float y, float z, Entity entity = DEFAULT_ENTITY_PARAM);

        // Scale
        Vec3 TF_GetScale(Entity entity = DEFAULT_ENTITY_PARAM) const;
        Vec3 TF_GetLocalScale(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void TF_SetScale(const Vec3& scale, Entity entity = DEFAULT_ENTITY_PARAM);
        void TF_SetScale(float x, float y, float z, Entity entity = DEFAULT_ENTITY_PARAM);
        void TF_SetScale(float uniformScale, Entity entity = DEFAULT_ENTITY_PARAM);

        // Relative transforms
        void TF_Translate(const Vec3& translation, Entity entity = DEFAULT_ENTITY_PARAM);
        void TF_Translate(float x, float y, float z, Entity entity = DEFAULT_ENTITY_PARAM);

        void TF_Rotate(const Vec3& rotation, Entity entity = DEFAULT_ENTITY_PARAM);
        void TF_Rotate(float x, float y, float z, Entity entity = DEFAULT_ENTITY_PARAM);

        // Direction vectors (based on rotation)
        Vec3 TF_GetForward(Entity entity = DEFAULT_ENTITY_PARAM) const;
        Vec3 TF_GetRight(Entity entity = DEFAULT_ENTITY_PARAM) const;
        Vec3 TF_GetUp(Entity entity = DEFAULT_ENTITY_PARAM) const;

        //=====================================================================
        // HIERARCHY OPERATIONS
        // All functions support optional Entity parameter
        //=====================================================================

        /**
         * Get the parent entity.
         * @param entity Target entity (default: this entity)
         * @return Parent entity ID, or INVALID_ENTITY if no parent
         */
        Entity GetParent(Entity entity = DEFAULT_ENTITY_PARAM) const;

        /**
         * Get the number of child entities.
         * @param entity Target entity (default: this entity)
         * @return Number of children
         */
        size_t GetChildCount(Entity entity = DEFAULT_ENTITY_PARAM) const;

        /**
         * Get a child entity by index.
         * @param index The index of the child (0-based)
         * @param entity Target entity (default: this entity)
         * @return Child entity ID, or INVALID_ENTITY if index out of range
         */
        Entity GetChild(size_t index, Entity entity = DEFAULT_ENTITY_PARAM) const;

        /**
         * Get all child entities.
         * @param entity Target entity (default: this entity)
         * @return Vector of child entity IDs
         */
        std::vector<Entity> GetChildren(Entity entity = DEFAULT_ENTITY_PARAM) const;

        //=====================================================================
        // RIGIDBODY PHYSICS (RB_*)
        // All functions support optional Entity parameter
        //=====================================================================

        bool RB_HasRigidbody(Entity entity = DEFAULT_ENTITY_PARAM) const;

        // Mass
        float RB_GetMass(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void RB_SetMass(float mass, Entity entity = DEFAULT_ENTITY_PARAM);

        // Gravity
        bool RB_GetUseGravity(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void RB_SetUseGravity(bool use, Entity entity = DEFAULT_ENTITY_PARAM);

        // Static/Dynamic
        bool RB_IsStatic(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void RB_SetStatic(bool isStatic, Entity entity = DEFAULT_ENTITY_PARAM);

        // Rotation locking
        void RB_LockRotation(bool lockX, bool lockY, bool lockZ, Entity entity = DEFAULT_ENTITY_PARAM);

        // Velocity
        Vec3 RB_GetVelocity(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void RB_SetVelocity(const Vec3& velocity, Entity entity = DEFAULT_ENTITY_PARAM);
        void RB_SetVelocity(float x, float y, float z, Entity entity = DEFAULT_ENTITY_PARAM);

        // Angular Velocity (Rotation)
        Vec3 RB_GetAngularVelocity(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void RB_SetAngularVelocity(const Vec3& angularVelocity, Entity entity = DEFAULT_ENTITY_PARAM);
        void RB_SetAngularVelocity(float x, float y, float z, Entity entity = DEFAULT_ENTITY_PARAM);

        // Forces
        void RB_AddForce(const Vec3& force, Entity entity = DEFAULT_ENTITY_PARAM);
        void RB_AddForce(float x, float y, float z, Entity entity = DEFAULT_ENTITY_PARAM);

        // Impulses
        void RB_AddImpulse(const Vec3& impulse, Entity entity = DEFAULT_ENTITY_PARAM);
        void RB_AddImpulse(float x, float y, float z, Entity entity = DEFAULT_ENTITY_PARAM);

        //=====================================================================
        // CHARACTER CONTROLLER PHYSICS (CC_*)
        // All functions support optional Entity parameter
        //=====================================================================

		void CC_Move(const Vec3& displacement, Entity entity = DEFAULT_ENTITY_PARAM);
		void CC_Rotate(float yawDegrees, Entity entity = DEFAULT_ENTITY_PARAM);
        bool CC_IsGrounded(Entity entity = DEFAULT_ENTITY_PARAM) const;
		Vec3 CC_GetGroundNormal(Entity entity = DEFAULT_ENTITY_PARAM) const;

        //=====================================================================
        // PHYSICS RAYCASTING
        //=====================================================================

        RaycastHit Raycast(const Vec3& origin, const Vec3& direction,
                           float maxDistance, uint32_t layerMask = 0xFFFFFFFF) const;

        RaycastHit Raycast(float originX, float originY, float originZ,
                           float dirX, float dirY, float dirZ,
                           float maxDistance, uint32_t layerMask = 0xFFFFFFFF) const;

        std::vector<RaycastHit> RaycastAll(const Vec3& origin, const Vec3& direction,
                                            float maxDistance, uint32_t layerMask = 0xFFFFFFFF) const;

        RaycastHit SphereCast(const Vec3& origin, float radius, const Vec3& direction,
                              float maxDistance, uint32_t layerMask = 0xFFFFFFFF) const;

        RaycastHit SphereCast(float originX, float originY, float originZ,
                              float radius,
                              float dirX, float dirY, float dirZ,
                              float maxDistance, uint32_t layerMask = 0xFFFFFFFF) const;

        //=====================================================================
        // AUDIO SOURCE
        // All functions support optional Entity parameter
        //=====================================================================

        bool HasAudioSource(Entity entity = DEFAULT_ENTITY_PARAM) const;

        // Playback control
        void PlayAudio(Entity entity = DEFAULT_ENTITY_PARAM);
        void StopAudio(Entity entity = DEFAULT_ENTITY_PARAM);
        void PauseAudio(Entity entity = DEFAULT_ENTITY_PARAM);
        void ResumeAudio(Entity entity = DEFAULT_ENTITY_PARAM);
        bool IsAudioPlaying(Entity entity = DEFAULT_ENTITY_PARAM) const;

        // Volume control
        float GetVolume(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void SetVolume(float volume, Entity entity = DEFAULT_ENTITY_PARAM);

        // Pitch control
        float GetPitch(Entity entity = DEFAULT_ENTITY_PARAM) const;
        void SetPitch(float pitch, Entity entity = DEFAULT_ENTITY_PARAM);

        // Loop control
        void SetAudioLoop(bool loop, Entity entity = DEFAULT_ENTITY_PARAM);


     //=====================================================================
     // CAMERA OPERATIONS
     //=====================================================================

     /**
      * Check if the entity has a camera component.
      * @return true if the entity has a camera component, false otherwise
      */
        bool HasCamera() const;

        /**
         * Get the field of view of the camera.
         * @return Field of view in degrees
         */
        float GetCameraFOV() const;

        /**
         * Set the field of view of the camera.
         * @param fov New field of view in degrees
         */
        void SetCameraFOV(float fov);

        /**
         * Get the aspect ratio of the camera.
         * @return Aspect ratio (width / height)
         */
        float GetCameraAspectRatio() const;

        /**
         * Set the aspect ratio of the camera.
         * @param aspectRatio New aspect ratio (width / height)
         */
        void SetCameraAspectRatio(float aspectRatio);

        /**
         * Get the near clipping plane distance of the camera.
         * @return Near plane distance
         */
        float GetCameraNearPlane() const;

        /**
         * Set the near clipping plane distance of the camera.
         * @param nearPlane New near plane distance
         */
        void SetCameraNearPlane(float nearPlane);

        /**
         * Get the far clipping plane distance of the camera.
         * @return Far plane distance
         */
        float GetCameraFarPlane() const;

        /**
         * Set the far clipping plane distance of the camera.
         * @param farPlane New far plane distance
         */
        void SetCameraFarPlane(float farPlane);

        /**
         * Check if this camera is the main camera.
         * @return true if this is the main camera, false otherwise
         */
        bool IsCameraMain() const;

        /**
         * Set this camera as the main camera.
         * @param isMain New main camera state
         */
        void SetCameraMain(bool isMain);

        /**
         * Check if the camera is currently active.
         * @return true if the camera is active, false otherwise
         */
        bool IsCameraActive() const;

        /**
         * Set the active state of the camera.
         * @param isActive New active state
         */
        void SetCameraActive(bool isActive);


        //=====================================================================
        // COMPONENT ACCESS (Type-safe opaque handles)
        //=====================================================================

        /**
         * Check if entity has a specific component type.
         * @tparam THandle Component handle type (TransformHandle, etc.)
         * @return true if component exists
         */
        template<typename THandle>
        bool HasComponent() const;

        /**
         * Get a reference to another entity's transform.
         * Use this to store references to other entities' components.
         * @param entity Target entity
         * @return Transform reference (check IsValid() before use)
         */
        TransformRef GetTransformRef(Entity entity) const;

        /**
         * Get a reference to another entity's rigidbody.
         * @param entity Target entity
         * @return Rigidbody reference (check IsValid() before use)
         */
        RigidbodyRef GetRigidbodyRef(Entity entity) const;

        /**
         * Get a reference to another entity's renderer.
         * @param entity Target entity
         * @return Renderer reference (check IsValid() before use)
         */
        RendererRef GetRendererRef(Entity entity) const;

        /**
         * Get a reference to another entity's audio source.
         * @param entity Target entity
         * @return AudioSource reference (check IsValid() before use)
         */
        AudioSourceRef GetAudioSourceRef(Entity entity) const;

        /**
         * Get a reference to a material asset by UUID.
         * @param materialUUID Material asset UUID string
         * @return Material reference (check IsValid() before use)
         */
        MaterialRef GetMaterialRef(const std::string& materialUUID) const;

        /**
         * Get the material reference from an entity's renderer component.
         * Convenience helper that combines GetMaterial() and GetMaterialRef().
         * @param entity Entity ID to get material from
         * @return Material reference (check IsValid() before use)
         */
        MaterialRef GetEntityMaterial(Entity entity) const;

        /**
         * Get a reference to a prefab asset by UUID.
         * Use this to store references to prefabs that can be instantiated at runtime.
         * @param prefabUUID Prefab asset UUID string
         * @return Prefab reference (check IsValid() before use)
         */
        PrefabRef GetPrefabRef(const std::string& prefabUUID) const;

        //=====================================================================
        // PREFAB INSTANTIATION
        //=====================================================================

        /**
         * Instantiate a prefab at a specific position and rotation.
         * @param prefabRef The prefab reference to instantiate
         * @param position World position for the instantiated prefab
         * @param rotation World rotation (Euler angles in degrees) for the instantiated prefab
         * @return Root entity of the instantiated prefab, or INVALID_ENTITY if failed
         */
        Entity InstantiatePrefab(const PrefabRef& prefabRef, const Vec3& position = Vec3::Zero(), const Vec3& rotation = Vec3::Zero());

        /**
         * Instantiate a prefab by UUID at a specific position and rotation.
         * @param prefabUUID The prefab asset UUID string
         * @param position World position for the instantiated prefab
         * @param rotation World rotation (Euler angles in degrees) for the instantiated prefab
         * @return Root entity of the instantiated prefab, or INVALID_ENTITY if failed
         */
        Entity InstantiatePrefab(const std::string& prefabUUID, const Vec3& position = Vec3::Zero(), const Vec3& rotation = Vec3::Zero());

        //=====================================================================
        // COMPONENT REF OPERATIONS (For stored references)
        //=====================================================================

        // Transform operations on ComponentRef
        Vec3 GetPosition(const TransformRef& ref) const;
        void SetPosition(const TransformRef& ref, const Vec3& pos);
        void SetPosition(const TransformRef& ref, float x, float y, float z);

        Vec3 GetRotation(const TransformRef& ref) const;
        void SetRotation(const TransformRef& ref, const Vec3& rot);

        Vec3 GetScale(const TransformRef& ref) const;
        void SetScale(const TransformRef& ref, const Vec3& scale);

        // Rigidbody operations on ComponentRef
        Vec3 GetVelocity(const RigidbodyRef& ref) const;
        void SetVelocity(const RigidbodyRef& ref, const Vec3& velocity);
        void AddForce(const RigidbodyRef& ref, const Vec3& force);

        // Renderer operations on ComponentRef
        MaterialRef GetMaterialRef(const RendererRef& ref) const;
        void SetMaterialRef(const RendererRef& ref, const MaterialRef& materialRef);

        //=====================================================================
        // FIELD REGISTRATION FOR EDITOR (Protected - use macros in scripts)
        //=====================================================================

    protected:
        void RegisterFloatField(const std::string& name, float* memberPtr);
        void RegisterIntField(const std::string& name, int* memberPtr);
        void RegisterBoolField(const std::string& name, bool* memberPtr);
        void RegisterStringField(const std::string& name, std::string* memberPtr);
        void RegisterVec3Field(const std::string& name, Vec3* memberPtr);

        void RegisterTransformRefField(const std::string& name, TransformRef* memberPtr);
        void RegisterRigidbodyRefField(const std::string& name, RigidbodyRef* memberPtr);
        void RegisterRendererRefField(const std::string& name, RendererRef* memberPtr);
        void RegisterAudioSourceRefField(const std::string& name, AudioSourceRef* memberPtr);
        void RegisterMaterialRefField(const std::string& name, MaterialRef* memberPtr);
        void RegisterPrefabRefField(const std::string& name, PrefabRef* memberPtr);
        void RegisterGameObjectRefField(const std::string& name, GameObjectRef* memberPtr);
        void RegisterLayerRefField(const std::string& name, LayerRef* memberPtr);

        // Vector field registration (native support - no override boilerplate needed!)
        void RegisterIntVectorField(const std::string& name, std::vector<int>* memberPtr);
        void RegisterFloatVectorField(const std::string& name, std::vector<float>* memberPtr);
        void RegisterBoolVectorField(const std::string& name, std::vector<bool>* memberPtr);
        void RegisterStringVectorField(const std::string& name, std::vector<std::string>* memberPtr);
        void RegisterEntityVectorField(const std::string& name, std::vector<Entity>* memberPtr);
        void RegisterMaterialRefVectorField(const std::string& name, std::vector<MaterialRef>* memberPtr);
        void RegisterPrefabRefVectorField(const std::string& name, std::vector<PrefabRef>* memberPtr);
        void RegisterGameObjectRefVectorField(const std::string& name, std::vector<GameObjectRef>* memberPtr);
        void RegisterTransformRefVectorField(const std::string& name, std::vector<TransformRef>* memberPtr);
        void RegisterRigidbodyRefVectorField(const std::string& name, std::vector<RigidbodyRef>* memberPtr);
        void RegisterRendererRefVectorField(const std::string& name, std::vector<RendererRef>* memberPtr);
        void RegisterAudioSourceRefVectorField(const std::string& name, std::vector<AudioSourceRef>* memberPtr);
        void RegisterLayerRefVectorField(const std::string& name, std::vector<LayerRef>* memberPtr);

        // Enum field registration (with automatic enum options)
        template<typename EnumType>
        void RegisterEnumField(const std::string& name, EnumType* memberPtr, const std::vector<std::string>& enumOptions);

        // LayerMask field registration (multi-select layer picker in editor)
        void RegisterLayerMaskField(const std::string& name, LayerMask* memberPtr);

        // Struct field registration (with reflection support)
        // NOTE: StructType must have NE_REFLECT macros. Include "Reflection.h" in your script.
        template<typename StructType>
        void RegisterStructField(const std::string& structName, StructType* memberPtr);

        //=====================================================================
        // EDITOR FIELD QUERY INTERFACE (Virtual - for advanced use)
        //=====================================================================

    public:
        virtual std::vector<std::string> GetExposedFieldNames() const;
        virtual std::string GetFieldType(const std::string& name) const;
        virtual std::string GetFieldValueAsString(const std::string& name) const;
        virtual bool SetFieldValueFromString(const std::string& name, const std::string& value);

        // Enum field support
        virtual std::vector<std::string> GetEnumOptions(const std::string& fieldName) const;
        virtual int GetEnumValue(const std::string& fieldName) const;
        virtual void SetEnumValue(const std::string& fieldName, int value);

        // LayerMask field support
        virtual uint32_t GetLayerMaskValue(const std::string& fieldName) const;
        virtual void SetLayerMaskValue(const std::string& fieldName, uint32_t value);

        // Array/vector field support
        virtual size_t GetArraySize(const std::string& fieldName) const;
        virtual std::string GetArrayElement(const std::string& fieldName, size_t index) const;
        virtual bool SetArrayElement(const std::string& fieldName, size_t index, const std::string& value);
        virtual void AddArrayElement(const std::string& fieldName);
        virtual void RemoveArrayElement(const std::string& fieldName, size_t index);

        void MarkFieldAsEntityReference(const std::string& name);

        //=====================================================================
        // GAME OBJECT ACCESS (Unity-style)
        //=====================================================================

        /**
         * Get the GameObject wrapper for this script's entity.
         * Similar to Unity's gameObject property.
         *
         * Example:
         * @code
         * // Get another script on the same GameObject
         * auto otherScript = gameObject.GetComponent<OtherScript>();
         * @endcode
         */
        class GameObject gameObject() const;

        /**
         * Get a script component by type on this entity.
         * Shortcut for gameObject.GetComponent<T>().
         * Similar to Unity's GetComponent<T>() on MonoBehaviour.
         *
         * Example:
         * @code
         * // In PlayerScript, access TestScript on the same entity
         * auto testScript = GetComponent<TestScript>();
         * if (testScript) {
         *     testScript->ExampleFunction();
         *     testScript->someValue = 42;
         * }
         * @endcode
         *
         * @tparam T The script type (must inherit from IScript)
         * @return Pointer to the script instance, or nullptr if not found
         */
        template<typename T>
        inline T* GetComponent() const {
            return gameObject().GetComponent<T>();
        }

        /**
         * Check if this entity has a specific script type.
         * Shortcut for gameObject.HasComponent<T>().
         *
         * @tparam T The script type to check for
         * @return true if the script exists on this entity
         */
        template<typename T>
        inline bool HasScript() const {
            return GetComponent<T>() != nullptr;
        }

        /**
         * Get a script by name on this entity.
         * Shortcut for gameObject.GetScript(name).
         *
         * @param scriptName The registered name of the script
         * @return Pointer to the script instance, or nullptr if not found
         */
        inline IScript* GetScript(const std::string& scriptName) const {
            return gameObject().GetScript(scriptName);
        }

        //=====================================================================
        // INTERNAL ENGINE INTERFACE (Do not call from scripts)
        //=====================================================================

        void _SetEntity(Entity entity) { m_entity = entity; }
        void _LinkToEngine(ScriptContext* context);
        void _RefreshComponentReferences();
        bool _HasStarted() const { return m_hasStarted; }
        void _MarkStartCalled() { m_hasStarted = true; }

    private:
        // Forward declaration of field registry (PIMPL pattern)
        class FieldRegistry;

        // Helper function to reduce code duplication in field registration
        void RegisterFieldInternal(
            const std::string& name,
            const std::string& typeToken,
            void* memberPtr,
            std::function<std::string()> getValue,
            std::function<bool(const std::string&)> setValue);

        // Helper methods for template functions to access FieldRegistry
        void SetFieldEnumOptions(const std::string& name, const std::vector<std::string>& options);
        void SetFieldEnumCallbacks(const std::string& name,
            std::function<int()> getEnumValue,
            std::function<void(int)> setEnumValue);

        void SetFieldLayerMaskCallbacks(const std::string& name,
            std::function<uint32_t()> getLayerMaskValue,
            std::function<void(uint32_t)> setLayerMaskValue);

     Entity m_entity = INVALID_ENTITY;
        bool m_enabled = true;
  bool m_hasStarted = false;

     ScriptContext* m_context = nullptr;  // Opaque pointer to engine internals
        FieldRegistry* m_fieldRegistry = nullptr;  // Opaque pointer to field storage
    };

    //=========================================================================
    // SCRIPT REGISTRAR INTERFACE (For DLL entry point)
    //=========================================================================

    /**
     * @class IScriptRegistrar
     * @brief Interface for registering scripts from game DLLs
     *
     * Game DLLs export a RegisterEngineScripts() function that receives
     * this interface to register all script types.
     */
    class IScriptRegistrar {
    public:
        virtual ~IScriptRegistrar() = default;

        /**
         * Register a script type with the engine.
         * @param name The name used to identify this script type
         * @param factory Function that creates a new instance of the script
         */
        virtual void RegisterScript(const std::string& name, std::function<IScript* ()> factory) = 0;

        /**
         * Optional: Check if a script type is already registered
         * @param name The script name to check
         * @return true if the script is registered, false otherwise
         */
        virtual bool IsScriptRegistered(const std::string& name) const = 0;

        /**
         * Optional: Get the number of registered scripts
         * @return Number of registered script types
         */
        virtual size_t GetRegisteredScriptCount() const = 0;
    };

    //=========================================================================
    // SCENE API (SDK-level Scene Management functions)
    //=========================================================================

    SCRIPT_API void SwitchScene(const std::string& path);

    //=========================================================================
    // LOGGING API (SDK-level logging functions)
    //=========================================================================

    /// Log level enumeration for SDK logging
    enum class LogLevel {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Critical = 4
    };

    /**
     * @brief Log a message from a script
     * @param level The severity level of the message
     * @param message The message to log
     * @param file Optional source file (use __FILE__ macro)
     * @param line Optional source line (use __LINE__ macro)
     */
    SCRIPT_API void Log(LogLevel level, const std::string& message, const std::string& file = "", int line = -1);

    //=========================================================================
    // COROUTINE API (SDK-level coroutine functions)
    //=========================================================================

    /// Opaque handle to a coroutine
    using CoroutineHandle = unsigned int;

    /**
     * @brief Create a new coroutine
     * @return Handle to the created coroutine
     */
    SCRIPT_API CoroutineHandle CreateCoroutine();

    /**
     * @brief Add an action (function) to the coroutine sequence
     * @param handle The coroutine handle
     * @param action The function to execute
     */
    SCRIPT_API void AddCoroutineAction(CoroutineHandle handle, std::function<void()> action);

    /**
     * @brief Add a wait/delay to the coroutine sequence
     * @param handle The coroutine handle
     * @param seconds Number of seconds to wait
     */
    SCRIPT_API void AddCoroutineWait(CoroutineHandle handle, float seconds);

    /**
     * @brief Start executing a coroutine
     * @param handle The coroutine handle to start
     */
    SCRIPT_API void StartCoroutine(CoroutineHandle handle);

    //=========================================================================
    // INPUT API (SDK-level input functions)
    //=========================================================================

    /**
     * @brief Check if a key is currently held down
     * @param key The key code (ASCII for letters, or GLFW key codes for special keys)
     * @return true if the key is currently down
     */
    SCRIPT_API bool IsKeyDown(int key);

    /**
     * @brief Check if a key was pressed this frame (state transition from up to down)
     * @param key The key code
     * @return true if the key was pressed this frame
     */
    SCRIPT_API bool WasKeyPressed(int key);

    /**
     * @brief Check if a key was released this frame (state transition from down to up)
     * @param key The key code
     * @return true if the key was released this frame
     */
    SCRIPT_API bool WasKeyReleased(int key);

    /**
     * @brief Check if a mouse button is currently held down
     * @param button The mouse button code (0 = left, 1 = right, 2 = middle)
     * @return true if the button is currently down
     */
    SCRIPT_API bool IsMouseDown(int button);

    /**
     * @brief Check if a mouse button was pressed this frame
     * @param button The mouse button code
     * @return true if the button was pressed this frame
     */
    SCRIPT_API bool WasMousePressed(int button);

    /**
     * @brief Check if a mouse button was released this frame
     * @param button The mouse button code
     * @return true if the button was released this frame
     */
    SCRIPT_API bool WasMouseReleased(int button);

    /**
     * @brief Get the current mouse cursor position
     * @return Pair of (x, y) screen coordinates
     */
    SCRIPT_API std::pair<double, double> MousePos();

    /**
     * @brief Get the mouse movement delta since last frame
     * @return Pair of (deltaX, deltaY)
     */
    SCRIPT_API std::pair<double, double> MouseDelta();

    /**
     * @brief Get the scroll wheel delta
     * @return Pair of (scrollX, scrollY)
     */
    SCRIPT_API std::pair<double, double> ScrollDelta();

    /**
     * @brief Lock/unlock the mouse cursor
     * @param locked true to lock cursor to window, false to unlock
     */
    SCRIPT_API void SetMouseLocked(bool locked);

    /**
     * @brief Check if mouse cursor is currently locked
     * @return true if mouse is locked
     */
    SCRIPT_API bool IsMouseLocked();

    //=========================================================================
    // EVENT API (SDK-level event system for script communication)
    //=========================================================================

    /**
     * @brief Send a generic event to the engine and other scripts
     * @param eventName The name/identifier of the event
     * @param data Optional pointer to event data (can be nullptr)
     *
     * Example:
     * @code
     * int damage = 20;
     * SendScriptEvent("OnPlayerHit", &damage);
     * @endcode
     */
    SCRIPT_API void SendScriptEvent(const char* eventName, void* data);

    /**
     * @brief Register a callback to listen for script events
     * @param eventName The name of the event to listen for
     * @param callback The function to call when the event is triggered
     *
     * Example:
     * @code
     * RegisterScriptEventListener("OnPlayerHit", [](void* data) {
     *     int* damage = static_cast<int*>(data);
     *     SCRIPT_LOG_INFO("Player took ", *damage, " damage!");
     * });
     * @endcode
     */
    SCRIPT_API void RegisterScriptEventListener(const char* eventName, std::function<void(void*)> callback);

    /**
     * @brief Clear all registered script event listeners
     * Useful for cleanup when scripts are unloaded
     */
    SCRIPT_API void ClearScriptEventListeners();

    //=========================================================================
    // TWEEN API (SDK-level tween/animation functions)
    //=========================================================================

    /// Tween type enumeration for different interpolation curves
    enum class TweenType {
        LINEAR = 0,
        EASE_IN,
        EASE_OUT,
        EASE_BOTH,
        CUBIC_EASE_IN,
        CUBIC_EASE_OUT,
        CUBIC_EASE_BOTH
    };

    /// Opaque handle to a tween animation
    using TweenHandle = unsigned int;

    /**
     * @brief Start a tween animation using a lambda/callback function
     * @param updateFunc Callback function that receives the interpolated value (0.0 to 1.0)
     * @param duration Duration of the tween in seconds
     * @param type The interpolation curve type
     * @param entity Optional entity to associate with this tween (for CheckTween)
     * @return Handle to the created tween
     *
     * Example:
     * @code
     * Vec3 startPos = GetPosition();
     * Vec3 targetPos = Vec3(10, 0, 0);
     * StartTweenLambda([this, startPos, targetPos](float t) {
     *     SetPosition(startPos + (targetPos - startPos) * t);
     * }, 2.0f, TweenType::LINEAR, GetEntity());
     * @endcode
     */
    SCRIPT_API TweenHandle StartTweenLambda(
        std::function<void(float)> updateFunc,
        float duration,
        TweenType type = TweenType::CUBIC_EASE_IN,
        Entity entity = 0
    );

    /**
     * @brief Start a tween for Vec3 values (position, rotation, scale)
     * @param setter Function to call with interpolated Vec3 value
     * @param start Starting Vec3 value
     * @param end Ending Vec3 value
     * @param duration Duration of the tween in seconds
     * @param type The interpolation curve type
     * @param entity Optional entity to associate with this tween
     * @return Handle to the created tween
     *
     * Example:
     * @code
     * StartTweenVec3([this](const Vec3& pos) { SetPosition(pos); },
     *                GetPosition(), Vec3(10, 0, 0), 2.0f, TweenType::LINEAR, GetEntity());
     * @endcode
     */
    SCRIPT_API TweenHandle StartTweenVec3(
        std::function<void(const Vec3&)> setter,
        const Vec3& start,
        const Vec3& end,
        float duration,
        TweenType type = TweenType::CUBIC_EASE_IN,
        Entity entity = 0
    );

    /**
     * @brief Start a tween for float values
     * @param setter Function to call with interpolated float value
     * @param start Starting float value
     * @param end Ending float value
     * @param duration Duration of the tween in seconds
     * @param type The interpolation curve type
     * @param entity Optional entity to associate with this tween
     * @return Handle to the created tween
     */
    SCRIPT_API TweenHandle StartTweenFloat(
        std::function<void(float)> setter,
        float start,
        float end,
        float duration,
        TweenType type = TweenType::CUBIC_EASE_IN,
        Entity entity = 0
    );

    /**
     * @brief Check if an entity has any active tweens
     * @param entity The entity to check
     * @return true if the entity has active tweens, false otherwise
     */
    SCRIPT_API bool CheckEntityTween(Entity entity);

    /**
     * @brief Stop a specific tween by handle
     * @param handle The tween handle to stop
     */
    SCRIPT_API void StopTween(TweenHandle handle);

    /**
     * @brief Stop all tweens associated with an entity
     * @param entity The entity whose tweens should be stopped
     */
    SCRIPT_API void StopEntityTweens(Entity entity);

    /**
     * @brief Clear all active tweens
     */
    SCRIPT_API void ClearAllTweens();

    //=========================================================================
    // RENDER SETTINGS API (SDK-level render settings functions)
    //=========================================================================

    /// Environment source enumeration for ambient lighting
    enum class EnvSource : uint8_t {
        Skybox = 0,
        Gradient = 1,
        Color = 2
    };

    /// Fog mode enumeration for fog rendering
    enum class FogMode : uint8_t {
        Linear = 0,
        Exponential = 1,
        ExponentialSquared = 2
    };

    // Environment Lighting
    SCRIPT_API EnvSource GetEnvSource();
    SCRIPT_API void SetEnvSource(EnvSource source);
    SCRIPT_API Vec3 GetAmbientColor();
    SCRIPT_API void SetAmbientColor(const Vec3& color);
    SCRIPT_API void SetAmbientColor(float r, float g, float b);
    SCRIPT_API float GetAmbientIntensity();
    SCRIPT_API void SetAmbientIntensity(float intensity);

    // Fog Settings
    SCRIPT_API bool IsFogEnabled();
    SCRIPT_API void SetFogEnabled(bool enabled);
    SCRIPT_API FogMode GetFogMode();
    SCRIPT_API void SetFogMode(FogMode mode);
    SCRIPT_API Vec3 GetFogColor();
    SCRIPT_API void SetFogColor(const Vec3& color);
    SCRIPT_API void SetFogColor(float r, float g, float b);
    SCRIPT_API float GetFogStart();
    SCRIPT_API void SetFogStart(float start);
    SCRIPT_API float GetFogEnd();
    SCRIPT_API void SetFogEnd(float end);
    SCRIPT_API float GetFogDensity();
    SCRIPT_API void SetFogDensity(float density);

} // namespace Scripting
} // namespace NE

//=============================================================================
// CLEAN SDK NAMESPACE ORGANIZATION
//=============================================================================
// Standardized namespaces for script API: Category::Function()

#include <sstream>

/// Input management namespace - keyboard, mouse, and cursor control
namespace Input {
    inline bool IsKeyDown(int key) { return NE::Scripting::IsKeyDown(key); }
    inline bool WasKeyPressed(int key) { return NE::Scripting::WasKeyPressed(key); }
    inline bool WasKeyReleased(int key) { return NE::Scripting::WasKeyReleased(key); }
    inline bool IsMouseDown(int button) { return NE::Scripting::IsMouseDown(button); }
    inline bool WasMousePressed(int button) { return NE::Scripting::WasMousePressed(button); }
    inline bool WasMouseReleased(int button) { return NE::Scripting::WasMouseReleased(button); }
    inline std::pair<double, double> GetMousePosition() { return NE::Scripting::MousePos(); }
    inline std::pair<double, double> GetMouseDelta() { return NE::Scripting::MouseDelta(); }
    inline std::pair<double, double> GetScrollDelta() { return NE::Scripting::ScrollDelta(); }
    inline void SetMouseLocked(bool locked) { NE::Scripting::SetMouseLocked(locked); }
    inline bool IsMouseLocked() { return NE::Scripting::IsMouseLocked(); }
}

/// Event system namespace - send and listen for game events
namespace Events {
    inline void Send(const char* eventName, void* data = nullptr) {
        NE::Scripting::SendScriptEvent(eventName, data);
    }
    inline void Listen(const char* eventName, std::function<void(void*)> callback) {
        NE::Scripting::RegisterScriptEventListener(eventName, callback);
    }
    inline void ClearAllListeners() {
        NE::Scripting::ClearScriptEventListeners();
    }
}

/// Coroutine system namespace - delayed and sequenced actions
namespace Coroutines {
    using Handle = NE::Scripting::CoroutineHandle;

    inline Handle Create() { return NE::Scripting::CreateCoroutine(); }
    inline void AddAction(Handle handle, std::function<void()> action) {
        NE::Scripting::AddCoroutineAction(handle, action);
    }
    inline void AddWait(Handle handle, float seconds) {
        NE::Scripting::AddCoroutineWait(handle, seconds);
    }
    inline void Start(Handle handle) {
        NE::Scripting::StartCoroutine(handle);
    }
}

/// Logging namespace - debug output and diagnostics
namespace Log {
    inline void Debug(const std::string& message, const std::string& file = "", int line = -1) {
        NE::Scripting::Log(NE::Scripting::LogLevel::Debug, message, file, line);
    }
    inline void Info(const std::string& message, const std::string& file = "", int line = -1) {
        NE::Scripting::Log(NE::Scripting::LogLevel::Info, message, file, line);
    }
    inline void Warning(const std::string& message, const std::string& file = "", int line = -1) {
        NE::Scripting::Log(NE::Scripting::LogLevel::Warning, message, file, line);
    }
    inline void Error(const std::string& message, const std::string& file = "", int line = -1) {
        NE::Scripting::Log(NE::Scripting::LogLevel::Error, message, file, line);
    }
    inline void Critical(const std::string& message, const std::string& file = "", int line = -1) {
        NE::Scripting::Log(NE::Scripting::LogLevel::Critical, message, file, line);
    }
}

/// Tween system namespace - animation and interpolation
namespace Tweener {
    using Handle = NE::Scripting::TweenHandle;
    using Type = NE::Scripting::TweenType;

    /**
     * @brief Start a tween using a lambda function that receives normalized time (0.0 to 1.0)
     * @param updateFunc Callback that receives interpolated value from 0.0 to 1.0
     * @param duration Duration in seconds
     * @param type Interpolation curve type
     * @param entity Optional entity to associate with this tween
     * @return Handle to the tween
     */
    inline Handle StartLambda(
        std::function<void(float)> updateFunc,
        float duration,
        Type type = Type::CUBIC_EASE_IN,
        NE::Scripting::Entity entity = 0)
    {
        return NE::Scripting::StartTweenLambda(updateFunc, duration, type, entity);
    }

    /**
     * @brief Start a tween for Vec3 values
     * @param setter Function to call with interpolated Vec3
     * @param start Starting value
     * @param end Ending value
     * @param duration Duration in seconds
     * @param type Interpolation curve type
     * @param entity Optional entity to associate with this tween
     * @return Handle to the tween
     */
    inline Handle StartVec3(
        std::function<void(const NE::Scripting::Vec3&)> setter,
        const NE::Scripting::Vec3& start,
        const NE::Scripting::Vec3& end,
        float duration,
        Type type = Type::CUBIC_EASE_IN,
        NE::Scripting::Entity entity = 0)
    {
        return NE::Scripting::StartTweenVec3(setter, start, end, duration, type, entity);
    }

    /**
     * @brief Start a tween for float values
     * @param setter Function to call with interpolated float
     * @param start Starting value
     * @param end Ending value
     * @param duration Duration in seconds
     * @param type Interpolation curve type
     * @param entity Optional entity to associate with this tween
     * @return Handle to the tween
     */
    inline Handle StartFloat(
        std::function<void(float)> setter,
        float start,
        float end,
        float duration,
        Type type = Type::CUBIC_EASE_IN,
        NE::Scripting::Entity entity = 0)
    {
        return NE::Scripting::StartTweenFloat(setter, start, end, duration, type, entity);
    }

    /**
     * @brief Check if an entity has any active tweens
     * @param entity The entity to check
     * @return true if entity has active tweens
     */
    inline bool CheckEntity(NE::Scripting::Entity entity) {
        return NE::Scripting::CheckEntityTween(entity);
    }

    /**
     * @brief Stop a specific tween
     * @param handle The tween handle to stop
     */
    inline void Stop(Handle handle) {
        NE::Scripting::StopTween(handle);
    }

    /**
     * @brief Stop all tweens associated with an entity
     * @param entity The entity whose tweens to stop
     */
    inline void StopEntity(NE::Scripting::Entity entity) {
        NE::Scripting::StopEntityTweens(entity);
    }

    /**
     * @brief Clear all active tweens
     */
    inline void Clear() {
        NE::Scripting::ClearAllTweens();
    }
}

/// Render settings namespace - control environment lighting and fog
namespace RenderSettings {
    using EnvSource = NE::Scripting::EnvSource;
    using FogMode = NE::Scripting::FogMode;

    // Environment Lighting
    /**
     * @brief Get the current environment source for ambient lighting
     * @return Current environment source (Skybox, Gradient, or Color)
     */
    inline EnvSource GetEnvSource() {
        return NE::Scripting::GetEnvSource();
    }

    /**
     * @brief Set the environment source for ambient lighting
     * @param source Environment source to use
     */
    inline void SetEnvSource(EnvSource source) {
        NE::Scripting::SetEnvSource(source);
    }

    /**
     * @brief Get the ambient color
     * @return Ambient color as Vec3
     */
    inline NE::Scripting::Vec3 GetAmbientColor() {
        return NE::Scripting::GetAmbientColor();
    }

    /**
     * @brief Set the ambient color
     * @param color New ambient color
     */
    inline void SetAmbientColor(const NE::Scripting::Vec3& color) {
        NE::Scripting::SetAmbientColor(color);
    }

    /**
     * @brief Set the ambient color using RGB components
     * @param r Red component (0.0 to 1.0)
     * @param g Green component (0.0 to 1.0)
     * @param b Blue component (0.0 to 1.0)
     */
    inline void SetAmbientColor(float r, float g, float b) {
        NE::Scripting::SetAmbientColor(r, g, b);
    }

    /**
     * @brief Get the ambient intensity
     * @return Ambient intensity multiplier
     */
    inline float GetAmbientIntensity() {
        return NE::Scripting::GetAmbientIntensity();
    }

    /**
     * @brief Set the ambient intensity
     * @param intensity Ambient intensity multiplier
     */
    inline void SetAmbientIntensity(float intensity) {
        NE::Scripting::SetAmbientIntensity(intensity);
    }

    // Fog Settings
    /**
     * @brief Check if fog is enabled
     * @return true if fog is enabled
     */
    inline bool IsFogEnabled() {
        return NE::Scripting::IsFogEnabled();
    }

    /**
     * @brief Enable or disable fog
     * @param enabled true to enable fog, false to disable
     */
    inline void SetFogEnabled(bool enabled) {
        NE::Scripting::SetFogEnabled(enabled);
    }

    /**
     * @brief Get the current fog mode
     * @return Current fog mode (Linear, Exponential, or ExponentialSquared)
     */
    inline FogMode GetFogMode() {
        return NE::Scripting::GetFogMode();
    }

    /**
     * @brief Set the fog mode
     * @param mode Fog mode to use
     */
    inline void SetFogMode(FogMode mode) {
        NE::Scripting::SetFogMode(mode);
    }

    /**
     * @brief Get the fog color
     * @return Fog color as Vec3
     */
    inline NE::Scripting::Vec3 GetFogColor() {
        return NE::Scripting::GetFogColor();
    }

    /**
     * @brief Set the fog color
     * @param color New fog color
     */
    inline void SetFogColor(const NE::Scripting::Vec3& color) {
        NE::Scripting::SetFogColor(color);
    }

    /**
     * @brief Set the fog color using RGB components
     * @param r Red component (0.0 to 1.0)
     * @param g Green component (0.0 to 1.0)
     * @param b Blue component (0.0 to 1.0)
     */
    inline void SetFogColor(float r, float g, float b) {
        NE::Scripting::SetFogColor(r, g, b);
    }

    /**
     * @brief Get the fog start distance (for Linear fog mode)
     * @return Fog start distance
     */
    inline float GetFogStart() {
        return NE::Scripting::GetFogStart();
    }

    /**
     * @brief Set the fog start distance (for Linear fog mode)
     * @param start Fog start distance
     */
    inline void SetFogStart(float start) {
        NE::Scripting::SetFogStart(start);
    }

    /**
     * @brief Get the fog end distance (for Linear fog mode)
     * @return Fog end distance
     */
    inline float GetFogEnd() {
        return NE::Scripting::GetFogEnd();
    }

    /**
     * @brief Set the fog end distance (for Linear fog mode)
     * @param end Fog end distance
     */
    inline void SetFogEnd(float end) {
        NE::Scripting::SetFogEnd(end);
    }

    /**
     * @brief Get the fog density (for Exponential fog modes)
     * @return Fog density
     */
    inline float GetFogDensity() {
        return NE::Scripting::GetFogDensity();
    }

    /**
     * @brief Set the fog density (for Exponential fog modes)
     * @param density Fog density
     */
    inline void SetFogDensity(float density) {
        NE::Scripting::SetFogDensity(density);
    }
}

//=============================================================================
// LOGGING MACROS (Convenience macros for stream-style logging)
//=============================================================================

/// Log a debug message
#define LOG_DEBUG(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    Log::Debug(oss.str(), __FILE__, __LINE__); } while(0)

/// Log an info message
#define LOG_INFO(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    Log::Info(oss.str(), __FILE__, __LINE__); } while(0)

/// Log a warning message
#define LOG_WARNING(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    Log::Warning(oss.str(), __FILE__, __LINE__); } while(0)

/// Log an error message
#define LOG_ERROR(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    Log::Error(oss.str(), __FILE__, __LINE__); } while(0)

/// Log a critical message
#define LOG_CRITICAL(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    Log::Critical(oss.str(), __FILE__, __LINE__); } while(0)

// Legacy macro aliases for backward compatibility
#define SCRIPT_LOG_DEBUG LOG_DEBUG
#define SCRIPT_LOG_INFO LOG_INFO
#define SCRIPT_LOG_WARNING LOG_WARNING
#define SCRIPT_LOG_ERROR LOG_ERROR
#define SCRIPT_LOG_CRITICAL LOG_CRITICAL

//=============================================================================
// TEMPLATE METHOD IMPLEMENTATIONS (Enum and Struct Registration)
//=============================================================================

namespace NE::Scripting {

    /**
     * Register an enum field with automatic dropdown support in editor
     * @param name Field name
     * @param memberPtr Pointer to enum member variable
     * @param enumOptions List of enum value names (in order)
     */
    template<typename EnumType>
    inline void IScript::RegisterEnumField(const std::string& name, EnumType* memberPtr, const std::vector<std::string>& enumOptions) {
        RegisterFieldInternal(
            name,
            "enum",
            memberPtr,
            // getValue: Return enum as string index
            [memberPtr]() -> std::string {
                return std::to_string(static_cast<int>(*memberPtr));
            },
            // setValue: Set enum from string index
            [memberPtr, enumOptions](const std::string& value) -> bool {
                try {
                    int idx = std::stoi(value);
                    if (idx >= 0 && idx < static_cast<int>(enumOptions.size())) {
                        *memberPtr = static_cast<EnumType>(idx);
                        return true;
                    }
                    return false;
                } catch (...) {
                    return false;
                }
            }
        );

        // Store enum options and accessor functions using helper methods
        SetFieldEnumOptions(name, enumOptions);
        SetFieldEnumCallbacks(name,
            [memberPtr]() -> int {
                return static_cast<int>(*memberPtr);
            },
            [memberPtr](int value) {
                *memberPtr = static_cast<EnumType>(value);
            }
        );
    }

    /**
     * Register a struct field using reflection system
     * All reflected fields will be registered as "structName.fieldName"
     * @param structName Prefix for nested fields
     * @param memberPtr Pointer to struct member variable
     *
     * NOTE: This requires StructType to have NE_REFLECT macros defined.
     *       Include "Reflection.h" in your game script file before using this.
     */
    template<typename StructType>
    inline void IScript::RegisterStructField(const std::string& structName, StructType* memberPtr)
    {
        // Use reflection system to iterate over all struct fields
        NE::Core::ForEachField(*memberPtr, [&](auto const& desc, auto& fieldValue) {
            std::string fullName = structName + "." + std::string(desc.name);

            using FieldT = std::remove_reference_t<decltype(fieldValue)>;

            if constexpr (std::is_same_v<FieldT, int>) {
                RegisterIntField(fullName,
                    reinterpret_cast<int*>(reinterpret_cast<char*>(memberPtr) +
                        (reinterpret_cast<char*>(&fieldValue) - reinterpret_cast<char*>(memberPtr))));
            }
            else if constexpr (std::is_same_v<FieldT, float>) {
                RegisterFloatField(fullName,
                    reinterpret_cast<float*>(reinterpret_cast<char*>(memberPtr) +
                        (reinterpret_cast<char*>(&fieldValue) - reinterpret_cast<char*>(memberPtr))));
            }
            else if constexpr (std::is_same_v<FieldT, bool>) {
                RegisterBoolField(fullName,
                    reinterpret_cast<bool*>(reinterpret_cast<char*>(memberPtr) +
                        (reinterpret_cast<char*>(&fieldValue) - reinterpret_cast<char*>(memberPtr))));
            }
            else if constexpr (std::is_same_v<FieldT, std::string>) {
                RegisterStringField(fullName,
                    reinterpret_cast<std::string*>(reinterpret_cast<char*>(memberPtr) +
                        (reinterpret_cast<char*>(&fieldValue) - reinterpret_cast<char*>(memberPtr))));
            }
            else if constexpr (std::is_same_v<FieldT, Vec3>) {
                RegisterVec3Field(fullName,
                    reinterpret_cast<Vec3*>(reinterpret_cast<char*>(memberPtr) +
                        (reinterpret_cast<char*>(&fieldValue) - reinterpret_cast<char*>(memberPtr))));
            }
        });
    }

} // namespace Scripting


//=============================================================================
// DLL ENTRY POINT SIGNATURE
//=============================================================================

/**
 * Game DLLs must export this function to register their scripts.
 *
 * Example:
 * @code
 * extern "C" {
 *     __declspec(dllexport)
 *     void RegisterEngineScripts(NE::Scripting::IScriptRegistrar* registrar) {
 *         registrar->RegisterScript("PlayerScript", []() -> NE::Scripting::IScript* {
 *             return new PlayerScript();
 *         });
 *     }
 * }
 * @endcode
 */
extern "C" {
    typedef void (*RegisterScriptsFunction)(NE::Scripting::IScriptRegistrar* registrar);
}
