#pragma once
#include "IScript.hpp"
#include "../ECS/Components/NativeScript.hpp"
#include <string>
#include <functional>
#include <map>
#include <memory>

// Define a platform-agnostic handle for the dynamic library.
#ifdef _WIN32
#include <windows.h>
#define DLL_HANDLE HMODULE
#else
#define DLL_HANDLE void*
#endif


namespace NE::Scripting
{

    // This class is the isolated module responsible for loading and managing
    // the game code DLL. This is the class you will replace if you upgrade to JIT.
    class ScriptingEngine {
    public:
        ScriptingEngine();
        ~ScriptingEngine();

        void LoadGameCode(const std::string& dllPath);
        void UnloadGameCode();

        bool HasScript(const std::string& scriptName);
        void BindComponent(NE::ECS::Component::NativeScriptComponent& component);

    private:
        DLL_HANDLE m_GameCodeDLL = nullptr;

        // Store a map of script names to their factory functions.
        using CreateScriptFunc = std::function<IScript* ()>;
        std::map<std::string, CreateScriptFunc> m_ScriptFactories;

        // The DLL will call this function to register its scripts with the engine.
        void RegisterScript(const std::string& name, CreateScriptFunc func);

        // This is how the DLL communicates back to the engine. We make the DLL's
        // entry function a friend so it can access the private RegisterScript.
        friend void RegisterEngineScripts(ScriptingEngine*);
    };
}




