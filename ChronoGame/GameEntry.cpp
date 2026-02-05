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
#include "Scripts/NoteCollector_Controller.hpp"
#include "Scripts/Puzzle_Wire.hpp"
#include "Scripts/Puzzle_Mirror.hpp"
#include "Scripts/Puzzle_Lever.hpp"
#include "Scripts/Interactable_WireButton.hpp"
#include "Scripts/Interactable_WireTether.hpp"
#include "Scripts/Interactable_Grabbable.hpp"
#include "Scripts/Interactable_OneWaySwitch.hpp"
#include "Scripts/Interactable_TwoWaySwitch.hpp"
#include "Scripts/Interactable_NoteCollector.hpp"
#include "Scripts/Interactable_Gate.hpp"
#include "Scripts/Interactable_DoorHinge.hpp"
#include "Scripts/Misc_Manager.hpp"
#include "Scripts/Misc_WireChild.hpp"
#include "Scripts/Misc_Grabber.hpp"
#include "Scripts/Misc_ICOSwitcher.hpp"
#include "Scripts/Misc_TwoStateRotater.hpp"
#include "Scripts/Misc_Sinkhole.hpp"
#include "Scripts/Misc_MaterialSwitcher.hpp"
#include "Scripts/Misc_PlayerRespawn.hpp"
#include "Scripts/Misc_RespawnOnCollision.hpp"
#include "Scripts/Listener_MoveObject.hpp"
#include "Scripts/Listener_StretchObject.hpp"
#include "Scripts/Interactable_SequencerPad.hpp"
#include "Scripts/Puzzle_MultiLightSequencer.hpp"
#include "Scripts/LaserListener.hpp"
#include "Scripts/IntersectionListerner.hpp"
#include "Scripts/Misc_Teleporter.hpp"

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
        registrar->RegisterScript("Highlightable_Material", []() -> NE::Scripting::IScript* {
            return new Highlightable_Material();
            });
        registrar->RegisterScript("Interactable_", []() -> NE::Scripting::IScript* {
            return new Interactable_();
            });
        registrar->RegisterScript("Player_Controller", []() -> NE::Scripting::IScript* {
            return new Player_Controller();
            });
        registrar->RegisterScript("Player_Raycast", []() -> NE::Scripting::IScript* {
            return new Player_Raycast();
            });
        registrar->RegisterScript("Watch_Controller", []() -> NE::Scripting::IScript* {
            return new Watch_Controller();
            });
        registrar->RegisterScript("Misc_Manager", []() -> NE::Scripting::IScript* {
            return new Misc_Manager();
            });
        registrar->RegisterScript("Puzzle_Wire", []() -> NE::Scripting::IScript* {
            return new Puzzle_Wire();
            });
        registrar->RegisterScript("Puzzle_Lever", []() -> NE::Scripting::IScript* {
            return new Puzzle_Lever();
            });
        registrar->RegisterScript("Misc_Manager", []() -> NE::Scripting::IScript* {
            return new Misc_Manager();
            });
        registrar->RegisterScript("Puzzle_Mirror", []() -> NE::Scripting::IScript* {
            return new MirrorPuzzle();
            });
        registrar->RegisterScript("Misc_WireChild", []() -> NE::Scripting::IScript* {
            return new Misc_WireChild();
            });
        registrar->RegisterScript("Interactable_WireButton", []() -> NE::Scripting::IScript* {
            return new Interactable_WireButton();
            });
        registrar->RegisterScript("Interactable_WireTether", []() -> NE::Scripting::IScript* {
            return new Interactable_WireTether();
            });
        registrar->RegisterScript("Interactable_Grabbable", []() -> NE::Scripting::IScript* {
            return new Interactable_Grabbable();
            });
        registrar->RegisterScript("Misc_Grabber", []() -> NE::Scripting::IScript* {
            return new Misc_Grabber();
            });
        registrar->RegisterScript("Misc_ICOSwitcher", []() -> NE::Scripting::IScript* {
            return new Misc_ICOSwitcher();
            });
        registrar->RegisterScript("Misc_TwoStateRotater", []() -> NE::Scripting::IScript* {
            return new Misc_TwoStateRotater();
            });
        registrar->RegisterScript("Interactable_OneWaySwitch", []() -> NE::Scripting::IScript* {
            return new Interactable_OneWaySwitch();
            });
        registrar->RegisterScript("Interactable_TwoWaySwitch", []() -> NE::Scripting::IScript* {
            return new Interactable_TwoWaySwitch();
            });
        registrar->RegisterScript("Interactable_NoteCollector", []() -> NE::Scripting::IScript* {
            return new Interactable_NoteCollector();
            });
        registrar->RegisterScript("Interactable_Gate", []() -> NE::Scripting::IScript* {
            return new Interactable_Gate();
            });
        registrar->RegisterScript("Interactable_DoorHinge", []() -> NE::Scripting::IScript* {
            return new Interactable_DoorHinge();
            });
        registrar->RegisterScript("NoteCollector_Controller", []() -> NE::Scripting::IScript* {
            return new NoteCollector_Controller();
            });
        registrar->RegisterScript("Misc_Sinkhole", []() -> NE::Scripting::IScript* {
            return new Misc_Sinkhole();
            });
        registrar->RegisterScript("Misc_MaterialSwitcher", []() -> NE::Scripting::IScript* {
            return new Misc_MaterialSwitcher();
            });
        registrar->RegisterScript("Misc_PlayerRespawn", []() -> NE::Scripting::IScript* {
            return new Misc_PlayerRespawn();
            });
        registrar->RegisterScript("Misc_RespawnOnCollision", []() -> NE::Scripting::IScript* {
            return new Misc_RespawnOnCollision();
            });
        registrar->RegisterScript("Listener_MoveObject", []() -> NE::Scripting::IScript* {
            return new Listener_MoveObject();
            });
        registrar->RegisterScript("Listener_StretchObject", []() -> NE::Scripting::IScript* {
            return new Listener_StretchObject();
            });
        registrar->RegisterScript("Interactable_SequencerPad", []() -> NE::Scripting::IScript* {
            return new Interactable_SequencerPad();
            });
        registrar->RegisterScript("Puzzle_MultiLightSequencer", []() -> NE::Scripting::IScript* {
            return new Puzzle_MultiLightSequencer();
            });
        registrar->RegisterScript("LaserListener", []() -> NE::Scripting::IScript* {
            return new LaserListener();
            });
        registrar->RegisterScript("IntersectionListener", []() -> NE::Scripting::IScript* {
            return new IntersectionListener();
            });
        registrar->RegisterScript("Misc_Teleporter", []() -> NE::Scripting::IScript* {
            return new Misc_Teleporter();
            });
        }
}
