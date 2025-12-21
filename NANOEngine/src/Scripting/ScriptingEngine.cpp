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
    // ScriptState moved to ScriptingEngine class definition

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

    ScriptingEngine& ScriptingEngine::GetInstance() {
        static ScriptingEngine instance;
        return instance;
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

    void ScriptingEngine::Initialize(const ScriptEngineConfig& config) {
        if (m_initialized) {
            SPD_INFO("ScriptingEngine already initialized.");
            return;
        }

        SPD_INFO("Initializing ScriptingEngine...");

        // Set up paths from config
        m_scriptDLLPath = config.scriptDLLName;
        m_scriptDLLPath = std::filesystem::absolute(m_scriptDLLPath).string();
        SPD_INFO("Script DLL path: " << m_scriptDLLPath);

        m_scriptSourceDirectory = config.scriptSourceDirectory;
        m_scriptSourceDirectory = std::filesystem::absolute(m_scriptSourceDirectory).string();
        SPD_INFO("Script source directory: " << m_scriptSourceDirectory);

        // Configure build command from config
        std::string msbuildPath = GetMSBuildPath();
        if (msbuildPath.empty()) {
            msbuildPath = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe";
            SPD_WARNING("MSBuild not found via vswhere, using fallback path");
        }

        m_scriptBuildCommand =
            "\"\"" + msbuildPath + "\" "
            "\"" + config.scriptProjectPath + "\" "
            "/p:Configuration=" + config.buildConfiguration + " "
            "/p:Platform=" + config.buildPlatform + " "
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
        // Only trigger on modifications to existing files, not new file creation
        // This prevents hot reload when creating new scripts that haven't been compiled yet
        if (eventType == filewatch::Event::modified) {
            SPD_INFO("File change detected: " << path);
            m_compileQueued.store(true);
        }
        // Ignore 'added' and 'renamed_new' events to prevent premature hot reload
    }

    void ScriptingEngine::HotCompileAndReload() {
        SPD_INFO("=== HOT COMPILE & RELOAD BEGIN ===");
        SPD_INFO("Executing: " << m_scriptBuildCommand);

        // Clear previous error
        SetLastError("");

        int result = std::system(m_scriptBuildCommand.c_str());

        if (result != 0) {
            std::string errorMsg = "Build failed with exit code " + std::to_string(result) +
                                   ". Check build output for errors. Command: " + m_scriptBuildCommand;
            SPD_ERROR(errorMsg);
            SetLastError(errorMsg);
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
            std::string errorMsg = std::string("Failed to copy new DLL: ") + e.what();
            SPD_ERROR(errorMsg);
            SetLastError(errorMsg);
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

    std::unordered_map<NE::ECS::Entity, ScriptingEngine::ScriptState>
    ScriptingEngine::SaveAllScriptStates() {
        std::unordered_map<NE::ECS::Entity, ScriptState> stateToRestore;

        auto entities = GetScene().GetECSCoordinator().GetComponentManager()
            .GetEntitiesWithComponent<ECS::Component::NativeScript>();

        for (NE::ECS::Entity entity : entities) {
            auto& nsc = GetScene().GetECSCoordinator().GetComponentManager()
                .GetComponent<ECS::Component::NativeScript>(entity);

            auto it = m_scriptInstances.find(entity);
            if (it == m_scriptInstances.end()) continue;

            IScript* instance = it->second;

            SPD_INFO("Saving state for entity " << (int)entity << " (" << nsc.ScriptName << ")");

            ScriptState state;
            state.scriptName = nsc.ScriptName;
            state.isEnabled = instance->IsEnabled();

            SaveSerializedFields(nsc);
            state.fields = nsc.SerializedFields;
            stateToRestore[entity] = state;

            // Disable script before destroying to prevent execution during cleanup
            instance->SetEnabled(false);
        }

        return stateToRestore;
    }

    void ScriptingEngine::DestroyAllScriptInstances() {
        // CRITICAL: Clear all event listeners and coroutines BEFORE destroying scripts
        // This prevents dangling function pointers when the old DLL is unloaded
        SPD_INFO("Clearing event listeners and coroutines...");
        NANOEngine::Events::ClearScriptEventListeners();
        Engine_ClearAllCoroutines();

        // Now safe to destroy script instances
        // Make a copy of the keys to avoid iterator invalidation
        std::vector<NE::ECS::Entity> entities;
        entities.reserve(m_scriptInstances.size());
        for (auto& [entity, instance] : m_scriptInstances) {
            entities.push_back(entity);
        }

        for (NE::ECS::Entity entity : entities) {
            OnScriptComponentDestroyed(entity);
        }
    }

    bool ScriptingEngine::SwapDLLs(const std::string& oldDllPath, const std::string& newDllPath) {
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
            return false;
        }

        SPD_INFO("New DLL loaded. Restoring script states...");
        PrintSummary();
        return true;
    }

    void ScriptingEngine::RestoreAllScriptStates(
        const std::unordered_map<NE::ECS::Entity, ScriptState>& stateToRestore) {

        for (auto const& [entity, state] : stateToRestore) {
            auto& componentMgr = GetScene().GetECSCoordinator().GetComponentManager();
            auto& entityMgr = GetScene().GetECSCoordinator().GetEntityManager();

            if (!componentMgr.HasComponent<ECS::Component::NativeScript>(entity)) {
                SPD_WARNING("Entity " << (int)entity << " no longer exists, skipping");
                continue;
            }

            auto& nsc = componentMgr.GetComponent<ECS::Component::NativeScript>(entity);

            auto factory = GetScriptFactory(state.scriptName);
            if (!factory) {
                SPD_ERROR("Script factory not found for '" << state.scriptName << "', skipping");
                continue;
            }

            try {
                // Create instance using the new method
                IScript* instance = factory();

                if (!instance) {
                    SPD_ERROR("Failed to create script instance for '" << state.scriptName << "'");
                    continue;
                }

                Scripting::LinkScriptToEngine(instance, &componentMgr, &entityMgr);
                instance->_SetEntity(entity);
                instance->Awake();
                instance->Initialize(entity);

                nsc.SerializedFields = state.fields;

                // Store instance in map
                m_scriptInstances[entity] = instance;

                RestoreSerializedFields(nsc);

                instance->SetEnabled(false);
            }
            catch (const std::exception& e) {
                SPD_ERROR("Exception creating script '" << state.scriptName << "': " << e.what());
            }
        }
    }

    void ScriptingEngine::EnableScripts(
        const std::unordered_map<NE::ECS::Entity, ScriptState>& stateToRestore) {

        for (auto const& [entity, state] : stateToRestore) {
            auto it = m_scriptInstances.find(entity);
            if (it != m_scriptInstances.end()) {
                it->second->SetEnabled(state.isEnabled);
            }
        }
    }

    void ScriptingEngine::HotReloadDLL(const std::string& oldDllPath, const std::string& newDllPath) {
        SPD_INFO("=== HOT RELOAD: Swapping DLLs ===");
        SPD_INFO("  Old: " << std::filesystem::path(oldDllPath).filename().string());
        SPD_INFO("  New: " << std::filesystem::path(newDllPath).filename().string());

        // Step 1: Save all script states
        auto stateToRestore = SaveAllScriptStates();

        // Step 2: Destroy all script instances and clear event listeners
        DestroyAllScriptInstances();

        // Step 3: Unload old DLL and load new one
        if (!SwapDLLs(oldDllPath, newDllPath)) {
            return;
        }

        // Step 4: Restore all script states
        RestoreAllScriptStates(stateToRestore);

        // Step 5: Enable scripts after full initialization
        EnableScripts(stateToRestore);

        SPD_INFO("=== HOT RELOAD COMPLETE ===");
    }

    void ScriptingEngine::SaveSerializedFields(NE::ECS::Component::NativeScript& nsc) {
        // Find the instance by iterating through instances (we don't have entity here in old code path)
        // This is for backward compatibility with old hot-reload code
        IScript* instance = nullptr;
        for (auto& [entity, inst] : m_scriptInstances) {
            if (m_componentManager && m_componentManager->HasComponent<ECS::Component::NativeScript>(entity)) {
                auto& entityNsc = m_componentManager->GetComponent<ECS::Component::NativeScript>(entity);
                if (&entityNsc == &nsc) {
                    instance = inst;
                    break;
                }
            }
        }

        if (!instance) return;

        // Clear existing serialized fields
        nsc.SerializedFields.clear();

        // Get all exposed field names from the script
        auto fieldNames = instance->GetExposedFieldNames();

        // Save each field's current value as a string
        for (const auto& fieldName : fieldNames) {
            std::string value = instance->GetFieldValueAsString(fieldName);
            nsc.SerializedFields[fieldName] = value;
        }

        SPD_DEBUG("Saved " << nsc.SerializedFields.size() << " fields for script '" << nsc.ScriptName << "'");
    }

    void ScriptingEngine::RestoreSerializedFields(NE::ECS::Component::NativeScript& nsc) {
        if (nsc.SerializedFields.empty()) return;

        // Find the instance by iterating through instances
        IScript* instance = nullptr;
        for (auto& [entity, inst] : m_scriptInstances) {
            if (m_componentManager && m_componentManager->HasComponent<ECS::Component::NativeScript>(entity)) {
                auto& entityNsc = m_componentManager->GetComponent<ECS::Component::NativeScript>(entity);
                if (&entityNsc == &nsc) {
                    instance = inst;
                    break;
                }
            }
        }

        if (!instance) return;

        // Restore each serialized field value
        for (const auto& [fieldName, value] : nsc.SerializedFields) {
            bool success = instance->SetFieldValueFromString(fieldName, value);
            if (!success) {
                SPD_WARNING("Failed to restore field '" << fieldName << "' for script '" << nsc.ScriptName << "'");
            }
        }

        SPD_DEBUG("Restored " << nsc.SerializedFields.size() << " fields for script '" << nsc.ScriptName << "'");
    }
    
    void ScriptingEngine::OnScriptComponentDestroyed(NE::ECS::Entity entity) {
        auto& nsc = GetScene().GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
        auto it = m_scriptInstances.find(entity);
        if (it != m_scriptInstances.end()) {
            IScript* instance = it->second;

            // Only call OnDestroy if the script was actually started
            // Editor scene scripts are never started, so skip OnDestroy for them
            if (instance->_HasStarted()) {
                instance->OnDestroy();
            }

            // Always clean up the instance itself
            delete instance;
            m_scriptInstances.erase(it);

            SPD_INFO("Destroyed script '" << nsc.ScriptName << "' for entity " << (int)entity);
        }
    }

    // === Instance Management (ECS Refactor) ===

    void ScriptingEngine::SetECSReferences(NE::ECS::ComponentManager* componentManager, NE::ECS::EntityManager* entityManager) {
        m_componentManager = componentManager;
        m_entityManager = entityManager;
        SPD_INFO("ScriptEngine: ECS references set");
    }

    bool ScriptingEngine::CreateScriptInstance(NE::ECS::Entity entity, NE::ECS::Component::NativeScript& nsc) {
        // Check if instance already exists
        if (m_scriptInstances.find(entity) != m_scriptInstances.end()) {
            SPD_WARNING("Script instance already exists for entity " << (int)entity);
            return false;
        }

        // Check if script name is empty
        if (nsc.ScriptName.empty()) {
            return false;
        }

        // Get the factory function for this script
        auto factory = GetScriptFactory(nsc.ScriptName);
        if (!factory) {
            SPD_WARNING("Cannot create script '" << nsc.ScriptName
                << "' for entity " << (int)entity
                << " - script not found in DLL. It may have been deleted or renamed.");
            return false;
        }

        // Create the instance
        IScript* instance = factory();
        if (!instance) {
            SPD_ERROR("Failed to create script instance for '" << nsc.ScriptName << "'");
            return false;
        }

        // Link to engine
        Scripting::LinkScriptToEngine(instance, m_componentManager, m_entityManager);
        instance->_SetEntity(entity);

        // Store the instance
        m_scriptInstances[entity] = instance;

        SPD_INFO("Created script instance '" << nsc.ScriptName << "' for entity " << (int)entity);
        return true;
    }

    void ScriptingEngine::DestroyScriptInstance(NE::ECS::Entity entity) {
        auto it = m_scriptInstances.find(entity);
        if (it == m_scriptInstances.end()) {
            return; // No instance to destroy
        }

        IScript* instance = it->second;

        // Only call OnDestroy if the script was started
        if (instance->_HasStarted()) {
            instance->OnDestroy();
        }

        // Clean up the instance
        delete instance;
        m_scriptInstances.erase(it);

        SPD_INFO("Destroyed script instance for entity " << (int)entity);
    }

    IScript* ScriptingEngine::GetScriptInstance(NE::ECS::Entity entity) const {
        auto it = m_scriptInstances.find(entity);
        return (it != m_scriptInstances.end()) ? it->second : nullptr;
    }

    bool ScriptingEngine::HasScriptInstance(NE::ECS::Entity entity) const {
        return m_scriptInstances.find(entity) != m_scriptInstances.end();
    }

    void ScriptingEngine::InitializeScriptInstance(NE::ECS::Entity entity) {
        auto it = m_scriptInstances.find(entity);
        if (it == m_scriptInstances.end()) {
            return;
        }

        IScript* instance = it->second;

        // Call Awake() first (even if disabled)
        instance->Awake();

        // Then Initialize()
        instance->Initialize(entity);

        // Restore serialized field values if they exist
        if (m_componentManager && m_componentManager->HasComponent<ECS::Component::NativeScript>(entity)) {
            auto& nsc = m_componentManager->GetComponent<ECS::Component::NativeScript>(entity);
            RestoreSerializedFields(nsc);
        }

        // Start disabled
        instance->SetEnabled(false);

        SPD_INFO("Initialized script instance for entity " << (int)entity);
    }

    void ScriptingEngine::UpdateScriptInstances(double deltaTime) {
        for (auto& [entity, instance] : m_scriptInstances) {
            if (!instance) continue;

            if (instance->IsEnabled()) {
                // Call Start() before first Update() if not yet called
                if (!instance->_HasStarted()) {
                    instance->Start();
                    instance->_MarkStartCalled();
                }

                instance->Update(deltaTime);
            }
        }
    }

    void ScriptingEngine::StartAllScriptInstances() {
        SPD_INFO("ScriptEngine: Starting all script instances...");
        for (auto& [entity, instance] : m_scriptInstances) {
            if (instance) {
                instance->_RefreshComponentReferences();
                instance->SetEnabled(true);
            }
        }
    }

    void ScriptingEngine::PauseAllScriptInstances() {
        SPD_INFO("ScriptEngine: Pausing all script instances...");
        for (auto& [entity, instance] : m_scriptInstances) {
            if (instance) {
                instance->SetEnabled(false);
            }
        }
    }

    void ScriptingEngine::StopAllScriptInstances() {
        SPD_INFO("ScriptEngine: Stopping all script instances...");

        if (!m_componentManager) {
            SPD_WARNING("ComponentManager not set, cannot save serialized fields");
            return;
        }

        for (auto& [entity, instance] : m_scriptInstances) {
            if (instance) {
                // Disable the script
                instance->SetEnabled(false);

                // Save field values for potential restoration
                if (m_componentManager->HasComponent<ECS::Component::NativeScript>(entity)) {
                    auto& nsc = m_componentManager->GetComponent<ECS::Component::NativeScript>(entity);
                    SaveSerializedFields(nsc);
                }
            }
        }
    }

    // === Scene Script Management Helpers ===

    void ScriptingEngine::BeginSceneLoad() {
        m_allowEntityAddedCallbacks = false;
    }

    void ScriptingEngine::EndSceneLoad() {
        m_allowEntityAddedCallbacks = true;
    }

    bool ScriptingEngine::ShouldCreateInstancesOnEntityAdded() const {
        return m_allowEntityAddedCallbacks;
    }

    void ScriptingEngine::SaveSceneScriptFields(NE::ECS::ComponentManager& componentManager) {
        auto& entities = componentManager.GetEntitiesWithComponent<ECS::Component::NativeScript>();

        SPD_INFO("SaveSceneScriptFields: Processing " << entities.size() << " entities with NativeScript components");

        for (NE::ECS::Entity entity : entities) {
            auto& nsc = componentManager.GetComponent<ECS::Component::NativeScript>(entity);

            // Only save if there's an instance for this entity
            auto it = m_scriptInstances.find(entity);
            if (it != m_scriptInstances.end() && it->second) {
                SPD_INFO("  Saving fields for entity " << (int)entity << " script '" << nsc.ScriptName << "'");
                SaveSerializedFields(nsc);
            } else {
                SPD_WARNING("  No instance found for entity " << (int)entity << " script '" << nsc.ScriptName << "' - skipping save");
            }
        }
    }

    void ScriptingEngine::TransferScriptFields(
        NE::ECS::ComponentManager& sourceComponentManager,
        NE::ECS::ComponentManager& targetComponentManager) {

        // Build map of LUID -> SerializedFields from source scene
        std::unordered_map<uint64_t, std::unordered_map<std::string, std::string>> fieldsByLUID;
        std::unordered_map<uint64_t, std::unordered_set<std::string>> refFieldsByLUID;

        auto& sourceEntities = sourceComponentManager.GetEntitiesWithComponent<ECS::Component::NativeScript>();
        SPD_INFO("TransferScriptFields: Source has " << sourceEntities.size() << " entities");

        for (NE::ECS::Entity entity : sourceEntities) {
            auto& nsc = sourceComponentManager.GetComponent<ECS::Component::NativeScript>(entity);

            SPD_INFO("  Source LUID " << nsc.luid << " (entity " << (int)entity << "): " << nsc.SerializedFields.size() << " fields");

            // Store fields by LUID for matching
            fieldsByLUID[nsc.luid] = nsc.SerializedFields;
            refFieldsByLUID[nsc.luid] = nsc.EntityReferenceFields;
        }

        // Apply to target scene by matching LUIDs
        auto& targetEntities = targetComponentManager.GetEntitiesWithComponent<ECS::Component::NativeScript>();
        SPD_INFO("TransferScriptFields: Target has " << targetEntities.size() << " entities");

        for (NE::ECS::Entity entity : targetEntities) {
            auto& targetNsc = targetComponentManager.GetComponent<ECS::Component::NativeScript>(entity);

            // Find matching source component by LUID
            auto fieldsIt = fieldsByLUID.find(targetNsc.luid);
            if (fieldsIt != fieldsByLUID.end()) {
                SPD_INFO("  Target LUID " << targetNsc.luid << " (entity " << (int)entity << "): Transferred " << fieldsIt->second.size() << " fields");
                targetNsc.SerializedFields = fieldsIt->second;
            } else {
                SPD_WARNING("  Target LUID " << targetNsc.luid << " (entity " << (int)entity << "): No matching source found");
            }

            auto refFieldsIt = refFieldsByLUID.find(targetNsc.luid);
            if (refFieldsIt != refFieldsByLUID.end()) {
                targetNsc.EntityReferenceFields = refFieldsIt->second;
            }
        }
    }

    void ScriptingEngine::RecreateScriptInstances(
        NE::ECS::ComponentManager& componentManager,
        NE::ECS::EntityManager& entityManager) {

        // Temporarily set ECS references (in case they're pointing to old scene)
        m_componentManager = &componentManager;
        m_entityManager = &entityManager;

        auto& entities = componentManager.GetEntitiesWithComponent<ECS::Component::NativeScript>();

        for (NE::ECS::Entity entity : entities) {
            auto& nsc = componentManager.GetComponent<ECS::Component::NativeScript>(entity);

            // Skip empty script names
            if (nsc.ScriptName.empty()) continue;

            // Create instance (will initialize and restore serialized fields)
            if (CreateScriptInstance(entity, nsc)) {
                InitializeScriptInstance(entity);
            }
        }
    }

} // namespace NE::Scripting