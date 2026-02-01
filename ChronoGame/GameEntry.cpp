#pragma once
// GameEntry.cpp

// Include the SDK from Engine DLL
#include "pch.h"
#include <ScriptSDK/ScriptAPI.h>

// Include headers for all scripts you want to register
#include "Scripts/Highlightable_Material.hpp"
#include "Scripts/Player_Controller.hpp"
#include "Scripts/Player_Raycast.hpp"
#include "Scripts/Watch_Controller.hpp"
#include "Scripts/Puzzle_Wire.hpp"
#include "Scripts/Puzzle_Mirror.hpp"
#include "Scripts/Puzzle_Lever.hpp"
#include "Scripts/Interactable_WireButton.hpp"
#include "Scripts/Interactable_Grabbable.hpp"
#include "Scripts/Interactable_OneWaySwitch.hpp"
#include "Scripts/Interactable_TwoWaySwitch.hpp"
#include "Scripts/Misc_Manager.hpp"
#include "Scripts/Misc_WireChild.hpp"
#include "Scripts/Misc_Grabber.hpp"
#include "Scripts/Misc_ICOSwitcher.hpp"
#include "Scripts/Misc_TwoStateRotater.hpp"
#include "Scripts/Misc_Sinkhole.hpp"
#include "Scripts/Misc_MaterialSwitcher.hpp"
#include "Scripts/Misc_PLayerRespawn.hpp"
#include "Scripts/Misc_PlayerRespawnTest.hpp"
#include "Scripts/Interactable_NoteCollector.hpp"
#include "Scripts/NoteCollector_Controller.hpp"

// extern "C" ensures C linkage so the Engine DLL can find this function
extern "C" {
    // Export this function so it can be called from the Engine DLL
    __declspec(dllexport)
        void RegisterEngineScripts(NE::Scripting::IScriptRegistrar* registrar) {

        // Validate that we received a valid registrar
        if (!registrar) {
            return;
        }

        registrar->RegisterScript("Interactable_", []() -> NE::Scripting::IScript* {
            return new Interactable_();
            });
    }
}
