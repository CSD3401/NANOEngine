# Unity-Style Prefab Spawning System

This document explains how to use the new prefab reference system in NANOEngine scripts.

## Overview

The prefab system allows scripts to hold references to prefabs and spawn them at runtime, similar to Unity's workflow.

## How to Use

### 1. Basic Setup

In your script header:

```cpp
#include "EngineAPI.hpp"

class MySpawnerScript : public IScript {
public:
    MySpawnerScript() {
        // Register the prefab field for editor exposure
        SCRIPT_PREFAB_REF(myPrefab);
    }

    void Update(double deltaTime) override {
        // Spawn the prefab when space is pressed
        if (Input::WasKeyPressed(' ')) {
            Entity spawned = InstantiatePrefab(myPrefab, GetPosition(), Vec3::Zero());
        }
    }

private:
    PrefabRef myPrefab;  // This will be exposed in the editor
};
```

### 2. API Reference

#### Types
- **PrefabRef** - Reference type for holding prefab assets

#### Methods
- **`InstantiatePrefab(PrefabRef, position, rotation)`** - Spawn a prefab from a reference
- **`InstantiatePrefab(path, position, rotation)`** - Spawn a prefab from a path/UUID

#### Macro
- **`SCRIPT_PREFAB_REF(fieldName)`** - Register a prefab field for editor exposure

### 3. Working with Spawned Entities

After instantiating, you can manipulate the spawned entity:

```cpp
Entity spawned = InstantiatePrefab(bulletPrefab, position, rotation);

if (spawned != INVALID_ENTITY) {
    // Add velocity if it has a rigidbody
    if (HasRigidbody(spawned)) {
        SetVelocity(GetForward() * 10.0f, spawned);
    }

    // Modify scale
    SetScale(2.0f, spawned);

    // Get children and modify them
    size_t childCount = GetChildCount(spawned);
    for (size_t i = 0; i < childCount; ++i) {
        Entity child = GetChild(i, spawned);
        // Do something with child...
    }
}
```

### 4. Common Use Cases

#### Enemy Spawner
```cpp
class EnemySpawner : public IScript {
private:
    PrefabRef enemyPrefab;
    float spawnInterval = 2.0f;
    float spawnTimer = 0.0f;

public:
    EnemySpawner() {
        SCRIPT_PREFAB_REF(enemyPrefab);
        SCRIPT_FIELD(spawnInterval, Float);
    }

    void Update(double deltaTime) override {
        spawnTimer += deltaTime;
        if (spawnTimer >= spawnInterval) {
            InstantiatePrefab(enemyPrefab, GetPosition(), Vec3::Zero());
            spawnTimer = 0.0f;
        }
    }
};
```

#### Bullet Shooter
```cpp
class BulletShooter : public IScript {
private:
    PrefabRef bulletPrefab;
    float bulletSpeed = 20.0f;

public:
    BulletShooter() {
        SCRIPT_PREFAB_REF(bulletPrefab);
        SCRIPT_FIELD(bulletSpeed, Float);
    }

    void Update(double deltaTime) override {
        if (Input::WasKeyPressed(' ')) {
            Vec3 spawnPos = GetPosition() + GetForward();
            Entity bullet = InstantiatePrefab(bulletPrefab, spawnPos, GetRotation());

            if (HasRigidbody(bullet)) {
                SetVelocity(GetForward() * bulletSpeed, bullet);
            }
        }
    }
};
```

#### Particle Effect Spawner
```cpp
class ParticleSpawner : public IScript {
private:
    PrefabRef particleEffectPrefab;

public:
    ParticleSpawner() {
        SCRIPT_PREFAB_REF(particleEffectPrefab);
    }

    void OnCollisionEnter(Entity other) override {
        // Spawn particle effect at collision point
        InstantiatePrefab(particleEffectPrefab, GetPosition(), Vec3::Zero());
    }
};
```

## Editor Integration

✅ **The inspector now fully supports `PrefabRef` fields!**

### Single Prefab Reference
- Shows a button with the prefab name (or "None" if not assigned)
- Drag and drop `.nfab` prefab files from the asset browser onto the button
- Click the "X" button to clear the reference
- Automatically resolves and stores the prefab UUID

### Prefab Vector Support
Use `vector<PrefabRef>` for multiple prefabs:

```cpp
class WaveSpawner : public IScript {
public:
    WaveSpawner() {
        SCRIPT_FIELD_VECTOR(enemyPrefabs, PrefabRef);  // Multiple prefabs!
        SCRIPT_FIELD(spawnDelay, Float);
    }

    void Update(double deltaTime) override {
        // Spawn random enemy from the list
        if (!enemyPrefabs.empty() && Input::WasKeyPressed(' ')) {
            int randomIndex = rand() % enemyPrefabs.size();
            InstantiatePrefab(enemyPrefabs[randomIndex], GetPosition(), Vec3::Zero());
        }
    }

private:
    std::vector<PrefabRef> enemyPrefabs;  // Can hold multiple prefab types!
    float spawnDelay = 2.0f;
};
```

In the editor:
- Each vector element has its own drag-drop button
- Add/remove elements with +/- buttons
- Drag different prefab types into each slot

## How It Works

✅ **UUID Resolution**: The system automatically finds prefabs by UUID!
- When you drag a prefab into the inspector, its UUID is stored
- On first spawn, the engine searches the `Assets` folder for the matching `.nfab` file
- The UUID-to-path mapping is cached for subsequent spawns
- Works seamlessly - just drag and spawn!

## Example Scripts

- **[PrefabSpawnerScript.hpp](PrefabSpawnerScript.hpp)** - Full-featured spawner with auto-spawn, timers, and manual triggering
- **[BulletShooterScript.hpp](BulletShooterScript.hpp)** - Bullet shooting system with fire rate control

## Technical Details

- Prefab references are serialized as UUID strings
- Instantiation uses `NE::DeserializePrefab()` which handles scene management internally
- The root entity of the instantiated prefab is returned
- Position and rotation are applied to the root transform after instantiation

## Future Enhancements

- [x] UUID-to-path resolution ✅ **DONE**
- [x] Editor prefab picker UI for PrefabRef fields ✅ **DONE**
- [x] Vector<PrefabRef> support in editor ✅ **DONE**
- [ ] Prefab variant support
- [ ] Nested prefab instantiation
- [ ] Prefab pooling system
