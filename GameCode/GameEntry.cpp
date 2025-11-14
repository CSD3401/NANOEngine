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
#include "Scripts/PhysicsPlayerController.hpp"
#include "Scripts/TextureSwitch.hpp"
#include "Scripts/LightSwitch.hpp"
#include "Scripts/PlayerCamera.hpp"
#include "Scripts/Gears.hpp"
#include "Scripts/k1bswitch.hpp"
#include "Scripts/k2bswitch.hpp"

// Component Reference Example Scripts
#include "Scripts/FollowerScript.hpp"
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

        // Register the new physics-based player controller
        registrar->RegisterScript("PhysicsPlayerController", []() -> IScript* {
            return new PhysicsPlayerController();
            });

        registrar->RegisterScript("TextureSwitch", []() -> IScript* {
            return new TextureSwitch();
            });

        registrar->RegisterScript("LightSwitch", []() -> IScript* {
            return new LightSwitch();
            });

        registrar->RegisterScript("PlayerCamera", []() -> IScript* {
            return new PlayerCamera();
            });

        registrar->RegisterScript("Gears", []() -> IScript* {
            return new Gears();
            });

        registrar->RegisterScript("k1bswitch", []() -> IScript* {
            return new k1bswitch();
            });

        registrar->RegisterScript("k2bswitch", []() -> IScript* {
            return new k2bswitch();
     });

        // Component Reference Example Scripts
          registrar->RegisterScript("FollowerScript", []() -> IScript* {
          return new FollowerScript();
     });

    }
}