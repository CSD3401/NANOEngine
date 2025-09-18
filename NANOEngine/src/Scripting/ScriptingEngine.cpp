#include "ScriptingEngine.hpp"
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <Windows.h>

namespace NE::Scripting {

    ScriptingEngine::ScriptingEngine()
        : m_initialized(false) {
    }

    ScriptingEngine::~ScriptingEngine() {
        Shutdown();
    }

    std::function<IScript* ()> ScriptingEngine::GetScriptFactory(const std::string& name) const {
        auto it = m_scriptFactories.find(name);
        if (it != m_scriptFactories.end()) {
            return it->second;
        }
        // Return an empty function if the script is not found
        return nullptr;
    }

    // === IScriptRegistrar Interface Implementation ===

    void ScriptingEngine::RegisterScript(const std::string& name, std::function<IScript* ()> factory) {
        try {
            ValidateScriptName(name);

            if (m_scriptFactories.find(name) != m_scriptFactories.end()) {
                std::cerr << "Warning: Script '" << name << "' is already registered. Overwriting..." << std::endl;
            }

            m_scriptFactories[name] = factory;
            std::cout << "Registered script: " << name << std::endl;

        }
        catch (const std::exception& e) {
            SetLastError("Failed to register script '" + name + "': " + e.what());
            throw;
        }
    }

    bool ScriptingEngine::IsScriptRegistered(const std::string& name) const {
        return m_scriptFactories.find(name) != m_scriptFactories.end();
    }

    size_t ScriptingEngine::GetRegisteredScriptCount() const {
        return m_scriptFactories.size();
    }

    // === Script Management ===

    std::unique_ptr<IScript> ScriptingEngine::CreateScript(const std::string& name) const {
        auto it = m_scriptFactories.find(name);
        if (it == m_scriptFactories.end()) {
            const_cast<ScriptingEngine*>(this)->SetLastError("Script '" + name + "' is not registered");
            return nullptr;
        }

        try {
            IScript* rawScript = it->second();
            return std::unique_ptr<IScript>(rawScript);
        }
        catch (const std::exception& e) {
            const_cast<ScriptingEngine*>(this)->SetLastError("Error creating script '" + name + "': " + e.what());
            return nullptr;
        }
    }

    std::vector<std::string> ScriptingEngine::GetRegisteredScriptNames() const {
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

            std::cout << "Scanning directory for DLLs: " << directory << std::endl;

            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                    std::string dllPath = entry.path().string();
                    std::cout << "Found DLL: " << dllPath << std::endl;

                    if (LoadGameDLL(dllPath)) {
                        loadedCount++;
                    }
                    else {
                        std::cerr << "Failed to load DLL: " << dllPath << " - " << GetLastError() << std::endl;
                    }
                }
            }

            std::cout << "Successfully loaded " << loadedCount << " DLLs from directory: " << directory << std::endl;

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
        // Note: This is a simplified approach. In a more sophisticated system,
        // you might want to track which DLL registered which scripts.
        std::cout << "Unloading DLL: " << dllName << " (scripts will remain registered)" << std::endl;

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
        std::cout << "Unloading all DLLs..." << std::endl;

        for (auto& dll : m_loadedDLLs) {
            UnloadSingleDLL(dll);
        }

        m_loadedDLLs.clear();
        m_scriptFactories.clear(); // Clear all registered scripts

        std::cout << "All DLLs unloaded and scripts cleared." << std::endl;
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
            std::cout << "ScriptingEngine already initialized." << std::endl;
            return;
        }

        std::cout << "Initializing ScriptingEngine..." << std::endl;
        std::cout << "  - " << m_loadedDLLs.size() << " DLLs loaded" << std::endl;
        std::cout << "  - " << m_scriptFactories.size() << " scripts registered" << std::endl;

        PrintSummary();

        m_initialized = true;
        std::cout << "ScriptingEngine initialization complete." << std::endl;
    }

    void ScriptingEngine::Shutdown() {
        if (!m_initialized) {
            return;
        }

        std::cout << "Shutting down ScriptingEngine..." << std::endl;

        UnloadAllDLLs();

        m_initialized = false;
        std::cout << "ScriptingEngine shutdown complete." << std::endl;
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
        std::cout << "\n=== Scripting Engine Summary ===" << std::endl;

        // Print loaded DLLs
        std::cout << "Loaded DLLs (" << m_loadedDLLs.size() << "):" << std::endl;
        if (m_loadedDLLs.empty()) {
            std::cout << "  (none)" << std::endl;
        }
        else {
            for (const auto& dll : m_loadedDLLs) {
                std::cout << "  - " << dll.name << " (" << dll.filepath << ")" << std::endl;
            }
        }

        // Print registered scripts
        auto scriptNames = GetRegisteredScriptNames();
        std::cout << "Registered Scripts (" << scriptNames.size() << "):" << std::endl;
        if (scriptNames.empty()) {
            std::cout << "  (none)" << std::endl;
        }
        else {
            for (const auto& name : scriptNames) {
                std::cout << "  - " << name << std::endl;
            }
        }
        std::cout << "================================\n" << std::endl;
    }

    bool ScriptingEngine::ReloadDLL(const std::string& dllPath) {
        std::string dllName = GetDLLName(dllPath);

        std::cout << "Reloading DLL: " << dllName << std::endl;

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
        std::cout << "Clearing all registered scripts..." << std::endl;
        m_scriptFactories.clear();
        std::cout << "All scripts cleared." << std::endl;
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
        std::cerr << "ScriptingEngine Error: " << error << std::endl;
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

            // Call the registration function
            registerFunc(this);

            // Store the loaded DLL information
            m_loadedDLLs.emplace_back(dllHandle, dllPath, dllName, registerFunc);

            std::cout << "Successfully loaded and registered scripts from: " << dllPath << std::endl;
            return true;

        }
        catch (const std::exception& e) {
            SetLastError("Exception during script registration: " + std::string(e.what()));
            FreeLibrary(dllHandle);
            return false;
        }
        catch (...) {
            SetLastError("Unknown exception during script registration");
            FreeLibrary(dllHandle);
            return false;
        }
    }

    bool ScriptingEngine::UnloadSingleDLL(LoadedDLL& dll) {
        if (FreeLibrary(dll.handle)) {
            std::cout << "Unloaded DLL: " << dll.name << std::endl;
            return true;
        }
        else {
            SetLastError("Failed to unload DLL: " + dll.name + " - " + GetSystemError());
            return false;
        }
    }

} // namespace NE::Scripting