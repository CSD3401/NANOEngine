# Tween API Usage Guide

The Tween API provides smooth interpolation animations for your scripts. All tween functions are available through the `Tween::` namespace.

## Table of Contents
- [Quick Start](#quick-start)
- [Tween Types](#tween-types)
- [API Reference](#api-reference)
- [Examples](#examples)

## Quick Start

```cpp
#include "EngineAPI.hpp"

class MyScript : public IScript {
    void Update(double deltaTime) override {
        if (Input::WasKeyPressed('T')) {
            // Simple position tween
            Vec3 startPos = GetPosition();
            Vec3 targetPos = Vec3(10, 0, 0);

            Tween::StartVec3(
                [this](const Vec3& pos) { SetPosition(pos); },
                startPos,
                targetPos,
                2.0f,                      // duration in seconds
                Tween::Type::LINEAR,       // interpolation type
                GetEntity()                // associate with this entity
            );
        }
    }
};
```

## Tween Types

The `Tween::Type` enum provides different interpolation curves:

- **LINEAR**: Constant speed from start to end
- **EASE_IN**: Slow start, accelerates toward end
- **EASE_OUT**: Fast start, decelerates toward end
- **EASE_BOTH**: Slow start and end, fast middle
- **CUBIC_EASE_IN**: More pronounced slow start
- **CUBIC_EASE_OUT**: More pronounced slow end
- **CUBIC_EASE_BOTH**: Smooth S-curve (default)

## API Reference

### Starting Tweens

#### `Tween::StartLambda(updateFunc, duration, type, entity)`
Start a tween using a custom lambda that receives normalized time (0.0 to 1.0).

**Parameters:**
- `updateFunc`: `std::function<void(float)>` - Called each frame with interpolated value (0.0 to 1.0)
- `duration`: `float` - Duration in seconds
- `type`: `Tween::Type` - Interpolation curve (default: CUBIC_EASE_IN)
- `entity`: `Entity` - Optional entity to associate with this tween (default: 0)

**Returns:** `Tween::Handle` - Handle to the created tween

**Example:**
```cpp
Vec3 startPos = GetPosition();
Vec3 targetPos = Vec3(10, 5, 0);

Tween::StartLambda([this, startPos, targetPos](float t) {
    Vec3 currentPos = Vec3(
        startPos.x + (targetPos.x - startPos.x) * t,
        startPos.y + (targetPos.y - startPos.y) * t,
        startPos.z + (targetPos.z - startPos.z) * t
    );
    SetPosition(currentPos);
}, 2.0f, Tween::Type::LINEAR, GetEntity());
```

#### `Tween::StartVec3(setter, start, end, duration, type, entity)`
Start a tween for Vec3 values (position, rotation, scale).

**Parameters:**
- `setter`: `std::function<void(const Vec3&)>` - Called with interpolated Vec3 value
- `start`: `Vec3` - Starting value
- `end`: `Vec3` - Ending value
- `duration`: `float` - Duration in seconds
- `type`: `Tween::Type` - Interpolation curve (default: CUBIC_EASE_IN)
- `entity`: `Entity` - Optional entity to associate with this tween (default: 0)

**Returns:** `Tween::Handle` - Handle to the created tween

**Example:**
```cpp
// Tween position
Tween::StartVec3(
    [this](const Vec3& pos) { SetPosition(pos); },
    GetPosition(),
    Vec3(10, 0, 0),
    2.0f,
    Tween::Type::CUBIC_EASE_IN,
    GetEntity()
);

// Tween rotation
Tween::StartVec3(
    [this](const Vec3& rot) { SetRotation(rot); },
    GetRotation(),
    Vec3(0, 180, 0),
    3.0f,
    Tween::Type::EASE_OUT,
    GetEntity()
);

// Tween scale
Tween::StartVec3(
    [this](const Vec3& scale) { SetScale(scale); },
    GetScale(),
    Vec3(2, 2, 2),
    1.5f,
    Tween::Type::CUBIC_EASE_BOTH,
    GetEntity()
);
```

#### `Tween::StartFloat(setter, start, end, duration, type, entity)`
Start a tween for float values.

**Parameters:**
- `setter`: `std::function<void(float)>` - Called with interpolated float value
- `start`: `float` - Starting value
- `end`: `float` - Ending value
- `duration`: `float` - Duration in seconds
- `type`: `Tween::Type` - Interpolation curve (default: CUBIC_EASE_IN)
- `entity`: `Entity` - Optional entity to associate with this tween (default: 0)

**Returns:** `Tween::Handle` - Handle to the created tween

**Example:**
```cpp
float opacity = 1.0f;
Tween::StartFloat(
    [&opacity](float value) { opacity = value; },
    1.0f,  // start fully opaque
    0.0f,  // end fully transparent
    2.0f,
    Tween::Type::LINEAR,
    GetEntity()
);
```

### Checking Tweens

#### `Tween::CheckEntity(entity)`
Check if an entity has any active tweens.

**Parameters:**
- `entity`: `Entity` - The entity to check

**Returns:** `bool` - true if entity has active tweens

**Example:**
```cpp
if (Tween::CheckEntity(GetEntity())) {
    LOG_DEBUG("This entity has active tweens");
}
```

### Stopping Tweens

#### `Tween::Stop(handle)`
Stop a specific tween by handle.

**Parameters:**
- `handle`: `Tween::Handle` - The tween handle to stop

**Example:**
```cpp
Tween::Handle myTween = Tween::StartVec3(...);
// Later...
Tween::Stop(myTween);
```

#### `Tween::StopEntity(entity)`
Stop all tweens associated with an entity.

**Parameters:**
- `entity`: `Entity` - The entity whose tweens to stop

**Example:**
```cpp
Tween::StopEntity(GetEntity());
```

#### `Tween::Clear()`
Clear all active tweens in the system.

**Example:**
```cpp
Tween::Clear();
```

## Examples

### Example 1: Simple Movement
```cpp
void Update(double deltaTime) override {
    if (Input::WasKeyPressed('M')) {
        Vec3 currentPos = GetPosition();
        Vec3 targetPos = Vec3(currentPos.x + 10, currentPos.y, currentPos.z);

        Tween::StartVec3(
            [this](const Vec3& pos) { SetPosition(pos); },
            currentPos,
            targetPos,
            2.0f,
            Tween::Type::LINEAR,
            GetEntity()
        );
    }
}
```

### Example 2: Bounce Effect
```cpp
void Start() override {
    // Create a bounce effect using EASE_OUT
    Vec3 startPos = GetPosition();
    Vec3 upPos = Vec3(startPos.x, startPos.y + 5, startPos.z);

    Tween::StartVec3(
        [this](const Vec3& pos) { SetPosition(pos); },
        startPos,
        upPos,
        0.5f,
        Tween::Type::EASE_OUT,
        GetEntity()
    );
}
```

### Example 3: Pulse Animation
```cpp
void Start() override {
    Vec3 normalScale = GetScale();
    Vec3 largeScale = Vec3(normalScale.x * 1.5f, normalScale.y * 1.5f, normalScale.z * 1.5f);

    // Scale up
    Tween::StartVec3(
        [this](const Vec3& scale) { SetScale(scale); },
        normalScale,
        largeScale,
        0.5f,
        Tween::Type::EASE_OUT,
        GetEntity()
    );
}
```

### Example 4: Complex Animation with Lambda
```cpp
void Update(double deltaTime) override {
    if (Input::WasKeyPressed('W')) {
        Vec3 startPos = GetPosition();
        Vec3 targetPos = Vec3(startPos.x + 10, startPos.y, startPos.z);
        float startTime = 0.0f;

        Tween::StartLambda([this, startPos, targetPos](float t) {
            // Custom easing with sine wave
            float easedT = sin(t * 3.14159f * 0.5f);

            // Add vertical wave motion
            float wave = sin(t * 3.14159f * 4.0f) * 2.0f;

            Vec3 currentPos = Vec3(
                startPos.x + (targetPos.x - startPos.x) * easedT,
                startPos.y + wave,
                startPos.z
            );
            SetPosition(currentPos);
        }, 3.0f, Tween::Type::LINEAR, GetEntity());
    }
}
```

### Example 5: Stop Tween on Collision
```cpp
void OnCollisionEnter(Entity other) override {
    // Stop all tweens when we collide with something
    Tween::StopEntity(GetEntity());
}
```

### Example 6: Conditional Tween
```cpp
void Update(double deltaTime) override {
    if (Input::WasKeyPressed('T')) {
        // Only start tween if not already tweening
        if (!Tween::CheckEntity(GetEntity())) {
            Tween::StartVec3(
                [this](const Vec3& pos) { SetPosition(pos); },
                GetPosition(),
                Vec3(10, 0, 0),
                2.0f,
                Tween::Type::LINEAR,
                GetEntity()
            );
        }
    }
}
```

## Best Practices

1. **Always associate tweens with entities** by passing `GetEntity()` as the last parameter. This allows you to track and stop tweens easily.

2. **Clean up tweens in OnDestroy()**:
   ```cpp
   void OnDestroy() override {
       Tween::StopEntity(GetEntity());
   }
   ```

3. **Capture by value in lambdas** when using values that might change:
   ```cpp
   Vec3 startPos = GetPosition();  // Capture this value
   Tween::StartLambda([this, startPos](float t) {
       // startPos is captured by value, won't change
   }, ...);
   ```

4. **Check for active tweens** before starting new ones to avoid conflicts:
   ```cpp
   if (!Tween::CheckEntity(GetEntity())) {
       // Start tween
   }
   ```

5. **Use appropriate tween types** for different effects:
   - **LINEAR**: Mechanical movements
   - **EASE_IN**: Acceleration, gravity
   - **EASE_OUT**: Deceleration, landing
   - **CUBIC_EASE_BOTH**: Natural, smooth animations

## See Also

- [TweenExampleScript.hpp](TweenExampleScript.hpp) - Complete working example
- [EngineAPI.hpp](EngineAPI.hpp) - Main scripting API documentation
