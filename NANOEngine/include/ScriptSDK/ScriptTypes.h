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
    // LAYER MASK (Physics layer filtering)
    //=========================================================================

    /// Layer mask for physics filtering - similar to Unity's LayerMask
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
