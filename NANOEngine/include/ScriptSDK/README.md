# NANOEngine Script SDK

## Overview

The NANOEngine Script SDK provides a **clean, standalone API** for creating game scripts without requiring access to the entire engine source code. This addresses the separation concerns raised by your tech lead.

## What Changed?

### Before (Problem)
```
Game Scripts
    ↓ #include "../ECS/Core/ComponentManager.hpp"
    ↓ #include "../Math/Vec3.hpp"
    ↓ #include "../Physics/PhysicsManager.hpp"
    ↓ Requires ENTIRE engine source tree
    ↓ Tightly coupled to engine internals
```

### After (Solution)
```
Game Scripts
    ↓ #include <ScriptSDK/ScriptAPI.h>
    ↓ ONLY 3 headers needed:
        - ScriptAPI.h (interface)
        - ScriptTypes.h (types)
        - ScriptMacros.h (convenience)
    ↓ + NANOEngine.lib (import library)
    ↓ + NANOEngine.dll (runtime)
    ↓ 100% standalone, no engine source needed
```

## Tech Lead's Requirements ✅

| Requirement | Status | Solution |
|-------------|--------|----------|
| **Not fully separated** | ✅ Fixed | Scripts now use opaque handles instead of direct component access |
| **Requires entire source code** | ✅ Fixed | Only need 3 SDK headers + DLL/LIB |
| **Script-only export** | ✅ Fixed | `SCRIPT_API` macro for clean DLL exports |
| **Clean API layer** | ✅ Fixed | PIMPL pattern hides all implementation details |
| **Standalone scripts.dll** | ✅ Fixed | Can compile with just SDK headers |

## File Structure

```
NANOEngine/
├── include/ScriptSDK/          ← PUBLIC API (distribute these)
│   ├── ScriptAPI.h             ← Main interface
│   ├── ScriptTypes.h           ← Self-contained types (Vec3, Entity, etc.)
│   └── ScriptMacros.h          ← Field registration macros
│
├── src/Scripting/              ← INTERNAL (engine only)
│   ├── ScriptAPI.cpp           ← Implementation/adapter
│   ├── ScriptContext.hpp       ← Internal context
│   ├── ScriptContextFactory.*  ← Bridge to old API
│   ├── IScript.hpp             ← Backward compat redirect
│   └── IScript_Compat.hpp      ← Legacy support
│
└── bin/
    ├── NANOEngine.dll          ← Runtime
    └── NANOEngine.lib          ← Import library
```

## For Game Developers (Standalone SDK)

### What You Need

1. **Headers**:
   - `include/ScriptSDK/ScriptAPI.h`
   - `include/ScriptSDK/ScriptTypes.h`
   - `include/ScriptSDK/ScriptMacros.h`

2. **Binaries**:
   - `NANOEngine.lib` (link-time)
   - `NANOEngine.dll` (runtime)

3. **Nothing else!** No engine source code required.

### Creating a Script

```cpp
#include <ScriptSDK/ScriptAPI.h>

class PlayerController : public NE::Scripting::IScript {
public:
    PlayerController() {
        // Register fields for editor
        SCRIPT_FIELD(speed, Float);
        SCRIPT_FIELD(jumpForce, Float);
        SCRIPT_FIELD(spawnPoint, Vec3);
        SCRIPT_COMPONENT_REF(targetTransform, Transform);
    }

    void Initialize(NE::Scripting::Entity entity) override {
        // Setup code
    }

    void Update(double deltaTime) override {
        // Move player
        if (HasRigidbody()) {
            NE::Scripting::Vec3 vel = GetVelocity();
            vel.x = speed * deltaTime;
            SetVelocity(vel);
        }

        // Check reference
        if (targetTransform.IsValid()) {
            NE::Scripting::Vec3 targetPos = GetPosition(targetTransform);
            // Follow target...
        }
    }

    const char* GetTypeName() const override { return "PlayerController"; }

    // Collision events
    void OnCollisionEnter(NE::Scripting::Entity other) override {}
    void OnCollisionExit(NE::Scripting::Entity other) override {}
    void OnTriggerEnter(NE::Scripting::Entity other) override {}
    void OnTriggerExit(NE::Scripting::Entity other) override {}

private:
    float speed = 5.0f;
    float jumpForce = 10.0f;
    NE::Scripting::Vec3 spawnPoint{0, 0, 0};
    NE::Scripting::TransformRef targetTransform;
};
```

### Building Your Scripts DLL

**Project Settings**:
```
Configuration Type: Dynamic Library (.dll)
Include Directories: <path-to-SDK>/include
Library Directories: <path-to-SDK>/lib
Link Input: NANOEngine.lib
```

**Entry Point** (`GameEntry.cpp`):
```cpp
#include <ScriptSDK/ScriptAPI.h>
#include "PlayerController.h"
#include "EnemyAI.h"
// ... other scripts

extern "C" {
    __declspec(dllexport)
    void RegisterEngineScripts(NE::Scripting::IScriptRegistrar* registrar) {
        registrar->RegisterScript("PlayerController", []() -> NE::Scripting::IScript* {
            return new PlayerController();
        });

        registrar->RegisterScript("EnemyAI", []() -> NE::Scripting::IScript* {
            return new EnemyAI();
        });
    }
}
```

## SDK API Reference

### Types (ScriptTypes.h)

| Type | Description |
|------|-------------|
| `NE::Scripting::Entity` | Opaque entity ID (uint32_t) |
| `NE::Scripting::Vec3` | Self-contained 3D vector |
| `NE::Scripting::RaycastHit` | Physics raycast result |
| `NE::Scripting::TransformRef` | Reference to another entity's transform |
| `NE::Scripting::RigidbodyRef` | Reference to another entity's rigidbody |
| `NE::Scripting::AudioSourceRef` | Reference to another entity's audio |

### Interface (ScriptAPI.h)

#### Lifecycle Methods
```cpp
virtual void Awake() {}                    // Created (even if disabled)
virtual void Initialize(Entity entity) = 0; // Attached to entity
virtual void Start() {}                    // Before first Update
virtual void Update(double deltaTime) = 0;  // Every frame
virtual void OnDestroy() {}                // Being destroyed
virtual void OnEnable() {}                 // Enabled
virtual void OnDisable() {}                // Disabled
virtual void OnValidate() {}               // Editor change
```

#### Collision Events
```cpp
virtual void OnCollisionEnter(Entity other) = 0;
virtual void OnCollisionExit(Entity other) = 0;
virtual void OnTriggerEnter(Entity other) = 0;
virtual void OnTriggerExit(Entity other) = 0;
```

#### Transform Operations
```cpp
Vec3 GetPosition() const;
void SetPosition(const Vec3& pos);
void SetPosition(float x, float y, float z);

Vec3 GetRotation() const;
void SetRotation(const Vec3& rot);

Vec3 GetScale() const;
void SetScale(const Vec3& scale);
void SetScale(float uniformScale);

void Translate(const Vec3& translation);
void Rotate(const Vec3& rotation);

Vec3 GetForward() const;
Vec3 GetRight() const;
Vec3 GetUp() const;
```

#### Physics Operations
```cpp
bool HasRigidbody() const;

float GetMass() const;
void SetMass(float mass);

bool GetUseGravity() const;
void SetUseGravity(bool use);

Vec3 GetVelocity() const;
void SetVelocity(const Vec3& velocity);

void AddForce(const Vec3& force);
void AddImpulse(const Vec3& impulse);

void LockRotation(bool lockX, bool lockY, bool lockZ);

RaycastHit Raycast(const Vec3& origin, const Vec3& direction,
                   float maxDistance, uint32_t layerMask = 0xFFFFFFFF) const;
std::vector<RaycastHit> RaycastAll(...) const;
```

#### Audio Operations
```cpp
bool HasAudioSource() const;

void PlayAudio();
void StopAudio();
void PauseAudio();
void ResumeAudio();
bool IsAudioPlaying() const;

float GetVolume() const;
void SetVolume(float volume);

float GetPitch() const;
void SetPitch(float pitch);

void SetAudioLoop(bool loop);
```

#### Component References
```cpp
TransformRef GetTransformRef(Entity entity) const;
RigidbodyRef GetRigidbodyRef(Entity entity) const;
AudioSourceRef GetAudioSourceRef(Entity entity) const;

// Use references
Vec3 GetPosition(const TransformRef& ref) const;
void SetPosition(const TransformRef& ref, const Vec3& pos);
Vec3 GetVelocity(const RigidbodyRef& ref) const;
void AddForce(const RigidbodyRef& ref, const Vec3& force);
```

### Macros (ScriptMacros.h)

```cpp
// Register simple fields
SCRIPT_FIELD(fieldName, Float)
SCRIPT_FIELD(fieldName, Int)
SCRIPT_FIELD(fieldName, Bool)
SCRIPT_FIELD(fieldName, String)
SCRIPT_FIELD(fieldName, Vec3)

// Register component references
SCRIPT_COMPONENT_REF(fieldName, Transform)
SCRIPT_COMPONENT_REF(fieldName, Rigidbody)
SCRIPT_COMPONENT_REF(fieldName, AudioSource)
```

## Migration Guide

### Existing Scripts (Automatic)

**No changes needed!** Your existing scripts will automatically use the new SDK through the compatibility layer:

```cpp
// OLD CODE (still works)
#include "Scripting/IScript.hpp"

class MyScript : public IScript {
    // ... (no changes needed)
};
```

### New Scripts (Recommended)

Use the clean SDK headers directly:

```cpp
// NEW CODE (recommended)
#include <ScriptSDK/ScriptAPI.h>

class MyScript : public NE::Scripting::IScript {
    // Same code, but fully namespaced
};
```

### Type Changes

| Old Type | New Type | Notes |
|----------|----------|-------|
| `NE::Math::Vec3` | `NE::Scripting::Vec3` | Self-contained, no engine dependency |
| `NE::ECS::Entity` | `NE::Scripting::Entity` | Same underlying type |
| `IScript::ComponentRef<T>` | `NE::Scripting::ComponentRef<THandle>` | Opaque handles instead of raw pointers |
| `IScript::RaycastHit` | `NE::Scripting::RaycastHit` | Same structure |

## Benefits

### For Game Developers
- ✅ **No engine source required** - Just headers + DLL
- ✅ **Faster compilation** - No deep engine headers
- ✅ **Cleaner API** - No internal types exposed
- ✅ **Easier distribution** - Minimal SDK footprint
- ✅ **Hot reload still works** - Same functionality

### For Engine Developers
- ✅ **Better encapsulation** - Internal changes don't affect scripts
- ✅ **DLL boundary safety** - No STL types crossing DLL
- ✅ **Easier testing** - Mock the SDK interface
- ✅ **Professional structure** - Industry-standard plugin architecture

## Technical Details

### Opaque Handles (PIMPL Pattern)

Scripts never see real component pointers:

```cpp
// Internal (engine)
class ScriptContext {
    ECS::ComponentManager* componentManager;
    Physics::PhysicsManager* physicsManager;
};

// Public SDK
struct TransformHandle { void* _internal; };  // Opaque
struct RigidbodyHandle { void* _internal; };  // Opaque

// Script uses entity IDs instead
TransformRef ref(entityID);  // Stores ID, not pointer
Vec3 pos = GetPosition(ref); // Engine resolves internally
```

### Type Conversion Layer

The adapter automatically converts between SDK and engine types:

```cpp
// ScriptAPI.cpp (internal)
inline Math::Vec3 ToEngineVec3(const Scripting::Vec3& v) {
    return Math::Vec3(v.x, v.y, v.z);
}

inline Scripting::Vec3 ToSDKVec3(const Math::Vec3& v) {
    return Scripting::Vec3(v.x, v.y, v.z);
}

// Script calls
Vec3 pos = script->GetPosition();  // Returns SDK Vec3

// Internally
Vec3 IScript::GetPosition() const {
    auto& transform = context->componentManager->GetComponent<Transform>(entity);
    return ToSDKVec3(transform.position);  // Convert engine Vec3 → SDK Vec3
}
```

### DLL Export Safety

```cpp
// ScriptTypes.h
#ifdef NANOENGINE_EXPORTS
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __declspec(dllimport)
#endif

class SCRIPT_API IScript {
    // Exported for game DLLs to inherit from
};
```

## Distribution Package

To distribute the SDK to external developers:

```
NANOEngine_ScriptSDK/
├── include/
│   └── ScriptSDK/
│       ├── ScriptAPI.h
│       ├── ScriptTypes.h
│       └── ScriptMacros.h
├── lib/
│   ├── NANOEngine.lib (x64 Release)
│   └── NANOEngine_d.lib (x64 Debug)
├── bin/
│   ├── NANOEngine.dll (x64 Release)
│   └── NANOEngine_d.dll (x64 Debug)
├── examples/
│   ├── PlayerController.cpp
│   └── GameEntry.cpp
└── README.md (this file)
```

Total size: **~3 header files + 2 binaries** = Minimal footprint!

## Support

For issues or questions about the Script SDK:
- Check examples in `examples/`
- Refer to existing scripts in `ChronoGame/Scripts/`
- Contact the engine team

---

**Version**: 1.0.0
**Last Updated**: 2025-11-17
**Compatible With**: NANOEngine v3.0+
