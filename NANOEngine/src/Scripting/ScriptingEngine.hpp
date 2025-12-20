#pragma once

#include "NANOEngineAPI.hpp"
#include "IScriptRegistrar.hpp"
#include "IScript.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <Windows.h>
#include <mutex>
#pragma warning(push)
#pragma warning(disable: 4068) // unknown pragma 'mark' - Xcode/Clang pragma not recognized by MSVC
#include <FileWatch.hpp>
#pragma warning(pop)
#include "ECS/Components/NativeScript.hpp"

// Forward declarations
namespace NE::ECS {
    class ComponentManager;
    class EntityManager;
}

namespace NE::Scripting {

    // Function pointer type for the registration function that game DLLs must export
    typedef void (*RegisterScriptsFunction)(IScriptRegistrar* registrar);

    /**
     * @struct ScriptEngineConfig
     * @brief Configuration for ScriptingEngine initialization
     */
    struct ScriptEngineConfig {
        std::string scriptDLLName = "ChronoGame.dll";
        std::string scriptSourceDirectory = "../../../ChronoGame/Scripts/";
        std::string scriptProjectPath = "../../../ChronoGame/ChronoGame.vcxproj";
        std::string buildConfiguration = "Release";
        std::string buildPlatform = "x64";
    };

    /**
     * Scripting engine that handles script registration and single game DLL loading.
     * Manages hot-reloading and state preservation for ChronoGame.dll.
     */
    class ScriptingEngine : public IScriptRegistrar {
    public:
        static NANOENGINE_API ScriptingEngine& GetInstance();

        // === IScriptRegistrar interface implementation ===
        void RegisterScript(const std::string& name, std::function<IScript* ()> factory) override;
        bool IsScriptRegistered(const std::string& name) const override;
        size_t GetRegisteredScriptCount() const override;

        // === Script Management ===
        /**
         * Create an instance of a registered script by name.
         * @param name The name of the script type to create
         * @return Pointer to new script instance, or nullptr if not found
         */
        std::unique_ptr<IScript> CreateScript(const std::string& name) const;

        /**
         * Get a list of all registered script names.
         * @return Vector containing all registered script names
         */
        std::vector<std::string> GetRegisteredScriptNames() const;

        /**
         * Get the factory function for a registered script.
         * @param name The name of the script
         * @return The function to create the script, or nullptr if not found
         */
        std::function<IScript* ()> GetScriptFactory(const std::string& name) const;

        // === DLL Loading Management ===
        /**
         * Load the game script DLL and register its scripts.
         * @param dllPath Path to the DLL file
         * @return true if successfully loaded and registered, false otherwise
         */
        bool LoadScriptDLL(const std::string& dllPath);

        /**
         * Unload the currently loaded script DLL.
         * @return true if successfully unloaded, false if no DLL was loaded
         */
        bool UnloadScriptDLL();

        /**
         * Check if the script DLL is currently loaded.
         * @return true if loaded, false otherwise
         */
        bool IsScriptDLLLoaded() const;

        // === Engine Lifecycle ===
        /**
         * Initialize the scripting engine and set up hot-reloading.
         * @param config Configuration settings (uses defaults if not provided)
         */
        void Initialize(const ScriptEngineConfig& config = ScriptEngineConfig());

        /**
         * Shutdown the scripting engine and clean up all resources.
         */
        void Shutdown();

        /**
         * Check if the engine has been initialized.
         */
        bool IsInitialized() const;

        // === Error Handling ===
        /**
         * Get the last error message from operations.
         * @return Error message string
         */
        NANOENGINE_API const std::string& GetLastError() const;

        // === Utility ===
        /**
         * Print a summary of loaded DLL and registered scripts.
         */
        void PrintSummary() const;

        // === Hot Reload ===
        /**
         * Thread-safe flag to request a compile (set by file watcher).
         */
        std::atomic<bool> m_compileQueued = false;

        /**
         * Compile and hot-reload the script DLL (called from main thread).
         */
        void HotCompileAndReload();

        /**
         * Save script field values to component for hot-reload preservation.
         */
        void SaveSerializedFields(NE::ECS::Component::NativeScript& nsc);

        /**
         * Restore script field values from component after hot-reload.
         */
        void RestoreSerializedFields(NE::ECS::Component::NativeScript& nsc);

        /**
         * Destroy script instance and call cleanup (used before hot-reload).
         */
        void OnScriptComponentDestroyed(NE::ECS::Entity entity);

        void DestroyAllScriptInstances();

        // === Instance Management (ECS Refactor) ===
        /**
         * Set ECS system references (called by ScriptSystem on initialization).
         * @param componentManager Pointer to the component manager
         * @param entityManager Pointer to the entity manager
         */
        NANOENGINE_API void SetECSReferences(NE::ECS::ComponentManager* componentManager, NE::ECS::EntityManager* entityManager);

        /**
         * Create script instance for an entity based on component data.
         * @param entity The entity to create the script instance for
         * @param nsc The NativeScript component containing script name and serialized data
         * @return true if instance was created successfully, false otherwise
         */
        NANOENGINE_API bool CreateScriptInstance(NE::ECS::Entity entity, NE::ECS::Component::NativeScript& nsc);

        /**
         * Destroy script instance for an entity.
         * @param entity The entity whose script instance should be destroyed
         */
        NANOENGINE_API void DestroyScriptInstance(NE::ECS::Entity entity);

        /**
         * Get script instance for an entity.
         * @param entity The entity to get the script instance for
         * @return Pointer to script instance, or nullptr if not found
         */
        NANOENGINE_API IScript* GetScriptInstance(NE::ECS::Entity entity) const;

        /**
         * Check if entity has a script instance.
         * @param entity The entity to check
         * @return true if entity has an instance, false otherwise
         */
        NANOENGINE_API bool HasScriptInstance(NE::ECS::Entity entity) const;

        /**
         * Update all script instances (called by ScriptSystem).
         * @param deltaTime Time elapsed since last update
         */
        NANOENGINE_API void UpdateScriptInstances(double deltaTime);

        /**
         * Initialize (Awake + Initialize) script instance for an entity.
         * @param entity The entity to initialize the script for
         */
        NANOENGINE_API void InitializeScriptInstance(NE::ECS::Entity entity);

        /**
         * Start scripts (enable them) for play mode.
         */
        NANOENGINE_API void StartAllScriptInstances();

        /**
         * Pause scripts (disable them) for pause mode.
         */
        NANOENGINE_API void PauseAllScriptInstances();

        /**
         * Stop scripts (disable and save state) for stop mode.
         */
        NANOENGINE_API void StopAllScriptInstances();

    private:
        ScriptingEngine();
        ~ScriptingEngine() = default;

        // === Script Registration ===
        std::unordered_map<std::string, std::function<IScript* ()>> m_scriptFactories;
        mutable std::mutex m_mutex;

        // === Instance Management ===
        // Maps Entity -> Script Instance (runtime data, managed by ScriptEngine)
        std::unordered_map<NE::ECS::Entity, IScript*> m_scriptInstances;

        // ECS references (set by ScriptSystem)
        NE::ECS::ComponentManager* m_componentManager = nullptr;
        NE::ECS::EntityManager* m_entityManager = nullptr;

        // === Single DLL Management ===
        struct {
            HMODULE handle = nullptr;
            std::string filepath;
            RegisterScriptsFunction registerFunction = nullptr;
        } m_loadedDLL;

        // === State Tracking ===
        bool m_initialized;
        std::string m_lastError;

        // === Hot Reload System ===
        std::unique_ptr<filewatch::FileWatch<std::string>> m_sourceWatcher;
        std::string m_scriptDLLPath;              // Original DLL path (e.g., "ChronoGame.dll")
        std::string m_scriptSourceDirectory;      // Source directory to watch for changes
        std::string m_scriptBuildCommand;         // MSBuild command to compile scripts
        int m_hotReloadCounter = 0;               // Counter for versioned hot-reload copies
        std::string m_currentLoadedDLLPath;       // Currently loaded DLL copy path

        // === Private Helper Methods ===
        void ValidateScriptName(const std::string& name) const;
        bool ValidateDLLPath(const std::string& path) const;
        void SetLastError(const std::string& error);
        std::string GetSystemError() const;
        void ClearRegisteredScripts();
        void HandleFileWatchEvent(const std::string& path, const filewatch::Event eventType);
        void HotReloadDLL(const std::string& oldDllPath, const std::string& newDllPath);
        std::string CreateHotReloadCopyPath(int version) const;

        // Hot reload helper methods
        struct ScriptState {
            std::string scriptName;
            bool isEnabled;
            std::unordered_map<std::string, std::string> fields;
        };

        std::unordered_map<NE::ECS::Entity, ScriptState> SaveAllScriptStates();
        bool SwapDLLs(const std::string& oldDllPath, const std::string& newDllPath);
        void RestoreAllScriptStates(const std::unordered_map<NE::ECS::Entity, ScriptState>& stateToRestore);
        void EnableScripts(const std::unordered_map<NE::ECS::Entity, ScriptState>& stateToRestore);
    };

} // namespace NE::Scripting