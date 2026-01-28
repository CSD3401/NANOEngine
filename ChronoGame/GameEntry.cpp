#pragma once
// GameEntry.cpp

// Include the SDK from Engine DLL
#include "pch.h"
#include <ScriptSDK/ScriptAPI.h>

// Include headers for all scripts you want to register
#include "Scripts/Interactable_.hpp"
#include "Scripts/Player_Controller.hpp"
#include "Scripts/Manager_.hpp"
#include "Scripts/Puzzle_Wire.hpp"
#include "Scripts/WireChild.hpp"
#include "Scripts/Interactable_WireButton.hpp"
#include "Scripts/Grabbable.hpp"
#include "Scripts/Grabber.hpp"
#include "Scripts/Miscellaneous_ICOSwitcher.hpp"
#include "Scripts/Puzzle_TwoStateRotater.hpp"
#include "Scripts/Puzzle_OneWaySwitch.hpp"
#include "Scripts/Puzzle_Sinkhole.hpp"
#include "Scripts/Example_EnumField.hpp"

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
        registrar->RegisterScript("Interactable_", []() -> NE::Scripting::IScript* {
            return new Interactable_();
            });
        registrar->RegisterScript("Player_Controller", []() -> NE::Scripting::IScript* {
            return new Player_Controller();
            });
        registrar->RegisterScript("Manager_", []() -> NE::Scripting::IScript* {
            return new Manager_();
            });
        registrar->RegisterScript("Puzzle_Wire", []() -> NE::Scripting::IScript* {
            return new Puzzle_Wire();
            });
        registrar->RegisterScript("WireChild", []() -> NE::Scripting::IScript* {
            return new WireChild();
            });
        registrar->RegisterScript("Interactable_WireButton", []() -> NE::Scripting::IScript* {
            return new Interactable_WireButton();
            });
        registrar->RegisterScript("Grabbable", []() -> NE::Scripting::IScript* {
            return new Grabbable();
            });
        registrar->RegisterScript("Grabber", []() -> NE::Scripting::IScript* {
            return new Grabber();
            });
        registrar->RegisterScript("Miscellaneous_ICOSwitcher", []() -> NE::Scripting::IScript* {
            return new Miscellaneous_ICOSwitcher();
            });
        registrar->RegisterScript("Puzzle_TwoStateRotater", []() -> NE::Scripting::IScript* {
            return new Puzzle_TwoStateRotater();
            });
        registrar->RegisterScript("Puzzle_OneWaySwitch", []() -> NE::Scripting::IScript* {
            return new Puzzle_OneWaySwitch();
            });
        registrar->RegisterScript("Puzzle_Sinkhole", []() -> NE::Scripting::IScript* {
            return new Puzzle_Sinkhole();
            });
        registrar->RegisterScript("Example_EnumField", []() -> NE::Scripting::IScript* {
            return new Example_EnumField();
            });
    }
}
