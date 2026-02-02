/**
 * @file ScriptTypes.h
 * @brief Minimal type definitions for NANOEngine scripting SDK
 *
 * This header contains only the essential types needed for game scripts.
 * It has NO dependencies on engine internals, allowing scripts to compile
 * standalone with just the SDK headers.
 */

#pragma once

// Export macros for script DLL interface
#ifdef NANOENGINE_EXPORTS
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __declspec(dllimport)
#endif

// Standard library includes (allowed in public API)
#include <string>
#include <vector>
#include <cstdint>
#include <utility>  // For std::pair in polymorphic lookups

// Forward declaration of engine Math::Vec3 for implicit conversion
namespace NE { namespace Math { struct Vec3; } }

namespace NE {
namespace Scripting {

    //=========================================================================
    // BASIC TYPES
    //=========================================================================

    /// Entity ID type - opaque handle to an entity
    using Entity = uint32_t;

    /// Invalid entity constant
    constexpr Entity INVALID_ENTITY = 0;

    /// Default entity parameter (sentinel value meaning "use current entity")
    constexpr Entity DEFAULT_ENTITY_PARAM = UINT32_MAX - 1;

    //=========================================================================
    // MATH TYPES (Self-contained, no engine dependencies)
    //=========================================================================

    /// 3D vector for positions, rotations, scales, directions
    struct SCRIPT_API Vec3 {
        float x, y, z;

        Vec3() : x(0), y(0), z(0) {}
        Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
        Vec3(float scalar) : x(scalar), y(scalar), z(scalar) {}

        // Implicit conversion from engine Math::Vec3
        Vec3(const NE::Math::Vec3& other);

        // Implicit conversion to engine Math::Vec3
        operator NE::Math::Vec3() const;

        // Assignment operator for engine Math::Vec3
        Vec3& operator=(const NE::Math::Vec3& other);

        // Basic operators
        Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
        Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
        Vec3 operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
        Vec3 operator/(float scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }

        Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
        Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
        Vec3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }

        // Utility methods
        float Length() const;
        float LengthSquared() const { return x * x + y * y + z * z; }

        Vec3 Normalized() const {
            float len = Length();
            if (len < 0.0001f) {
                return Vec3(0, 0, 0);
            }
            return Vec3(x / len, y / len, z / len);
        }

        void Normalize();

        float Dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
        Vec3 Cross(const Vec3& other) const;

        // Static constants
        static Vec3 Zero() { return Vec3(0, 0, 0); }
        static Vec3 One() { return Vec3(1, 1, 1); }
        static Vec3 Up() { return Vec3(0, 1, 0); }
        static Vec3 Down() { return Vec3(0, -1, 0); }
        static Vec3 Forward() { return Vec3(0, 0, 1); }
        static Vec3 Back() { return Vec3(0, 0, -1); }
        static Vec3 Right() { return Vec3(1, 0, 0); }
        static Vec3 Left() { return Vec3(-1, 0, 0); }
    };

    //=========================================================================
    // RAYCAST HIT INFO
    //=========================================================================

    /// Raycast hit information returned by physics queries
    struct SCRIPT_API RaycastHit {
        bool hasHit = false;        ///< Did the ray hit anything?
        Vec3 point;                 ///< World position where ray hit
        Vec3 normal;                ///< Surface normal at hit point
        float distance = 0.0f;      ///< Distance from ray origin to hit point
        Entity entity = INVALID_ENTITY; ///< Entity that was hit
    };

    //=========================================================================
    // OPAQUE HANDLES (Forward declarations only)
    //=========================================================================

    // These types are opaque to scripts - they can only be used through
    // the ScriptContext API. This prevents scripts from depending on
    // internal component layouts.

    /// Opaque handle to Transform component (internal use only)
    struct TransformHandle { void* _internal; };

    /// Opaque handle to Rigidbody component (internal use only)
    struct RigidbodyHandle { void* _internal; };

    /// Opaque handle to Renderer component (internal use only)
    struct RendererHandle { void* _internal; };

    /// Opaque handle to AudioSource component (internal use only)
    struct AudioSourceHandle { void* _internal; };

    /// Opaque handle to Material asset (internal use only)
    struct MaterialHandle { void* _internal; };

    /// Opaque handle to Prefab asset (internal use only)
    struct PrefabHandle { void* _internal; };

    //=========================================================================
    // COMPONENT REFERENCE (Type-safe opaque reference)
    //=========================================================================

    /// Type-safe reference to a component on another entity
    /// Scripts can store these and the engine handles lifetime management
    template<typename THandle>
    struct ComponentRef {
        Entity ownerEntity = INVALID_ENTITY;  // for backwards compatibility
        uint64_t componentLuid = 0;           // LUID-based component reference

        ComponentRef() = default;
        explicit ComponentRef(Entity entity, uint64_t luid = 0)
            : ownerEntity(entity), componentLuid(luid) {}

        /// Check if reference is valid
        bool IsValid() const {
            return componentLuid != 0 || ownerEntity != INVALID_ENTITY;
        }
        operator bool() const { return IsValid(); }

        /// Get the entity this component belongs to
        Entity GetEntity() const { return ownerEntity; }

        /// Get the component LUID
        uint64_t GetLuid() const { return componentLuid; }

        // Internal use only
        void _SetEntity(Entity entity) { ownerEntity = entity; }
        void _SetLuid(uint64_t luid) { componentLuid = luid; }
    };

    // Specific component reference types
    using TransformRef = ComponentRef<TransformHandle>;
    using RigidbodyRef = ComponentRef<RigidbodyHandle>;
    using RendererRef = ComponentRef<RendererHandle>;
    using AudioSourceRef = ComponentRef<AudioSourceHandle>;
    using MaterialRef = ComponentRef<MaterialHandle>;
    using PrefabRef = ComponentRef<PrefabHandle>;

    //=========================================================================
    // GAME OBJECT REFERENCE (For referencing other entities)
    //=========================================================================

    /**
     * @struct GameObjectRef
     * @brief Reference to another entity (GameObject)
     *
     * Use this field type to reference other entities in your scripts.
     * In the editor, you can drag entities from the hierarchy to set this reference.
     *
     * Example:
     * @code
     * class PlayerScript : public IScript {
     *     GameObjectRef target;  // Drag enemy from hierarchy in editor
     *
     *     void Initialize(Entity entity) override {
     *         RegisterGameObjectField("Target", &target);
     *     }
     *
     *     void Update(double deltaTime) override {
     *         if (target) {
     *             Vec3 targetPos = GetPosition(target.GetEntity());
     *             // Do something with target position
     *         }
     *     }
     * };
     * @endcode
     */
    struct GameObjectRef {
        Entity entity = INVALID_ENTITY;  // Referenced entity ID

        GameObjectRef() = default;
        explicit GameObjectRef(Entity e) : entity(e) {}

        // Check if reference is valid
        bool IsValid() const {
            return entity != INVALID_ENTITY;
        }

        operator bool() const { return IsValid(); }

        // Get the entity ID
        Entity GetEntity() const { return entity; }

        // Set the entity
        void SetEntity(Entity e) { entity = e; }

        // Clear the reference
        void Clear() { entity = INVALID_ENTITY; }

        // Comparison operators
        bool operator==(const GameObjectRef& other) const { return entity == other.entity; }
        bool operator!=(const GameObjectRef& other) const { return entity != other.entity; }
    };

    //=========================================================================
    // LAYER REFERENCE (For referencing a single layer from LayerRegistry)
    //=========================================================================

    /**
     * @struct LayerRef
     * @brief Reference to a single layer from the LayerRegistry
     *
     * Use this field type to reference a layer in your scripts.
     * In the editor, this will show a dropdown of all available layers.
     *
     * Example:
     * @code
     * class EnemyScript : public IScript {
     *     LayerRef targetLayer;  // Select layer from dropdown in editor
     *
     *     void Initialize(Entity entity) override {
     *         RegisterLayerRefField("Target Layer", &targetLayer);
     *     }
     *
     *     void Update(double deltaTime) override {
     *         // Use targetLayer.GetID() for raycasting layer mask, etc.
     *         uint32_t mask = 1u << targetLayer.GetID();
     *         RaycastHit hit = Raycast(GetPosition(), GetForward(), 100.0f, mask);
     *     }
     * };
     * @endcode
     */
    struct LayerRef {
        uint8_t layerID = 0;  // Layer ID (0-31, corresponds to LayerRegistry slots)

        LayerRef() = default;
        explicit LayerRef(uint8_t id) : layerID(id) {}

        // Get the layer ID
        uint8_t GetID() const { return layerID; }

        // Set the layer ID
        void SetID(uint8_t id) { layerID = id; }

        // Convert to layer mask bit
        uint32_t ToMask() const { return 1u << static_cast<uint32_t>(layerID); }

        // Check if this references a valid layer (ID within range)
        bool IsValid() const { return layerID < 32; }

        // Comparison operators
        bool operator==(const LayerRef& other) const { return layerID == other.layerID; }
        bool operator!=(const LayerRef& other) const { return layerID != other.layerID; }
    };

    //=========================================================================
    // GAME OBJECT WRAPPER (Script-to-Script Communication)
    //=========================================================================

    // Forward declarations
    class ScriptContext;
    class IScript;

    // Internal helper functions (implemented in GameObject.cpp where ScriptContext is defined)
    // These allow templates to work without needing full ScriptContext definition in the SDK header
    SCRIPT_API IScript* GameObject_GetScriptByType(Entity entity, const std::string& typeName);
    SCRIPT_API std::vector<IScript*> GameObject_GetAllScripts(Entity entity);  // For polymorphic GetComponent
    SCRIPT_API std::vector<Entity> GameObject_FindAllWithScript(const std::string& typeName);
    SCRIPT_API std::vector<std::pair<Entity, IScript*>> GameObject_GetAllEntitiesWithScripts();  // For polymorphic FindObjectsOfType
    SCRIPT_API void GameObject_SetStaticContext(class ScriptContext* context);  // Called when scripts are linked
    SCRIPT_API void GameObject_ResetStaticContext();  // Called by ScriptingEngine during shutdown

    /**
     * @class GameObject
     * @brief Wrapper for accessing scripts on other entities
     *
     * Provides Unity-like API for accessing script instances on entities.
     * Focuses on script-to-script communication - accessing member functions
     * and variables from other IScript classes.
     *
     * Example usage:
     * @code
     * // In your script
     * GameObject player = GameObject::Find("Player");
     * auto healthScript = player.GetComponent<HealthScript>();
     * if (healthScript) {
     *     healthScript->lives = 3;
     *     healthScript->TakeDamage(1);
     * }
     * @endcode
     *
     * Note: For transform, active state, layer access, etc. use the IScript methods
     * like GetPosition(), SetActive(), GetLayer() directly on your script instance.
     */
    class GameObject {
    public:
        //=====================================================================
        // CONSTRUCTION
        //=====================================================================

        /** Default constructor - creates an invalid GameObject */
        GameObject() = default;

        /** Construct from an entity ID (exported - needs ScriptContext access) */
        SCRIPT_API GameObject(Entity entity, ScriptContext* context = nullptr);

        /** Construct from a GameObjectRef (convenience for using ref fields) */
        inline GameObject(const GameObjectRef& ref, ScriptContext* context = nullptr)
            : GameObject(ref.GetEntity(), context) {}

        /** Copy constructor */
        GameObject(const GameObject& other) = default;

        /** Copy assignment */
        GameObject& operator=(const GameObject& other) = default;

        //=====================================================================
        // ENTITY IDENTITY
        //=====================================================================

        /** Get the underlying entity ID */
        Entity GetEntityId() const { return m_entity; }

        /** Check if this GameObject is valid (has a valid entity) */
        bool IsValid() const { return m_entity != INVALID_ENTITY; }

        /** Explicit conversion to bool for validity checking */
        explicit operator bool() const { return IsValid(); }

        /** Comparison operators */
        bool operator==(const GameObject& other) const { return m_entity == other.m_entity; }
        bool operator!=(const GameObject& other) const { return m_entity != other.m_entity; }

        //=====================================================================
        // NAMING
        //=====================================================================

        /** Get the name of this GameObject (exported - needs ComponentManager access) */
        SCRIPT_API std::string GetName() const;

        /** Set the name of this GameObject (exported - needs ComponentManager access) */
        SCRIPT_API void SetName(const std::string& name);

        //=====================================================================
        // SCRIPT ACCESS (GetComponent - The key feature)
        //=====================================================================

        /**
         * Get a script component by type on this GameObject.
         * This is similar to Unity's GetComponent<T>().
         *
         * @tparam T The script type (must inherit from IScript)
         * @return Pointer to the script instance, or nullptr if not found
         *
         * Example:
         * @code
         * auto health = gameObject.GetComponent<HealthScript>();
         * if (health) {
         *     health->lives--;
         *     health->TakeDamage(10);
         * }
         * @endcode
         */
        template<typename T>
        T* GetComponent() const;

        /**
         * Get a script by name on this GameObject.
         * Useful when you don't have access to the type at compile time.
         *
         * @param scriptName The registered name of the script
         * @return Pointer to the script instance, or nullptr if not found
         */
        SCRIPT_API IScript* GetScript(const std::string& scriptName) const;

        /**
         * Check if this GameObject has a specific script type.
         * @tparam T The script type to check for
         * @return true if the script exists on this GameObject
         */
        template<typename T>
        bool HasComponent() const;

        /**
         * Check if this GameObject has a specific script type (alias for HasComponent<T>).
         * @tparam T The script type to check for
         * @return true if the script exists on this GameObject
         */
        template<typename T>
        inline bool HasScript() const {
            return HasComponent<T>();
        }

        /**
         * Check if this GameObject has a script by name.
         * @param scriptName The registered name of the script
         * @return true if the script exists on this GameObject
         */
        SCRIPT_API bool HasScript(const std::string& scriptName) const;

        //=====================================================================
        // STATIC UTILITIES (Find GameObjects)
        //=====================================================================

        /**
         * Find a GameObject by name.
         * Similar to Unity's GameObject.Find().
         *
         * @param name The name to search for
         * @return GameObject with the given name, or invalid GameObject if not found
         */
        SCRIPT_API static GameObject Find(const std::string& name);

        /**
         * Find all GameObjects with a specific script type.
         * Similar to Unity's FindObjectsOfType<T>().
         *
         * @tparam T The script type to search for
         * @return Vector of all GameObjects with the specified script
         *
         * Note: This is a slow O(n) operation that iterates all entities.
         */
        template<typename T>
        static std::vector<GameObject> FindObjectsOfType();

    private:
        Entity m_entity = INVALID_ENTITY;
        mutable ScriptContext* m_context = nullptr;  // Internal context for engine access (mutable for lazy initialization in const methods)

        // Helper to get context from current entity if not set
        SCRIPT_API void EnsureContext() const;
    };

    //=========================================================================
    // LAYER MASK (Physics layer filtering)
    //=========================================================================
    /// Stores a bitmask of enabled layers for collision/raycast filtering
    struct SCRIPT_API LayerMask {
        uint32_t mask = 0;  ///< Bitmask of enabled layers

        LayerMask() = default;
        LayerMask(uint32_t value) : mask(value) {}

        /// Check if a specific layer is enabled in this mask
        bool Contains(uint8_t layer) const {
            return (mask & (1u << static_cast<uint32_t>(layer))) != 0;
        }

        /// Enable a specific layer in this mask
        void Add(uint8_t layer) {
            mask |= (1u << static_cast<uint32_t>(layer));
        }

        /// Disable a specific layer in this mask
        void Remove(uint8_t layer) {
            mask &= ~(1u << static_cast<uint32_t>(layer));
        }

        /// Toggle a specific layer in this mask
        void Toggle(uint8_t layer) {
            mask ^= (1u << static_cast<uint32_t>(layer));
        }

        /// Set the layer mask from a list of layers
        void Set(std::initializer_list<uint8_t> layers) {
            mask = 0;
            for (uint8_t layer : layers) {
                mask |= (1u << static_cast<uint32_t>(layer));
            }
        }

        /// Clear all layers
        void Clear() {
            mask = 0;
        }

        /// Check if mask is empty (no layers enabled)
        bool IsEmpty() const {
            return mask == 0;
        }

        /// Get the raw mask value
        uint32_t GetMask() const {
            return mask;
        }

        /// Set the raw mask value
        void SetMask(uint32_t value) {
            mask = value;
        }

        /// Implicit conversion to uint32_t for convenience
        operator uint32_t() const {
            return mask;
        }

        // Comparison operators
        bool operator==(const LayerMask& other) const { return mask == other.mask; }
        bool operator!=(const LayerMask& other) const { return mask != other.mask; }

        // Bitwise operators
        LayerMask operator|(const LayerMask& other) const { return LayerMask(mask | other.mask); }
        LayerMask operator&(const LayerMask& other) const { return LayerMask(mask & other.mask); }
        LayerMask operator^(const LayerMask& other) const { return LayerMask(mask ^ other.mask); }
        LayerMask operator~() const { return LayerMask(~mask); }

        LayerMask& operator|=(const LayerMask& other) { mask |= other.mask; return *this; }
        LayerMask& operator&=(const LayerMask& other) { mask &= other.mask; return *this; }
        LayerMask& operator^=(const LayerMask& other) { mask ^= other.mask; return *this; }
    };

} // namespace Scripting
} // namespace NE

//=============================================================================
// GAME OBJECT TEMPLATE IMPLEMENTATIONS
//=============================================================================

// Internal forward declarations (required for template implementations)
// These are NOT part of the public API and are only used by the internal implementation
namespace NE { namespace ECS { namespace Component { struct NativeScript; } } }

namespace NE::Scripting {

    // Forward declaration
    class IScript;

    // Helper to get the script type name, with fallback options
    template<typename T>
    inline std::string GetScriptTypeName() {
        // Try to create a temporary instance to get the type name
        // This is safe because we're just calling a virtual method that returns a const char*
        T temp;
        return temp.GetTypeName();
    }

    /**
     * Get a script component by type on this GameObject.
     * Uses dynamic_cast to support polymorphic lookups - if you have a Door script
     * that inherits from Interactable, GetComponent<Interactable>() will find it.
     * @tparam T The script type (must inherit from IScript)
     * @return Pointer to the script instance, or nullptr if not found
     */
    template<typename T>
    inline T* GameObject::GetComponent() const {
        if (!IsValid()) {
            return nullptr;
        }

        // Ensure context is valid (needed when GameObject was created from GameObjectRef)
        const_cast<GameObject*>(this)->EnsureContext();

        // Get all scripts on this entity and use dynamic_cast to find matching type
        // This supports inheritance - GetComponent<BaseClass>() finds derived classes
        std::vector<IScript*> scripts = GameObject_GetAllScripts(m_entity);
        for (IScript* script : scripts) {
            if (T* result = dynamic_cast<T*>(script)) {
                return result;
            }
        }

        return nullptr;
    }

    /**
     * Check if this GameObject has a specific script type.
     * Uses polymorphic lookup (checks inheritance).
     * @tparam T The script type to check for
     * @return true if the script exists on this GameObject
     */
    template<typename T>
    inline bool GameObject::HasComponent() const {
        return GetComponent<T>() != nullptr;
    }

    /**
     * Find all GameObjects with a specific script type.
     * Uses dynamic_cast to support polymorphic lookups - if you search for Interactable,
     * it will find all entities with scripts that inherit from Interactable.
     * @tparam T The script type to search for
     * @return Vector of all GameObjects with the specified script
     *
     * Note: This is a slow O(n) operation that iterates all entities.
     * Consider caching results if calling frequently.
     */
    template<typename T>
    inline std::vector<GameObject> GameObject::FindObjectsOfType() {
        std::vector<GameObject> result;

        // Get all entities with scripts and use dynamic_cast to find matching types
        std::vector<std::pair<Entity, IScript*>> allScripts = GameObject_GetAllEntitiesWithScripts();

        Entity lastAddedEntity = INVALID_ENTITY;
        for (const auto& [entity, script] : allScripts) {
            // Skip if we already added this entity (an entity might have multiple scripts)
            if (entity == lastAddedEntity) {
                continue;
            }

            // Use dynamic_cast to check if this script is or inherits from T
            if (dynamic_cast<T*>(script)) {
                result.push_back(GameObject(entity));
                lastAddedEntity = entity;
            }
        }

        return result;
    }

} // namespace Scripting
