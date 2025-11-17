#include "ScriptingEngine.hpp"
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <Windows.h>
#include "Core/SpdLogger.hpp"
#include "ScriptContextFactory.hpp"

#include "Engine.hpp"
#include "SceneManagement/Scene.hpp"

namespace {
    struct ScriptState {
        std::string scriptName;
        bool isEnabled;
        std::unordered_map<std::string, std::string> fields;
    };

    std::string GetHotReloadPath(const std::string& originalPath, int version) {
        std::filesystem::path path(originalPath);
        std::string stem = path.stem().string();
        std::string ext = path.extension().string();
        std::string newFilename = stem + "_hot_" + std::to_string(version) + ext;
        return path.replace_filename(newFilename).string();
    }

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

    bool ScriptingEngine::LoadGameDLL(const std::string& dllPath) {
        try {
            if (!ValidateDLLPath(dllPath)) {
                SetLastError("Invalid DLL path: " + dllPath);
                return false;
            }

            std::string dllName = GetDLLName(dllPath);
            if (IsDLLLoaded(dllName)) {
                SetLastError("DLL already loaded: " + dllName);
                return false;
            }

            return LoadSingleDLL(dllPath);

        }
        catch (const std::exception& e) {
            SetLastError("Exception loading DLL '" + dllPath + "': " + e.what());
            return false;
        }
    }

    int ScriptingEngine::LoadAllDLLsInDirectory(const std::string& directory) {
        int loadedCount = 0;

        try {
            if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
                SetLastError("Directory does not exist: " + directory);
                return 0;
            }

            SPD_INFO("Scanning directory for DLLs: " << directory);

            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                    std::string dllPath = entry.path().string();
                    SPD_INFO("Found DLL: " << dllPath);

                    if (LoadGameDLL(dllPath)) {
                        loadedCount++;
                    }
                    else {
                        SPD_ERROR("Failed to load DLL: " << dllPath << " - " << GetLastError());
                    }
                }
            }

            SPD_INFO("Successfully loaded " << loadedCount << " DLLs from directory: " << directory);

        }
        catch (const std::filesystem::filesystem_error& e) {
            SetLastError("Filesystem error: " + std::string(e.what()));
        }
        catch (const std::exception& e) {
            SetLastError("Exception while loading DLLs: " + std::string(e.what()));
        }

        return loadedCount;
    }

    bool ScriptingEngine::UnloadDLL(const std::string& dllName) {
        LoadedDLL* dll = FindLoadedDLL(dllName);
        if (!dll) {
            SetLastError("DLL not found: " + dllName);
            return false;
        }

        // Remove scripts that were registered by this DLL
        //SPD_INFO("Unloading DLL: " << dllName << " (scripts will remain registered)");

        ClearRegisteredScripts();
       

        bool success = UnloadSingleDLL(*dll);
        if (success) {
            // Remove from loaded DLLs list
            m_loadedDLLs.erase(
                std::remove_if(m_loadedDLLs.begin(), m_loadedDLLs.end(),
                    [&dllName](const LoadedDLL& d) { return d.name == dllName; }),
                m_loadedDLLs.end());
        }

        return success;
    }

    void ScriptingEngine::UnloadAllDLLs() {
        SPD_INFO("Unloading all DLLs...");

        for (auto& dll : m_loadedDLLs) {
            UnloadSingleDLL(dll);
        }
        m_loadedDLLs.clear();


        SPD_INFO("All DLLs unloaded and scripts cleared.");
    }

    const std::vector<ScriptingEngine::LoadedDLL>& ScriptingEngine::GetLoadedDLLs() const {
        return m_loadedDLLs;
    }

    bool ScriptingEngine::IsDLLLoaded(const std::string& dllName) const {
        return FindLoadedDLL(dllName) != nullptr;
    }

    // === Engine Lifecycle ===

    void ScriptingEngine::Initialize() {
        if (m_initialized) {
            SPD_INFO("ScriptingEngine already initialized.");
            return;
        }

        // DLL Path
        m_scriptDLLPath = "ChronoGame.dll";
        m_scriptDLLPath = std::filesystem::absolute(m_scriptDLLPath).string();
        SPD_INFO("Loading script DLL: " << m_scriptDLLPath);

        // Source Directory
        m_scriptSourceDirectory = "../../../ChronoGame/Scripts/";
        m_scriptSourceDirectory = std::filesystem::absolute(m_scriptSourceDirectory).string();

        // Build Command
        /*const char* vsPath = std::getenv("VSINSTALLDIR");
        if (vsPath)
        {*/
        std::string msbuildPath = GetMSBuildPath();
        if (msbuildPath.empty())
            msbuildPath = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe"; // fallback to PATH

        //std::string msbuildPath = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe";
        //m_scriptBuildCommand = "cmd /C \""
        //    + msbuildPath
        //    + "\" \"../../../NANOEngine.sln\" "
        //    "/t:GameCode "
        //    "/p:Configuration=Release "
        //    "/p:Platform=x64 "
        //    "/p:LanguageStandard=stdcpp20";

        //m_scriptBuildCommand =
        //    "cmd /C \"\"" + msbuildPath + "\" "       // <-- double quotes "" after /C
        //    "\"../../../NANOEngine.sln\" " // <-- full .sln path in quotes
        //    "/t:GameCode "
        //    "/p:Configuration=Release "
        //    "/p:Platform=x64 "
        //    "/p:LanguageStandard=stdcpp20\"";        // <-- final closing quote
        m_scriptBuildCommand =
            "\"\"" + msbuildPath + "\" "
            "\"../../../ChronoGame/ChronoGame.vcxproj\" "
            "/p:Configuration=Release "
            "/p:Platform=x64 "
            "/p:BuildProjectReferences=false"
            "/p:LanguageStandard=stdcpp20\"";


        std::cout << "Command: " << m_scriptBuildCommand << std::endl;
        //}

           // + "\" \"D:/Users/Irwen Yap/Documents/My Projects/Github/NANOEngine\" "
        // Instead of loading the original, we load a *copy*
        m_currentLoadedDLLPath = GetHotReloadPath(m_scriptDLLPath, m_hotReloadCounter);
        m_hotReloadCounter++;


        try {
            std::filesystem::path originalDLL(m_scriptDLLPath);
            //std::filesystem::path originalPDB = originalDLL.replace_extension(".pdb");

            std::filesystem::path targetDLL(m_currentLoadedDLLPath);
            //std::filesystem::path targetPDB = targetDLL.replace_extension(".pdb");

            // Copy the DLL and PDB to our new temp path
            std::filesystem::copy_file(originalDLL, targetDLL, std::filesystem::copy_options::overwrite_existing);
            /*if (std::filesystem::exists(originalPDB)) {
                std::filesystem::copy_file(originalPDB, targetPDB, std::filesystem::copy_options::overwrite_existing);
            }*/

            SPD_INFO("Loading copy: " << m_currentLoadedDLLPath);
            if (LoadGameDLL(m_currentLoadedDLLPath)) {
                SPD_ERROR("Failed to load script DLL copy: " << m_currentLoadedDLLPath);
                SPD_ERROR("Last Error: " << GetLastError());
            } else {
                SPD_INFO("Successfully loaded DLL.");
                PrintSummary();
            }

        } catch (const std::filesystem::filesystem_error& e) {
            SPD_ERROR("Failed to copy initial DLL for loading: " << e.what());
            SPD_WARNING("Continuing without game scripts. Hot-compile may still function.");
        }

        // --- Setup File Watcher ---
        try {
            auto fileWatchCallback = [this](const std::string& path, const filewatch::Event eventType) {
                this->HandleFileWatchEvent(path, eventType);
                };

            // Watch the SOURCE directory
            m_sourceWatcher = std::make_unique<filewatch::FileWatch<std::string>>(
                m_scriptSourceDirectory,
                std::regex(".*\\.(cpp|hpp|h)$"), // Watch for .cpp, .hpp, or .h files
                fileWatchCallback
            );
            SPD_INFO("File watcher started for: " << m_scriptSourceDirectory);
        } catch (const std::exception& e) {
            SPD_ERROR("Failed to create file watcher for " << m_scriptSourceDirectory << ": " << e.what());
            SPD_WARNING("Hot-compile will be disabled.");
            m_sourceWatcher.reset();
        }

        SPD_INFO("Initializing ScriptingEngine...");
        SPD_INFO("  - " << m_loadedDLLs.size() << " DLLs loaded");
        SPD_INFO("  - " << m_scriptFactories.size() << " scripts registered");

        PrintSummary();
        
        m_initialized = true;
        SPD_INFO("ScriptingEngine initialization complete.");
    }

    void ScriptingEngine::Shutdown() {
        if (!m_initialized) {
            return;
        }

        m_sourceWatcher.reset();
        for (NE::ECS::Entity entity : GetScene().GetECSCoordinator().GetComponentManager().GetEntitiesWithComponent<ECS::Component::NativeScript>()) {
            Scripting::ScriptingEngine::GetInstance().OnScriptComponentDestroyed(entity);
            auto& ns = GetScene().GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
            ns.CreateScript = {};   // or ns.CreateScript = nullptr; if it�s a function ptr
            ns.DestroyScript = {};
        }

        SPD_INFO("Shutting down ScriptingEngine...");
        ClearRegisteredScripts();
        UnloadAllDLLs();

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

        // Print loaded DLLs
        SPD_INFO("Loaded DLLs (" << m_loadedDLLs.size() << "):");
        if (m_loadedDLLs.empty()) {
            SPD_INFO("  (none)");
        }
        else {
            for (const auto& dll : m_loadedDLLs) {
                SPD_INFO("  - " << dll.name << " (" << dll.filepath << ")");
            }
        }

        // Print registered scripts
        auto scriptNames = GetRegisteredScriptNames();
        SPD_INFO("Registered Scripts (" << scriptNames.size() << "):");
        if (scriptNames.empty()) {
            SPD_INFO("  (none)");
        }
        else {
            for (const auto& name : scriptNames) {
                SPD_INFO("  - " << name);
            }
        }
        SPD_INFO("================================\n");
    }

    bool ScriptingEngine::ReloadDLL(const std::string& dllPath) {
        std::string dllName = GetDLLName(dllPath);

        SPD_INFO("Reloading DLL: " << dllName);

        // Unload if currently loaded
        if (IsDLLLoaded(dllName)) {
            if (!UnloadDLL(dllName)) {
                SetLastError("Failed to unload DLL for reload: " + dllName);
                return false;
            }
        }

        // Load again
        return LoadGameDLL(dllPath);
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

    std::string ScriptingEngine::GetDLLName(const std::string& filepath) const {
        std::filesystem::path path(filepath);
        return path.filename().string();
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

    ScriptingEngine::LoadedDLL* ScriptingEngine::FindLoadedDLL(const std::string& dllName) {
        auto it = std::find_if(m_loadedDLLs.begin(), m_loadedDLLs.end(),
            [&dllName](const LoadedDLL& dll) {
                return dll.name == dllName;
            });
        return (it != m_loadedDLLs.end()) ? &(*it) : nullptr;
    }

    const ScriptingEngine::LoadedDLL* ScriptingEngine::FindLoadedDLL(const std::string& dllName) const {
        auto it = std::find_if(m_loadedDLLs.begin(), m_loadedDLLs.end(),
            [&dllName](const LoadedDLL& dll) {
                return dll.name == dllName;
            });
        return (it != m_loadedDLLs.end()) ? &(*it) : nullptr;
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

    bool ScriptingEngine::LoadSingleDLL(const std::string& dllPath) {
        // Load the DLL
        HMODULE dllHandle = LoadLibraryA(dllPath.c_str());
        if (!dllHandle) {
            SetLastError("Failed to load DLL: " + dllPath + " - " + GetSystemError());
            return false;
        }

        // Get the registration function
        RegisterScriptsFunction registerFunc = (RegisterScriptsFunction)GetProcAddress(dllHandle, "RegisterEngineScripts");
        if (!registerFunc) {
            SetLastError("Failed to find 'RegisterEngineScripts' function in: " + dllPath + " - " + GetSystemError());
            FreeLibrary(dllHandle);
            return false;
        }

        try {
            std::string dllName = GetDLLName(dllPath);

            // Store the loaded DLL information
            m_loadedDLLs.emplace_back(dllHandle, dllPath, dllName, registerFunc);

            // Set temp state *before* calling registerFunc
            m_currentLoadingDLLHandle = dllHandle;

            // Call the registration function
            registerFunc(this);

            // Clear temp state
            m_currentLoadingDLLHandle = nullptr;

            SPD_INFO("Successfully loaded and registered scripts from: " << dllPath);
            return true;

        }
        catch (const std::exception& e) {
            SetLastError("Exception during script registration: " + std::string(e.what()));
            m_currentLoadingDLLHandle = nullptr; // Clear temp state
            FreeLibrary(dllHandle);
            return false;
        }
        catch (...) {
            SetLastError("Unknown exception during script registration");
            m_currentLoadingDLLHandle = nullptr; // Clear temp state
            FreeLibrary(dllHandle);
            return false;
        }
    }

    bool ScriptingEngine::UnloadSingleDLL(LoadedDLL& dll) {
        if (FreeLibrary(dll.handle)) {
            SPD_INFO("Unloaded DLL: " << dll.name);
            return true;
        }
        else {
            SetLastError("Failed to unload DLL: " + dll.name + " - " + GetSystemError());
            return false;
        }
    }

    void ScriptingEngine::HandleFileWatchEvent(const std::string& path, const filewatch::Event eventType) {
        // This runs on the file watcher's thread
        if (eventType == filewatch::Event::modified || eventType == filewatch::Event::renamed_new || eventType == filewatch::Event::added) {

            SPD_INFO("FileWatch: Detected change in: " << path);

            // Just set the flag. The main thread will handle the rest.
            m_compileQueued.store(true);
        }
    }

    // --- Hot Compile & Reload Implementation ---


    void ScriptingEngine::HotCompileAndReload() {
        // This runs on the main thread
        SPD_INFO("--- BEGIN HOT COMPILE ---");
        SPD_INFO("Executing build command: " << m_scriptBuildCommand);

        // Run the build command and block until it's done
        int result = std::system(m_scriptBuildCommand.c_str());

        if (result != 0) {
            SPD_ERROR("Build command failed with code " << result << ". Aborting hot-reload.");
            return;
        }

        SPD_INFO("Compile successful. Proceeding with hot-reload...");

        // hard path for now
        std::filesystem::path builtDLL =
            "../../bin/ChronoGame/Release-x64/ChronoGame.dll";

        // Copy Files to New Temp Path 
        std::string newDLLPath = GetHotReloadPath(m_scriptDLLPath, m_hotReloadCounter);
        std::string oldDLLPath = m_currentLoadedDLLPath;

        m_hotReloadCounter++; // Increment *after* getting new path
        m_currentLoadedDLLPath = newDLLPath;

        try {
            std::filesystem::path originalDLL(m_scriptDLLPath);
            //std::filesystem::path originalPDB = originalDLL.replace_extension(".pdb");
            SPD_INFO("OG DLL: " << originalDLL);
            std::filesystem::path targetDLL(newDLLPath);
            SPD_INFO("NEW DLL: " << targetDLL);
            //std::filesystem::path targetPDB = targetDLL.replace_extension(".pdb");

            // Copy the newly-built files to our new temp path
            std::filesystem::copy_file(originalDLL, targetDLL, std::filesystem::copy_options::overwrite_existing);
            SPD_INFO("DLL COPIED");
            /*if (std::filesystem::exists(originalPDB)) {
                std::filesystem::copy_file(originalPDB, targetPDB, std::filesystem::copy_options::overwrite_existing);
            }*/
            SPD_INFO("Copied new DLL to: " << newDLLPath);

        } catch (const std::filesystem::filesystem_error& e) {
            SPD_ERROR("Failed to copy new DLL for reload: " << e.what());
            m_currentLoadedDLLPath = oldDLLPath; // Revert path
            m_hotReloadCounter--; // Revert counter
            return;
        }

        // 3. Run Hot Reload on the *New* File 
        HotReloadDLL(oldDLLPath, newDLLPath); // This reloads using the new copy

        // 4. Clean Up Old File (Optional)
        // The old DLL (e.g., GameCode_hot_0.dll) is now unloaded and can be deleted.
        try {
            if (!oldDLLPath.empty() && std::filesystem::exists(oldDLLPath)) {
                std::filesystem::path dllPath(oldDLLPath);
                //std::filesystem::path pdbPath = dllPath.replace_extension(".pdb");

                std::filesystem::remove(dllPath);
                /*if (std::filesystem::exists(pdbPath)) {
                    std::filesystem::remove(pdbPath);
                }*/
                SPD_INFO("Cleaned up old DLL: " << oldDLLPath);
            }
        } catch (const std::exception& e) {
            SPD_WARNING("Could not clean up old DLL: " << oldDLLPath << " - " << e.what());
        }
    }

    void ScriptingEngine::HotReloadDLL(const std::string& oldDllPath, const std::string& newDllPath) {
        // It handles state serialization, DLL swapping, and state restoration.

        std::string newDllName = std::filesystem::path(newDllPath).filename().string();
        std::string oldDllName = std::filesystem::path(oldDllPath).filename().string();
        SPD_INFO("--- BEGIN HOT RELOAD: " << oldDllName << " -> " << newDllName << " ---");

        // 1. --- Store State ---
        std::unordered_map<NE::ECS::Entity, ScriptState> stateToRestore;

        auto entities = GetScene().GetECSCoordinator().GetComponentManager().GetEntitiesWithComponent<ECS::Component::NativeScript>();
        for (NE::ECS::Entity entity : entities) {
            auto& nsc = GetScene().GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
            if (!nsc.Instance) continue;

            SPD_INFO("Serializing state for entity " << (int)entity << " (Script: " << nsc.ScriptName << ")");

            ScriptState state;
            state.scriptName = nsc.ScriptName;
            state.isEnabled = nsc.Instance->IsEnabled();


            SaveSerializedFields(nsc);
            state.fields = nsc.SerializedFields;

            stateToRestore[entity] = state;

            OnScriptComponentDestroyed(entity);

            nsc.CreateScript = nullptr;

        }

        // 2. --- Reload the DLL ---

        if (!oldDllName.empty() && IsDLLLoaded(oldDllName)) {
            if (!UnloadDLL(oldDllName)) {
                SPD_ERROR("--- HOT RELOAD FAILED (Unload): Failed to unload old DLL: " << oldDllName << " ---");
                // This is bad, but we might as well try to load the new one anyway
                // so the user can at least try to recover without restarting.
            } else {
                SPD_INFO("Unloaded old DLL: " << oldDllName);
            }
        }

        bool loadSuccess = LoadGameDLL(newDllPath);
        if (!loadSuccess) {
            SPD_ERROR("--- HOT RELOAD FAILED (Load): " << GetLastError() << " ---");
            return; // Can't restore state if load failed
        }

        SPD_INFO("DLL reloaded. Restoring script states...");
        PrintSummary();

        // 3. --- Restore State ---
        for (auto const& [entity, state] : stateToRestore) {
            if (!GetScene().GetECSCoordinator().GetComponentManager().HasComponent<ECS::Component::NativeScript>(entity)) {
                SPD_WARNING("Entity " << (int)entity << " no longer exists. Cannot restore script.");
                continue;
            }

            auto& nsc = GetScene().GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
            nsc.CreateScript = GetScriptFactory(state.scriptName);
            nsc.DestroyScript = [](IScript* script) { delete script; };

            if (!nsc.CreateScript) {
                SPD_ERROR("Failed to find new script factory for '" << state.scriptName << "'. Cannot restore state.");
                continue;
            }

            nsc.Instance = nsc.CreateScript();
            Scripting::LinkScriptToEngine(nsc.Instance, &GetScene().GetECSCoordinator().GetComponentManager());
            nsc.Instance->_SetEntity(entity);
            nsc.Instance->Awake();
            nsc.Instance->Initialize(entity);

            nsc.SerializedFields = state.fields; // Give the component its old data
            RestoreSerializedFields(nsc);

            nsc.Instance->SetEnabled(false);
        }

        // Only enable when all scripts are fully initialized
        for (auto const& [entity, state] : stateToRestore) {
            auto& nsc = GetScene().GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
            nsc.Instance->SetEnabled(state.isEnabled);
        }



        SPD_INFO("--- END HOT RELOAD ---");
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
            nsc.Instance->OnDestroy();
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