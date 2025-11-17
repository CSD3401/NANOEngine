#pragma once

/**
 * @file EngineAPI.hpp
 * @brief Compatibility header providing access to common engine features for scripts
 *
 * This header includes commonly needed engine functionality that is not part of the
 * core ScriptSDK but is often needed by game scripts (logging, input, events, etc.)
 */

// Core SDK - Always include this first
#include <ScriptSDK/ScriptAPI.h>

// ============ LOGGING MACROS ============
// Forward declare SpdLogger class and enums from the engine DLL
#include <sstream>
#include <string>
#include <functional>

enum class SpdLogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

// SpdLogger is exported from NANOEngine.dll - declare it here to avoid including internal headers
class __declspec(dllimport) SpdLogger {
public:
    static SpdLogger& GetInstance();
    void Log(SpdLogLevel level, const std::string& message, const std::string& file = "", int line = -1);
};

// Logging macros that scripts can use
#define SPD_DEBUG(...)   do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Debug, oss.str(), __FILE__, __LINE__); } while(0)
#define SPD_INFO(...)  do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Info, oss.str(), __FILE__, __LINE__); } while(0)
#define SPD_WARNING(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Warning, oss.str(), __FILE__, __LINE__); } while(0)
#define SPD_ERROR(...)   do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Error, oss.str(), __FILE__, __LINE__); } while(0)
#define SPD_CRITICAL(...) do { std::ostringstream oss; oss << __VA_ARGS__; \
    SpdLogger::GetInstance().Log(SpdLogLevel::Critical, oss.str(), __FILE__, __LINE__); } while(0)

// ============ COROUTINES ============
// Coroutine types and functions exported from NANOEngine.dll
typedef unsigned int CoroutineHandle;

extern "C" {
    __declspec(dllimport) CoroutineHandle Engine_CreateCoroutine();
    __declspec(dllimport) void Engine_AddAction(CoroutineHandle handle, void (*func)());
    __declspec(dllimport) void Engine_AddWaitForSeconds(CoroutineHandle handle, float seconds);
    __declspec(dllimport) void Engine_StartCoroutine(CoroutineHandle handle);
    __declspec(dllimport) void Engine_UpdateCoroutines(float deltaTime);
}

// C++ version for lambdas (forward declare from DLL)
__declspec(dllimport) void Engine_AddActionCpp(CoroutineHandle handle, std::function<void()> func);

// Convenience overload
template<typename Func>
inline void Engine_AddAction(CoroutineHandle handle, Func&& func) {
    Engine_AddActionCpp(handle, std::function<void()>(std::forward<Func>(func)));
}

// ============ ENGINE FEATURES ============
// Input handling
#include "../../NANOEngine/src/Input/InputManager.hpp"

// Event system
#include "../../NANOEngine/src/Events/EventBus.hpp"

// Reflection system (for custom structs)
#include "../../NANOEngine/src/Core/Reflection.hpp"

// Exposed field registry (for advanced editor integration)
#include "../ExposedFieldRegistry.hpp"

// Type aliases for convenience
namespace ChronoGame {
    // Alias the SDK types into ChronoGame namespace for easier use
    using Vec3 = NE::Scripting::Vec3;
    using Entity = NE::Scripting::Entity;
    using IScript = NE::Scripting::IScript;
    using RaycastHit = NE::Scripting::RaycastHit;

    // Component reference types
    using TransformRef = NE::Scripting::TransformRef;
    using RigidbodyRef = NE::Scripting::RigidbodyRef;
    using AudioSourceRef = NE::Scripting::AudioSourceRef;
}
