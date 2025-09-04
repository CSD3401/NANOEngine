#include "ScriptingEngine.hpp"
#include <iostream>

// Platform-specific includes for dynamic library loading.
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif



namespace NE::Scripting {

    // Define the signature of the function we expect to find in the DLL.
    // This function will register all scripts from the DLL with our engine.
    using RegisterScriptsFunc = void(*)(ScriptingEngine*);

    ScriptingEngine::ScriptingEngine() {}

    ScriptingEngine::~ScriptingEngine() {
        UnloadGameCode();
    }

    void ScriptingEngine::LoadGameCode(const std::string& dllPath) {
        if (m_GameCodeDLL) {
            UnloadGameCode();
        }

#ifdef _WIN32
        m_GameCodeDLL = LoadLibraryA(dllPath.c_str());
#else
        m_GameCodeDLL = dlopen(dllPath.c_str(), RTLD_LAZY);
#endif

        if (!m_GameCodeDLL) {
            std::cerr << "Error: Could not load game code DLL: " << dllPath << std::endl;
            return;
        }

        // 2. Find the registration function in the DLL.
        RegisterScriptsFunc registerFunc = nullptr;
#ifdef _WIN32
        registerFunc = (RegisterScriptsFunc)GetProcAddress(m_GameCodeDLL, "RegisterEngineScripts");
#else
        registerFunc = (RegisterScriptsFunc)dlsym(m_GameCodeDLL, "RegisterEngineScripts");
#endif

        if (!registerFunc) {
            std::cerr << "Error: Could not find 'RegisterEngineScripts' function in DLL." << std::endl;
            UnloadGameCode();
            return;
        }

        // 3. Call the function, passing a pointer to this engine instance.
        // The DLL will then call back to our RegisterScript method for each script.
        registerFunc(this);
        std::cout << "Successfully loaded and registered scripts from: " << dllPath << std::endl;
    }

    void ScriptingEngine::UnloadGameCode() {
        if (!m_GameCodeDLL) return;

        m_ScriptFactories.clear();
#ifdef _WIN32
        FreeLibrary(m_GameCodeDLL);
#else
        dlclose(m_GameCodeDLL);
#endif
        m_GameCodeDLL = nullptr;
        std::cout << "Game code DLL unloaded." << std::endl;
    }

    bool ScriptingEngine::HasScript(const std::string& scriptName) {
        return m_ScriptFactories.count(scriptName);
    }

    void ScriptingEngine::BindComponent(NE::ECS::Component::NativeScriptComponent& component) {
        if (HasScript(component.ScriptName)) {
            auto& factory = m_ScriptFactories.at(component.ScriptName);
            component.CreateScript = factory;
            component.DestroyScript = [](IScript* script) { delete script; };
        }
        else {
            std::cerr << "Warning: Script '" << component.ScriptName << "' not found." << std::endl;
            component.CreateScript = nullptr;
            component.DestroyScript = nullptr;
        }
    }

    // This method is called BY the DLL to register a script factory.
    void ScriptingEngine::RegisterScript(const std::string& name, CreateScriptFunc func) {
        if (m_ScriptFactories.count(name)) {
            std::cout << "Warning: Script '" << name << "' is already registered. Overwriting." << std::endl;
        }
        m_ScriptFactories[name] = func;
        std::cout << "Registered script: " << name << std::endl;
    }

}
