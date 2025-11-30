// GameEntry.cpp

// Include the SDK from Engine DLL
#include "pch.h"
#include <ScriptSDK/ScriptAPI.h>


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
#include "Scripts/Pickable.hpp"
#include "Scripts/PlayerController.hpp"
// Component Reference Example Scripts
#include "Scripts/FollowerScript.hpp"
#include "Scripts/TweenExampleScript.hpp"
#include "Scripts/ParentControllerScript.hpp"
#include "Scripts/MaterialSequencer.hpp"
#include "Scripts/PrefabSpawnerScript.hpp"
#include "Scripts/BulletShooterScript.hpp"
#include "Scripts/CameraController.hpp"
#include "Scripts/RenderSettingsDemo.hpp"



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
        registrar->RegisterScript("PlayerScript", []() -> NE::Scripting::IScript* {
            return new PlayerScript();
            });

        registrar->RegisterScript("TestScript", []() -> NE::Scripting::IScript* {
            return new TestScript();
            });

        // Register the new physics-based player controller
        registrar->RegisterScript("PhysicsPlayerController", []() -> NE::Scripting::IScript* {
            return new PhysicsPlayerController();
            });

        registrar->RegisterScript("TextureSwitch", []() -> NE::Scripting::IScript* {
            return new TextureSwitch();
            });

        registrar->RegisterScript("LightSwitch", []() -> NE::Scripting::IScript* {
            return new LightSwitch();
            });

        registrar->RegisterScript("PlayerCamera", []() -> NE::Scripting::IScript* {
            return new PlayerCamera();
            });

        registrar->RegisterScript("Gears", []() -> NE::Scripting::IScript* {
            return new Gears();
            });

        registrar->RegisterScript("k1bswitch", []() -> NE::Scripting::IScript* {
            return new k1bswitch();
            });

        registrar->RegisterScript("k2bswitch", []() -> NE::Scripting::IScript* {
            return new k2bswitch();
            });

        // Component Reference Example Scripts
        registrar->RegisterScript("FollowerScript", []() -> NE::Scripting::IScript* {
            return new FollowerScript();
            });

        registrar->RegisterScript("PlayerController", []() -> NE::Scripting::IScript* {
            return new PlayerController();
            });

        registrar->RegisterScript("TweenExampleScript", []() -> NE::Scripting::IScript* {
            return new TweenExampleScript();
            });

        registrar->RegisterScript("ParentControllerScript", []() -> NE::Scripting::IScript* {
            return new ParentControllerScript();
            });

        registrar->RegisterScript("MaterialSequencer", []() -> NE::Scripting::IScript* {
            return new MaterialSequencer();
            });

        registrar->RegisterScript("RenderSettingsDemo",
            []() -> NE::Scripting::IScript* { return new RenderSettingsDemo(); });


        registrar->RegisterScript("PrefabSpawnerScript", []() -> NE::Scripting::IScript* {
            return new PrefabSpawnerScript();
            });

        registrar->RegisterScript("BulletShooterScript", []() -> NE::Scripting::IScript* {
            return new BulletShooterScript();
            });

        registrar->RegisterScript("CameraController", []() -> NE::Scripting::IScript* {
            return new CameraController();
            });

        registrar->RegisterScript("Pickable", []() -> NE::Scripting::IScript* {
            return new Pickable();
            });

        }
    
}
