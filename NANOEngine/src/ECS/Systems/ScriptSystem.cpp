#include "ScriptSystem.hpp" 
#include <iostream>
#include <filesystem>
#include "../Components/NativeScript.hpp"
#include "Core/SpdLogger.hpp"

namespace NE::ECS::Systems {

    // --- Helper function to get the temporary DLL path ---
    std::string GetHotReloadPath(const std::string& originalPath, int version) {
        std::filesystem::path path(originalPath);
        std::string stem = path.stem().string();
        std::string ext = path.extension().string();
        std::string newFilename = stem + "_hot_" + std::to_string(version) + ext;
        return path.replace_filename(newFilename).string();
    }

    struct ScriptState {
        std::string scriptName;
        bool isEnabled;
        std::unordered_map<std::string, std::string> fields;
    };

    ScriptSystem::ScriptSystem(ComponentManager* cm)
        : m_componentManager(cm) {
    }

    //ScriptSystem::~ScriptSystem() {
    //    m_sourceWatcher.reset();

    //    // --- Clean up the last loaded DLL ---
    //    // Ensure we don't leave temp files.
    //    try {
    //        if (!m_currentLoadedDLLPath.empty() && std::filesystem::exists(m_currentLoadedDLLPath)) {
    //            std::filesystem::path dllPath(m_currentLoadedDLLPath);
    //            std::filesystem::path pdbPath = dllPath.replace_extension(".pdb");

    //            std::filesystem::remove(dllPath);
    //            if (std::filesystem::exists(pdbPath)) {
    //                std::filesystem::remove(pdbPath);
    //            }
    //        }

    //        SPD_INFO("Constructor deletes.");
    //    }
    //    catch (const std::exception& e) {
    //        SPD_WARNING("Could not clean up temp DLL: " << e.what());
    //    }
    //}

    void ScriptSystem::OnEntityAdded(Entity entity) {
        // Logic for when an entity relevant to the script system is added
        auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
        if (nsc.CreateScript && !nsc.Instance) {
            nsc.Instance = nsc.CreateScript();
            nsc.Instance->LinkToEngine(m_componentManager); // Link to engine systems
            nsc.Instance->SetEntity(entity);
            nsc.Instance->Initialize(entity);
            nsc.Instance->SetEnabled(false); // Start disabled
            SPD_INFO("Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity);
        }
    }

    void ScriptSystem::OnEntityRemoved(Entity entity) {
        // should probably call OnScriptComponentDestroyed here if the entity
        // has a script component, to ensure proper cleanup.
        (void)entity;
    }

    void ScriptSystem::Init() {
        SPD_INFO("ScriptSystem::Init() - Starting initialization");
        
        // Logic to initialize the system when a scene loads
        scriptingEngine = std::make_unique<Scripting::ScriptingEngine>();

        // DLL Path
        m_scriptDLLPath = "GameCode.dll";
        m_scriptDLLPath = std::filesystem::absolute(m_scriptDLLPath).string();
        SPD_INFO("Loading script DLL: " << m_scriptDLLPath);

        // Source Directory
        m_scriptSourceDirectory = "../../../GameCode/Scripts/";
        m_scriptSourceDirectory = std::filesystem::absolute(m_scriptSourceDirectory).string();

        // Build Command
        /*const char* vsPath = std::getenv("VSINSTALLDIR");
        if (vsPath)
        {*/
            std::string msbuildPath = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe";
            m_scriptBuildCommand =
                "cmd /C \"\"" + msbuildPath + "\" "
                "\"../../../NANOEngine.sln\" "
                "/t:GameCode "
                "/p:Configuration=Release /p:Platform=x64\"";

            std::cout << "Command: " << m_scriptBuildCommand << std::endl;
        //}

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
            if (!scriptingEngine->LoadGameDLL(m_currentLoadedDLLPath)) {
                SPD_ERROR("Failed to load script DLL copy: " << m_currentLoadedDLLPath);
                SPD_ERROR("Last Error: " << scriptingEngine->GetLastError());
            }
            else {
                SPD_INFO("Successfully loaded DLL.");
                scriptingEngine->PrintSummary();
            }

        }
        catch (const std::filesystem::filesystem_error& e) {
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
        }
        catch (const std::exception& e) {
            SPD_ERROR("Failed to create file watcher for " << m_scriptSourceDirectory << ": " << e.what());
            SPD_WARNING("Hot-compile will be disabled.");
            m_sourceWatcher.reset();
        }

        SPD_INFO("ScriptSystem::Init() - Completed");
    }

    void ScriptSystem::Exit() {
        // Logic to clean up the system when a scene unloads

        m_sourceWatcher.reset();
        SPD_INFO("File watcher stopped.");

		for (Entity entity : m_componentManager->GetEntitiesWithComponent<Component::NativeScript>()) {
			OnScriptComponentDestroyed(entity);
		}

        SPD_INFO("Script system shutdown complete");
        scriptingEngine->Shutdown(); //tmp
    }

    void ScriptSystem::StartScripts()
    {
        SPD_INFO("ScriptSystem: Entering Play Mode...");
        const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			if (nsc.Instance)
				nsc.Instance->SetEnabled(true);
        }
    }

    void ScriptSystem::PauseScripts()
    {
		SPD_INFO("ScriptSystem: Pausing Play Mode...");
		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			if (nsc.Instance) {
				nsc.Instance->SetEnabled(false);
				SPD_INFO("Paused script '" << nsc.ScriptName << "' for entity " << (int)entity);
			}
		}
    }

    void ScriptSystem::StopScripts()
    {
        SPD_INFO("ScriptSystem: Exiting Play Mode...");
        const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

            if (nsc.Instance) {
                nsc.Instance->OnDestroy();
                if (nsc.DestroyScript) {
                    nsc.DestroyScript(nsc.Instance);
                }
                else {
                    delete nsc.Instance; // Fallback
                }
                // CRITICAL: Reset the instance pointer to null
                nsc.Instance = nullptr;
                SPD_INFO("Destroyed script '" << nsc.ScriptName << "' for entity " << (int)entity);
            }

			// Recreate the script instance for the next play session
            if (nsc.CreateScript && !nsc.Instance) {
                nsc.Instance = nsc.CreateScript();
                nsc.Instance->LinkToEngine(m_componentManager); // Link to engine systems
                nsc.Instance->SetEntity(entity);
                nsc.Instance->Initialize(entity);
				nsc.Instance->SetEnabled(false); // Start disabled
                SPD_INFO("Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity);
            }
        }
    }
    

    void ScriptSystem::Update(double deltaTime) {

        // --- Check for compile request ---
        if (m_compileQueued.load()) {
            m_compileQueued.store(false); // Consume the flag
            HotCompileAndReload(); // Run the compile and reload
        }

        const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

        // Second loop: Update all active scripts
        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
            if (nsc.Instance && nsc.Instance->IsEnabled()) {
                nsc.Instance->Update(deltaTime);
            }
        }
    }

    void ScriptSystem::OnScriptComponentDestroyed(Entity entity) {
        auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
        if (nsc.Instance) {
            nsc.Instance->OnDestroy();
            if (nsc.DestroyScript) {
                nsc.DestroyScript(nsc.Instance);
            }
            else {
                delete nsc.Instance; // Fallback
            }
            nsc.Instance = nullptr;
            SPD_INFO("Destroyed script '" << nsc.ScriptName << "' for entity " << (int)entity);
        }
    }

    // --- Hot Compile & Reload Implementation ---

    void ScriptSystem::HandleFileWatchEvent(const std::string& path, const filewatch::Event eventType) {
        // This runs on the file watcher's thread
        if (eventType == filewatch::Event::modified || eventType == filewatch::Event::renamed_new || eventType == filewatch::Event::added) {

            SPD_INFO("FileWatch: Detected change in: " << path);

            // Just set the flag. The main thread will handle the rest.
            m_compileQueued.store(true);
        }
    }


    void ScriptSystem::HotCompileAndReload() {
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

        // Copy Files to New Temp Path 
        std::string newDLLPath = GetHotReloadPath(m_scriptDLLPath, m_hotReloadCounter);
        std::string oldDLLPath = m_currentLoadedDLLPath;

        m_hotReloadCounter++; // Increment *after* getting new path
        m_currentLoadedDLLPath = newDLLPath;

        try {
            std::filesystem::path originalDLL(m_scriptDLLPath);
            //std::filesystem::path originalPDB = originalDLL.replace_extension(".pdb");

            std::filesystem::path targetDLL(newDLLPath);
            //std::filesystem::path targetPDB = targetDLL.replace_extension(".pdb");

            // Copy the newly-built files to our new temp path
            std::filesystem::copy_file(originalDLL, targetDLL, std::filesystem::copy_options::overwrite_existing);
            /*if (std::filesystem::exists(originalPDB)) {
                std::filesystem::copy_file(originalPDB, targetPDB, std::filesystem::copy_options::overwrite_existing);
            }*/
            SPD_INFO("Copied new DLL to: " << newDLLPath);

        }
        catch (const std::filesystem::filesystem_error& e) {
            SPD_ERROR("Failed to copy new DLL for reload: " << e.what());
            m_currentLoadedDLLPath = oldDLLPath; // Revert path
            m_hotReloadCounter--; // Revert counter
            return;
        }

        // 3. Run Hot Reload on the *New* File 
        HotReloadDLL(oldDLLPath,newDLLPath); // This reloads using the new copy

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
        }
        catch (const std::exception& e) {
            SPD_WARNING("Could not clean up old DLL: " << oldDLLPath << " - " << e.what());
        }
    }

    void ScriptSystem::HotReloadDLL(const std::string& oldDllPath, const std::string& newDllPath) {
        // It handles state serialization, DLL swapping, and state restoration.

        std::string newDllName = std::filesystem::path(newDllPath).filename().string();
        std::string oldDllName = std::filesystem::path(oldDllPath).filename().string();
        SPD_INFO("--- BEGIN HOT RELOAD: " << oldDllName << " -> " << newDllName << " ---");

        // 1. --- Store State ---
        std::unordered_map<Entity, ScriptState> stateToRestore;

        auto entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();
        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
            if (!nsc.Instance) continue;

            SPD_INFO("Serializing state for entity " << (int)entity << " (Script: " << nsc.ScriptName << ")");

            ScriptState state;
            state.scriptName = nsc.ScriptName;
            state.isEnabled = nsc.Instance->IsEnabled();

            auto fieldNames = nsc.Instance->GetExposedFieldNames();
            for (const auto& fieldName : fieldNames) {
                state.fields[fieldName] = nsc.Instance->GetFieldValueAsString(fieldName);
            }

            stateToRestore[entity] = state;

            OnScriptComponentDestroyed(entity);
            
        }

        // 2. --- Reload the DLL ---

        if (!oldDllName.empty() && scriptingEngine->IsDLLLoaded(oldDllName)) {
            if (!scriptingEngine->UnloadDLL(oldDllName)) {
                SPD_ERROR("--- HOT RELOAD FAILED (Unload): Failed to unload old DLL: " << oldDllName << " ---");
                // This is bad, but we might as well try to load the new one anyway
                // so the user can at least try to recover without restarting.
            }
            else {
                SPD_INFO("Unloaded old DLL: " << oldDllName);
            }
        }

        bool loadSuccess = scriptingEngine->LoadGameDLL(newDllPath);
        if (!loadSuccess) {
            SPD_ERROR("--- HOT RELOAD FAILED (Load): " << scriptingEngine->GetLastError() << " ---");
            return; // Can't restore state if load failed
        }

        SPD_INFO("DLL reloaded. Restoring script states...");
        scriptingEngine->PrintSummary();

        // 3. --- Restore State ---
        for (auto const& [entity, state] : stateToRestore) {
            if (!m_componentManager->HasComponent<Component::NativeScript>(entity)) {
                SPD_WARNING("Entity " << (int)entity << " no longer exists. Cannot restore script.");
                continue;
            }

            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

            nsc.CreateScript = scriptingEngine->GetScriptFactory(state.scriptName);
            nsc.DestroyScript = [](IScript* script) { delete script; };

            if (!nsc.CreateScript) {
                SPD_ERROR("Failed to find new script factory for '" << state.scriptName << "'. Cannot restore state.");
                continue;
            }

            nsc.Instance = nsc.CreateScript();
            nsc.Instance->LinkToEngine(m_componentManager);
            nsc.Instance->SetEntity(entity);
            nsc.Instance->Initialize(entity);

            SPD_INFO("Restoring state for entity " << (int)entity << " (Script: " << state.scriptName << ")");
            for (auto const& [fieldName, fieldValue] : state.fields) {
                if (!nsc.Instance->SetFieldValueFromString(fieldName, fieldValue)) {
                    SPD_WARNING("Failed to set field '" << fieldName << "' to value '" << fieldValue << "'");
                }
            }

            nsc.Instance->SetEnabled(state.isEnabled);
        }

        SPD_INFO("--- END HOT RELOAD ---");
    }
}




