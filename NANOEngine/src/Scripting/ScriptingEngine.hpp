#pragma once

#include "IScriptRegistrar.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <Windows.h>
#include <mutex>
#include <FileWatch.hpp>
#include "ECS/Components/NativeScript.hpp"

// Forward declarations
class IScript;

// Export macros for when Engine is built as DLL
//#ifdef ENGINE_EXPORTS
//#define ENGINE_API __declspec(dllexport)
//#else
//#define ENGINE_API __declspec(dllimport)
//#endif

namespace NE::Scripting {

    // Function pointer type for the registration function that game DLLs must export
    typedef void (*RegisterScriptsFunction)(IScriptRegistrar* registrar);

    /**
     * Combined scripting engine that handles both script registration and DLL loading.
     * Can be used as a DLL itself to be loaded by editor applications.
     */
    class ScriptingEngine : public IScriptRegistrar {
    public:
        struct LoadedDLL {
            HMODULE handle;
            std::string filepath;
            std::string name;
            RegisterScriptsFunction registerFunction;

            LoadedDLL(HMODULE h, const std::string& path, const std::string& n, RegisterScriptsFunction func)
                : handle(h), filepath(path), name(n), registerFunction(func) {}
        };

    public:


        static ScriptingEngine& GetInstance() {
            static ScriptingEngine se;
            return se;
        }

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

        // === DLL Loading Management ===
        /**
         * Load a game DLL and register its scripts.
         * @param dllPath Path to the DLL file
         * @return true if successfully loaded and registered, false otherwise
         */
        bool LoadGameDLL(const std::string& dllPath);

        /**
         * Load all DLLs in a specified directory.
         * @param directory Directory to scan for DLL files
         * @return Number of successfully loaded DLLs
         */
        int LoadAllDLLsInDirectory(const std::string& directory);

        /**
         * Unload a specific DLL by name.
         * @param dllName Name of the DLL to unload (including .dll extension)
         * @return true if successfully unloaded, false if not found
         */
        bool UnloadDLL(const std::string& dllName);

        /**
         * Unload all loaded DLLs and clear registered scripts.
         */
        void UnloadAllDLLs();

        /**
         * Get information about all loaded DLLs.
         * @return Vector of loaded DLL information
         */
        const std::vector<LoadedDLL>& GetLoadedDLLs() const;

        /**
         * Check if a DLL is currently loaded.
         * @param dllName Name of the DLL to check (including .dll extension)
         * @return true if loaded, false otherwise
         */
        bool IsDLLLoaded(const std::string& dllName) const;

        // === Engine Lifecycle ===
        /**
         * Initialize the scripting engine.
         * Call this after loading all desired DLLs.
         */
        void Initialize();

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
        const std::string& GetLastError() const;

        // === Utility ===
        /**
         * Print a summary of loaded DLLs and registered scripts.
         */
        void PrintSummary() const;

        /**
         * Reload a specific DLL (unload then load again).
         * Useful for development and hot-reloading.
         * @param dllPath Path to the DLL file
         * @return true if successfully reloaded
         */
        bool ReloadDLL(const std::string& dllPath);

        /**
         * Clear all registered scripts without unloading DLLs.
         * Useful for reloading script registrations.
         */
        void ClearRegisteredScripts();

        /**
     * Get the factory function for a registered script.
     * @param name The name of the script
     * @return The function to create the script, or an empty function if not found
     */
        std::function<IScript* ()> GetScriptFactory(const std::string& name) const;


        // Thread-safe flag to request a compile
        std::atomic<bool> m_compileQueued = false;

        void SaveSerializedFields(NE::ECS::Component::NativeScript& nsc);

        void RestoreSerializedFields(NE::ECS::Component::NativeScript& nsc);
        void HandleFileWatchEvent(const std::string& path, const filewatch::Event eventType);
        void HotCompileAndReload();
        void HotReloadDLL(const std::string& oldDllPath, const std::string& newDllPath);
        void OnScriptComponentDestroyed(NE::ECS::Entity entity);
    private:
        ScriptingEngine();
        ~ScriptingEngine() = default;

        // Script registration storage
        std::unordered_map<std::string, std::function<IScript* ()>> m_scriptFactories;

        // DLL management storage
        std::vector<LoadedDLL> m_loadedDLLs;

        mutable std::mutex m_mutex;

        // State tracking
        bool m_initialized;
        std::string m_lastError;
        HMODULE m_currentLoadingDLLHandle = nullptr; // Temp state during LoadSingleDLL

        // === Private Helper Methods ===

        // Script validation
        void ValidateScriptName(const std::string& name) const;

        // DLL helpers
        std::string GetDLLName(const std::string& filepath) const;
        bool ValidateDLLPath(const std::string& path) const;
        LoadedDLL* FindLoadedDLL(const std::string& dllName);
        const LoadedDLL* FindLoadedDLL(const std::string& dllName) const;
        // Error handling
        void SetLastError(const std::string& error);
        std::string GetSystemError() const;

        // Internal DLL operations
        bool LoadSingleDLL(const std::string& dllPath);
        bool UnloadSingleDLL(LoadedDLL& dll);

        // Hot Reloading
        std::unique_ptr<filewatch::FileWatch<std::string>> m_sourceWatcher;
        std::string m_scriptDLLPath;           // Path to the final DLL 
        std::string m_scriptSourceDirectory;   // Path to watch for .cpp/hpp
        std::string m_scriptBuildCommand;      // The command to run

        // Counter to create unique filenames for hot-reloading
        int m_hotReloadCounter = 0;
        std::string m_currentLoadedDLLPath; // The path to the DLL *actually* loaded


    };

} // namespace NE::Scripting