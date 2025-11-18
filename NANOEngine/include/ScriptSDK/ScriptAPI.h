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
#include <functional>

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
        // TRANSFORM OPERATIONS (Unity-style)
        //=====================================================================

        Vec3 GetPosition() const;
        void SetPosition(const Vec3& pos);
        void SetPosition(float x, float y, float z);

        Vec3 GetRotation() const;
        void SetRotation(const Vec3& rot);
        void SetRotation(float x, float y, float z);

        Vec3 GetScale() const;
        void SetScale(const Vec3& scale);
        void SetScale(float x, float y, float z);
        void SetScale(float uniformScale);

        void Translate(const Vec3& translation);
        void Translate(float x, float y, float z);

        void Rotate(const Vec3& rotation);
        void Rotate(float x, float y, float z);

        // Transform direction vectors
        Vec3 GetForward() const;
        Vec3 GetRight() const;
        Vec3 GetUp() const;

        //=====================================================================
        // RIGIDBODY PHYSICS (Unity-style)
        //=====================================================================

        bool HasRigidbody() const;

        float GetMass() const;
        void SetMass(float mass);

        bool GetUseGravity() const;
        void SetUseGravity(bool use);

        bool IsStatic() const;
        void SetStatic(bool isStatic);

        void LockRotation(bool lockX, bool lockY, bool lockZ);

        Vec3 GetVelocity() const;
        void SetVelocity(const Vec3& velocity);
        void SetVelocity(float x, float y, float z);

        void AddForce(const Vec3& force);
        void AddForce(float x, float y, float z);

        void AddImpulse(const Vec3& impulse);
        void AddImpulse(float x, float y, float z);

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

        //=====================================================================
        // AUDIO SOURCE
        //=====================================================================

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
         * Get a reference to another entity's audio source.
         * @param entity Target entity
         * @return AudioSource reference (check IsValid() before use)
         */
        AudioSourceRef GetAudioSourceRef(Entity entity) const;

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
        void RegisterAudioSourceRefField(const std::string& name, AudioSourceRef* memberPtr);

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

        // Array/vector field support
        virtual size_t GetArraySize(const std::string& fieldName) const;
        virtual std::string GetArrayElement(const std::string& fieldName, size_t index) const;
        virtual bool SetArrayElement(const std::string& fieldName, size_t index, const std::string& value);
        virtual void AddArrayElement(const std::string& fieldName);
        virtual void RemoveArrayElement(const std::string& fieldName, size_t index);

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
