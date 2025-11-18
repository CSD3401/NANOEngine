#include "ScriptingEngine.hpp"
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <Windows.h>
#include "Core/SpdLogger.hpp"
#include "ScriptContextFactory.hpp"
#include "Engine.hpp"
#include "SceneManagement/Scene.hpp"
#include "Events/EventBus.hpp"
#include "Core/Couroutine.hpp"

namespace {
    struct ScriptState {
        std::string scriptName;
        bool isEnabled;
        std::unordered_map<std::string, std::string> fields;
    };

    std::string GetMSBuildPath() {
        std::string vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
        std::string cmd = "\"" + vswhere + "\" -latest -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe";

        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) return {};

        char buffer[512];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe))
            result += buffer;
        _pclose(pipe);

        // Trim newline
        if (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

        return result;
    }
}

namespace NE {
    SceneManagement::Scene& GetScene();
}

namespace NE::Scripting {

    ScriptingEngine::ScriptingEngine()
        : m_initialized(false) {
    }

    std::function<IScript* ()> ScriptingEngine::GetScriptFactory(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_scriptFactories.find(name);
       
        if (it != m_scriptFactories.end()) {
            return it->second;
        }
        // Return an empty function if the script is not found
        return nullptr;
    }

    // === IScriptRegistrar Interface Implementation ===

    void ScriptingEngine::RegisterScript(const std::string& name, std::function<IScript* ()> factory) {
        std::lock_guard<std::mutex> lock(m_mutex);
        try {
            ValidateScriptName(name);

            if (m_scriptFactories.find(name) != m_scriptFactories.end()) {
                SPD_WARNING("Script '" << name << "' is already registered.");
            }

            m_scriptFactories[name] = factory;
            SPD_INFO("Registered script: " << name);

        }
        catch (const std::exception& e) {
            SetLastError("Failed to register script '" + name + "': " + e.what());
            throw;
        }
    }

    bool ScriptingEngine::IsScriptRegistered(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_scriptFactories.find(name) != m_scriptFactories.end();
    }

    size_t ScriptingEngine::GetRegisteredScriptCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_scriptFactories.size();
    }

    // === Script Management ===

    std::unique_ptr<IScript> ScriptingEngine::CreateScript(const std::string& name) const {
        std::function<IScript* ()> factory;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_scriptFactories.find(name);
            if (it == m_scriptFactories.end()) {
                const_cast<ScriptingEngine*>(this)->SetLastError("Script '" + name + "' is not registered");
                return nullptr;
            }
            factory = it->second; // copy
        } // unlock before calling into DLL

        try {
            IScript* rawScript = factory();
            return std::unique_ptr<IScript>(rawScript);
        }
        catch (const std::exception& e) {
            const_cast<ScriptingEngine*>(this)->SetLastError("Error creating script '" + name + "': " + e.what());
            return nullptr;
        }
    }

    std::vector<std::string> ScriptingEngine::GetRegisteredScriptNames() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> names;
        names.reserve(m_scriptFactories.size());

        for (const auto& pair : m_scriptFactories) {
            names.push_back(pair.first);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    // === DLL Loading Management ===

    bool ScriptingEngine::LoadScriptDLL(const std::string& dllPath) {
        try {
            if (!ValidateDLLPath(dllPath)) {
                SetLastError("Invalid DLL path: " + dllPath);
                return false;
            }

            if (IsScriptDLLLoaded()) {
                SetLastError("Script DLL already loaded: " + m_loadedDLL.filepath);
                return false;
            }

            // Load the DLL
            HMODULE dllHandle = LoadLibraryA(dllPath.c_str());
            if (!dllHandle) {
                SetLastError("Failed to load DLL: " + dllPath + " - " + GetSystemError());
                return false;
            }

            // Get the registration function
            RegisterScriptsFunction registerFunc =
                (RegisterScriptsFunction)GetProcAddress(dllHandle, "RegisterEngineScripts");
            if (!registerFunc) {
                SetLastError("Failed to find 'RegisterEngineScripts' function in: " + dllPath + " - " + GetSystemError());
                FreeLibrary(dllHandle);
                return false;
            }

            // Store the loaded DLL information
            m_loadedDLL.handle = dllHandle;
            m_loadedDLL.filepath = dllPath;
            m_loadedDLL.registerFunction = registerFunc;

            // Call the registration function
            try {
                registerFunc(this);
                SPD_INFO("Successfully loaded and registered scripts from: " << dllPath);
                return true;
            }
            catch (const std::exception& e) {
                SetLastError("Exception during script registration: " + std::string(e.what()));
                FreeLibrary(dllHandle);
                m_loadedDLL.handle = nullptr;
                m_loadedDLL.filepath.clear();
                m_loadedDLL.registerFunction = nullptr;
                return false;
            }
        }
        catch (const std::exception& e) {
            SetLastError("Exception loading DLL '" + dllPath + "': " + e.what());
            return false;
        }
    }

    bool ScriptingEngine::UnloadScriptDLL() {
        if (!IsScriptDLLLoaded()) {
            SetLastError("No script DLL is currently loaded");
            return false;
        }

        ClearRegisteredScripts();

        if (FreeLibrary(m_loadedDLL.handle)) {
            SPD_INFO("Unloaded script DLL: " << m_loadedDLL.filepath);
            m_loadedDLL.handle = nullptr;
            m_loadedDLL.filepath.clear();
            m_loadedDLL.registerFunction = nullptr;
            return true;
        }
        else {
            SetLastError("Failed to unload DLL: " + m_loadedDLL.filepath + " - " + GetSystemError());
            return false;
        }
    }

    bool ScriptingEngine::IsScriptDLLLoaded() const {
        return m_loadedDLL.handle != nullptr;
    }

    // === Engine Lifecycle ===

    void ScriptingEngine::Initialize() {
        if (m_initialized) {
            SPD_INFO("ScriptingEngine already initialized.");
            return;
        }

        SPD_INFO("Initializing ScriptingEngine...");

        // Set up paths
        m_scriptDLLPath = "ChronoGame.dll";
        m_scriptDLLPath = std::filesystem::absolute(m_scriptDLLPath).string();
        SPD_INFO("Script DLL path: " << m_scriptDLLPath);

        m_scriptSourceDirectory = "../../../ChronoGame/Scripts/";
        m_scriptSourceDirectory = std::filesystem::absolute(m_scriptSourceDirectory).string();
        SPD_INFO("Script source directory: " << m_scriptSourceDirectory);

        // Configure build command
        std::string msbuildPath = GetMSBuildPath();
        if (msbuildPath.empty()) {
            msbuildPath = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe";
            SPD_WARNING("MSBuild not found via vswhere, using fallback path");
        }

        m_scriptBuildCommand =
            "\"\"" + msbuildPath + "\" "
            "\"../../../ChronoGame/ChronoGame.vcxproj\" "
            "/p:Configuration=Release "
            "/p:Platform=x64 "
            "/p:BuildProjectReferences=false "
            "/p:LanguageStandard=stdcpp20\"";

        SPD_INFO("Build command: " << m_scriptBuildCommand);

        // Load initial DLL copy for hot-reload support
        m_currentLoadedDLLPath = CreateHotReloadCopyPath(m_hotReloadCounter);
        m_hotReloadCounter++;

        try {
            std::filesystem::path originalDLL(m_scriptDLLPath);
            std::filesystem::path targetDLL(m_currentLoadedDLLPath);

            std::filesystem::copy_file(originalDLL, targetDLL, std::filesystem::copy_options::overwrite_existing);

            SPD_INFO("Created hot-reload copy: " << m_currentLoadedDLLPath);

            if (!LoadScriptDLL(m_currentLoadedDLLPath)) {
                SPD_ERROR("Failed to load script DLL copy: " << m_currentLoadedDLLPath);
                SPD_ERROR("Error: " << GetLastError());
            } else {
                SPD_INFO("Successfully loaded script DLL.");
                PrintSummary();
            }

        } catch (const std::filesystem::filesystem_error& e) {
            SPD_ERROR("Failed to copy initial DLL for loading: " << e.what());
            SPD_WARNING("Continuing without game scripts. Hot-compile may still function.");
        }

        // Set up file watcher for hot-reloading
        try {
            auto fileWatchCallback = [this](const std::string& path, const filewatch::Event eventType) {
                this->HandleFileWatchEvent(path, eventType);
            };

            m_sourceWatcher = std::make_unique<filewatch::FileWatch<std::string>>(
                m_scriptSourceDirectory,
                std::regex(".*\\.(cpp|hpp|h)$"),
                fileWatchCallback
            );
            SPD_INFO("File watcher started for: " << m_scriptSourceDirectory);
        } catch (const std::exception& e) {
            SPD_ERROR("Failed to create file watcher: " << e.what());
            SPD_WARNING("Hot-reload will be disabled.");
            m_sourceWatcher.reset();
        }

        m_initialized = true;
        SPD_INFO("ScriptingEngine initialization complete.");
        SPD_INFO("  - Script DLL: " << (IsScriptDLLLoaded() ? "Loaded" : "Not loaded"));
        SPD_INFO("  - Registered scripts: " << m_scriptFactories.size());
    }

    void ScriptingEngine::Shutdown() {
        if (!m_initialized) {
            return;
        }

        SPD_INFO("Shutting down ScriptingEngine...");

        // Stop file watching
        m_sourceWatcher.reset();

        // Clear script registrations
        ClearRegisteredScripts();

        // Unload the script DLL
        if (IsScriptDLLLoaded()) {
            UnloadScriptDLL();
        }

        m_initialized = false;
        SPD_INFO("ScriptingEngine shutdown complete.");
    }

    bool ScriptingEngine::IsInitialized() const {
        return m_initialized;
    }

    // === Error Handling ===

    const std::string& ScriptingEngine::GetLastError() const {
        return m_lastError;
    }

    // === Utility ===

    void ScriptingEngine::PrintSummary() const {
        SPD_INFO("=== Scripting Engine Summary ===");

        if (IsScriptDLLLoaded()) {
            SPD_INFO("Loaded Script DLL:");
            SPD_INFO("  - " << m_loadedDLL.filepath);
        } else {
            SPD_INFO("Loaded Script DLL: (none)");
        }

        auto scriptNames = GetRegisteredScriptNames();
        SPD_INFO("Registered Scripts (" << scriptNames.size() << "):");
        if (scriptNames.empty()) {
            SPD_INFO("  (none)");
        } else {
            for (const auto& name : scriptNames) {
                SPD_INFO("  - " << name);
            }
        }
        SPD_INFO("================================\n");
    }

    void ScriptingEngine::ClearRegisteredScripts() {
        std::lock_guard<std::mutex> lock(m_mutex);
        SPD_INFO("Clearing all registered scripts...");
        m_scriptFactories.clear();
        SPD_INFO("All scripts cleared.");
    }

    // === Private Helper Methods ===

    void ScriptingEngine::ValidateScriptName(const std::string& name) const {
        if (name.empty()) {
            throw std::invalid_argument("Script name cannot be empty");
        }

        if (name.find_first_of(" \t\n\r") != std::string::npos) {
            throw std::invalid_argument("Script name cannot contain whitespace characters");
        }
    }

    bool ScriptingEngine::ValidateDLLPath(const std::string& path) const {
        if (path.empty()) {
            return false;
        }

        try {
            std::filesystem::path dllPath(path);

            if (!std::filesystem::exists(dllPath)) {
                return false;
            }

            if (!std::filesystem::is_regular_file(dllPath)) {
                return false;
            }

            if (dllPath.extension() != ".dll") {
                return false;
            }

            return true;
        }
        catch (const std::filesystem::filesystem_error&) {
            return false;
        }
    }

    std::string ScriptingEngine::CreateHotReloadCopyPath(int version) const {
        std::filesystem::path path(m_scriptDLLPath);
        std::string stem = path.stem().string();
        std::string ext = path.extension().string();
        std::string newFilename = stem + "_hot_" + std::to_string(version) + ext;
        return path.replace_filename(newFilename).string();
    }

    void ScriptingEngine::SetLastError(const std::string& error) {
        m_lastError = error;
        SPD_ERROR("ScriptingEngine Error: " << error);
    }

    std::string ScriptingEngine::GetSystemError() const {
        DWORD error = ::GetLastError();
        if (error == 0) {
            return "No error";
        }

        LPSTR messageBuffer = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);

        while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
            message.pop_back();
        }

        return message;
    }

    void ScriptingEngine::HandleFileWatchEvent(const std::string& path, const filewatch::Event eventType) {
        if (eventType == filewatch::Event::modified ||
            eventType == filewatch::Event::renamed_new ||
            eventType == filewatch::Event::added) {
            SPD_INFO("File change detected: " << path);
            m_compileQueued.store(true);
        }
    }

    void ScriptingEngine::HotCompileAndReload() {
        SPD_INFO("=== HOT COMPILE & RELOAD BEGIN ===");
        SPD_INFO("Executing: " << m_scriptBuildCommand);

        int result = std::system(m_scriptBuildCommand.c_str());

        if (result != 0) {
            SPD_ERROR("Build failed with code " << result << ". Aborting hot-reload.");
            return;
        }

        SPD_INFO("Build successful. Starting hot-reload...");

        std::string newDLLPath = CreateHotReloadCopyPath(m_hotReloadCounter);
        std::string oldDLLPath = m_currentLoadedDLLPath;

        m_hotReloadCounter++;
        m_currentLoadedDLLPath = newDLLPath;

        try {
            std::filesystem::path originalDLL(m_scriptDLLPath);
            std::filesystem::path targetDLL(newDLLPath);

            std::filesystem::copy_file(originalDLL, targetDLL,
                std::filesystem::copy_options::overwrite_existing);

            SPD_INFO("Created new DLL copy: " << newDLLPath);

        } catch (const std::filesystem::filesystem_error& e) {
            SPD_ERROR("Failed to copy new DLL: " << e.what());
            m_currentLoadedDLLPath = oldDLLPath;
            m_hotReloadCounter--;
            return;
        }

        HotReloadDLL(oldDLLPath, newDLLPath);

        // Clean up old DLL copy
        try {
            if (!oldDLLPath.empty() && std::filesystem::exists(oldDLLPath)) {
                std::filesystem::remove(oldDLLPath);
                SPD_INFO("Removed old DLL copy: " << oldDLLPath);
            }
        } catch (const std::exception& e) {
            SPD_WARNING("Failed to remove old DLL: " << e.what());
        }
    }

    void ScriptingEngine::HotReloadDLL(const std::string& oldDllPath, const std::string& newDllPath) {
        SPD_INFO("=== HOT RELOAD: Swapping DLLs ===");
        SPD_INFO("  Old: " << std::filesystem::path(oldDllPath).filename().string());
        SPD_INFO("  New: " << std::filesystem::path(newDllPath).filename().string());

        // Step 1: Save all script states
        std::unordered_map<NE::ECS::Entity, ScriptState> stateToRestore;

        auto entities = GetScene().GetECSCoordinator().GetComponentManager()
            .GetEntitiesWithComponent<ECS::Component::NativeScript>();

        for (NE::ECS::Entity entity : entities) {
            auto& nsc = GetScene().GetECSCoordinator().GetComponentManager()
                .GetComponent<ECS::Component::NativeScript>(entity);

            if (!nsc.Instance) continue;

            SPD_INFO("Saving state for entity " << (int)entity << " (" << nsc.ScriptName << ")");

            ScriptState state;
            state.scriptName = nsc.ScriptName;
            state.isEnabled = nsc.Instance->IsEnabled();

            SaveSerializedFields(nsc);
            state.fields = nsc.SerializedFields;
            stateToRestore[entity] = state;

            // Disable script before destroying to prevent execution during cleanup
            nsc.Instance->SetEnabled(false);
        }

        // Step 1.5: CRITICAL - Clear all event listeners and coroutines BEFORE destroying scripts
        // This prevents dangling function pointers when the old DLL is unloaded.
        // Without this, event callbacks and coroutine continuations will point to freed memory -> CRASH
        SPD_INFO("Clearing event listeners and coroutines...");
        NANOEngine::Events::ClearScriptEventListeners();
        Engine_ClearAllCoroutines();

        // Step 1.6: Now safe to destroy script instances
        for (NE::ECS::Entity entity : entities) {
            auto& nsc = GetScene().GetECSCoordinator().GetComponentManager()
                .GetComponent<ECS::Component::NativeScript>(entity);

            if (!nsc.Instance) continue;

            OnScriptComponentDestroyed(entity);
            nsc.CreateScript = nullptr;
        }

        // Step 2: Unload old DLL and load new one
        if (IsScriptDLLLoaded()) {
            if (!UnloadScriptDLL()) {
                SPD_ERROR("Failed to unload old DLL. Attempting to load new one anyway...");
            } else {
                SPD_INFO("Unloaded old DLL");
            }
        }

        if (!LoadScriptDLL(newDllPath)) {
            SPD_ERROR("HOT RELOAD FAILED: Could not load new DLL");
            SPD_ERROR("Error: " << GetLastError());
            return;
        }

        SPD_INFO("New DLL loaded. Restoring script states...");
        PrintSummary();

        // Step 3: Restore all script states
        for (auto const& [entity, state] : stateToRestore) {
            auto& componentMgr = GetScene().GetECSCoordinator().GetComponentManager();

            if (!componentMgr.HasComponent<ECS::Component::NativeScript>(entity)) {
                SPD_WARNING("Entity " << (int)entity << " no longer exists, skipping");
                continue;
            }

            auto& nsc = componentMgr.GetComponent<ECS::Component::NativeScript>(entity);
            nsc.CreateScript = GetScriptFactory(state.scriptName);
            nsc.DestroyScript = [](IScript* script) { delete script; };

            if (!nsc.CreateScript) {
                SPD_ERROR("Script factory not found for '" << state.scriptName << "', skipping");
                nsc.Instance = nullptr;  // Ensure instance is null
                continue;
            }

            try {
                nsc.Instance = nsc.CreateScript();

                if (!nsc.Instance) {
                    SPD_ERROR("Failed to create script instance for '" << state.scriptName << "'");
                    continue;
                }

                Scripting::LinkScriptToEngine(nsc.Instance, &componentMgr);
                nsc.Instance->_SetEntity(entity);
                nsc.Instance->Awake();
                nsc.Instance->Initialize(entity);

                nsc.SerializedFields = state.fields;
                RestoreSerializedFields(nsc);

                nsc.Instance->SetEnabled(false);
            }
            catch (const std::exception& e) {
                SPD_ERROR("Exception creating script '" << state.scriptName << "': " << e.what());
                nsc.Instance = nullptr;
            }
        }

        // Enable all scripts after full initialization (with null checks)
        for (auto const& [entity, state] : stateToRestore) {
            auto& componentMgr = GetScene().GetECSCoordinator().GetComponentManager();

            if (!componentMgr.HasComponent<ECS::Component::NativeScript>(entity)) {
                continue;
            }

            auto& nsc = componentMgr.GetComponent<ECS::Component::NativeScript>(entity);

            if (nsc.Instance) {  // ✅ Only enable if instance exists
                nsc.Instance->SetEnabled(state.isEnabled);
            }
        }

        SPD_INFO("=== HOT RELOAD COMPLETE ===");
    }

    void ScriptingEngine::SaveSerializedFields(NE::ECS::Component::NativeScript& nsc) {
        if (!nsc.Instance) return;

        // Clear existing serialized fields
        nsc.SerializedFields.clear();

        // Get all exposed field names from the script
        auto fieldNames = nsc.Instance->GetExposedFieldNames();

        // Save each field's current value as a string
        for (const auto& fieldName : fieldNames) {
            std::string value = nsc.Instance->GetFieldValueAsString(fieldName);
            nsc.SerializedFields[fieldName] = value;
        }

        SPD_DEBUG("Saved " << nsc.SerializedFields.size() << " fields for script '" << nsc.ScriptName << "'");
    }

    void ScriptingEngine::RestoreSerializedFields(NE::ECS::Component::NativeScript& nsc) {
        if (!nsc.Instance || nsc.SerializedFields.empty()) return;

        // Restore each serialized field value
        for (const auto& [fieldName, value] : nsc.SerializedFields) {
            bool success = nsc.Instance->SetFieldValueFromString(fieldName, value);
            if (!success) {
                SPD_WARNING("Failed to restore field '" << fieldName << "' for script '" << nsc.ScriptName << "'");
            }
        }

        SPD_DEBUG("Restored " << nsc.SerializedFields.size() << " fields for script '" << nsc.ScriptName << "'");
    }
    
    void ScriptingEngine::OnScriptComponentDestroyed(NE::ECS::Entity entity) {
        auto& nsc = GetScene().GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
        if (nsc.Instance) {
            // Only call OnDestroy if the script was actually started
            // Editor scene scripts are never started, so skip OnDestroy for them
            if (nsc.Instance->_HasStarted()) {
                nsc.Instance->OnDestroy();
            }

            // Always clean up the instance itself
            if (nsc.DestroyScript) {
                nsc.DestroyScript(nsc.Instance);
            } else {
                delete nsc.Instance; // Fallback
            }
            nsc.Instance = nullptr;
            SPD_INFO("Destroyed script '" << nsc.ScriptName << "' for entity " << (int)entity);
        }
    }

} // namespace NE::Scripting