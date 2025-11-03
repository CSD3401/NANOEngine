// GameEntry.cpp

// Include the registrar interface from Engine DLL
#include "pch.h"
#include "Scripting/IScriptRegistrar.hpp"


// Include the base script interface
// Note: If IScript is defined in Engine DLL, include it from there
// #include "Core/IScript.hpp"

// Include headers for all scripts you want to register
#include "Scripts/PlayerScript.hpp"
#include "Scripts/TestScript.hpp"
#include "Scripts/TextureSwitch.hpp"

// extern "C" ensures C linkage so the Engine DLL can find this function
extern "C" {
    // Export this function so it can be called from the Engine DLL
    __declspec(dllexport)
        void RegisterEngineScripts(NE::Scripting::IScriptRegistrar* registrar) {

        // Validate that we received a valid registrar
        if (!registrar) {
            return;
        }

      
        // Register all your game-specific scripts here
        registrar->RegisterScript("PlayerScript", []() -> IScript* {
            return new PlayerScript();
            });

        registrar->RegisterScript("TestScript", []() -> IScript* {
            return new TestScript();
            });

        registrar->RegisterScript("TextureSwitch", []() -> IScript* {
            return new TextureSwitch();
            });
       
    }
}