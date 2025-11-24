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
        Entity ownerEntity = INVALID_ENTITY;

        ComponentRef() = default;
        explicit ComponentRef(Entity entity) : ownerEntity(entity) {}

        /// Check if reference is valid
        bool IsValid() const { return ownerEntity != INVALID_ENTITY; }
        operator bool() const { return IsValid(); }

        /// Get the entity this component belongs to
        Entity GetEntity() const { return ownerEntity; }

        // Internal use only
        void _SetEntity(Entity entity) { ownerEntity = entity; }
    };

    // Specific component reference types
    using TransformRef = ComponentRef<TransformHandle>;
    using RigidbodyRef = ComponentRef<RigidbodyHandle>;
    using AudioSourceRef = ComponentRef<AudioSourceHandle>;
    using MaterialRef = ComponentRef<MaterialHandle>;
    using PrefabRef = ComponentRef<PrefabHandle>;

} // namespace Scripting
} // namespace NE
