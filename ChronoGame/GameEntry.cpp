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
#include "Scripts/Puzzle_BatteryPanel.hpp"
#include "Scripts/Interactable_WireButton.hpp"
#include "Scripts/Interactable_WireTether.hpp"
#include "Scripts/Interactable_Grabbable.hpp"
#include "Scripts/Interactable_OneWaySwitch.hpp"
#include "Scripts/Interactable_TwoWaySwitch.hpp"
#include "Scripts/Interactable_NoteCollector.hpp"
#include "Scripts/Interactable_Gate.hpp"
#include "Scripts/Interactable_DoorHinge.hpp"
#include "Scripts/Interactable_Battery.hpp"
#include "Scripts/Misc_Manager.hpp"
#include "Scripts/Misc_WireChild.hpp"
#include "Scripts/Misc_Grabber.hpp"
#include "Scripts/Misc_ICOSwitcher.hpp"
#include "Scripts/Misc_TwoStateRotater.hpp"
#include "Scripts/Misc_Sinkhole.hpp"
#include "Scripts/Misc_MaterialSwitcher.hpp"
#include "Scripts/Misc_PlayerRespawn.hpp"
#include "Scripts/Misc_RespawnOnCollision.hpp"
#include "Scripts/Misc_WallToggle.hpp"
#include "Scripts/Listener_MoveObject.hpp"
#include "Scripts/Listener_StretchObject.hpp"
#include "Scripts/Interactable_SequencerPad.hpp"
#include "Scripts/Puzzle_MultiLightSequencer.hpp"
#include "Scripts/LaserListener.hpp"
#include "Scripts/IntersectionListerner.hpp"
#include "Scripts/Misc_Teleporter.hpp"
#include "Scripts/Misc_TransitionTeleporter.hpp"
#include "Scripts/UIButton_SwitchSceneThree.hpp"
#include "Scripts/UIButton_SwitchSceneTwo.hpp"
#include "Scripts/UIButton_SwitchScene.hpp"
#include "Scripts/UI_MasterVolumeButtons.hpp"
#include "Scripts/BackgroundAudio.hpp"
#include "Scripts/Camera_FOVPulse.hpp"
#include "Scripts/UI_Notes.hpp"
#include "Scripts/PhoneBooth.hpp"
#include "Scripts/Misc_TimeFogLighting.hpp"
#include "Scripts/Cutscene_Controller.hpp"
#include "Scripts/Dialouge_Marker.hpp"
#include "Scripts/Misc_TimeAnimationController.hpp"
#include "Scripts/Misc_TrolleyPush.hpp"
#include "Scripts/Interactable_TeleportToTop.hpp"
#include "Scripts/Misc_ExplosionCameraShakeOnCollision.hpp"
#include "Scripts/Camera_ExplosionShake.hpp"
#include "Scripts/UI_MainMenu.hpp"
#include "Scripts/UI_BGMVolumeButtons.hpp"
#include "Scripts/UI_GammaSlider.hpp"
#include "Scripts/UI_SFXVolumeButtons.hpp"
#include "Scripts/UI_AmbienceVolumeButtons.hpp"
#include "Scripts/Interactable_MirrorTile.hpp"
#include "Scripts/Misc_StopAnimationOnEvent.hpp"
#include "Scripts/Misc_SolvedMaterialOverride.hpp"
#include "Scripts/Puzzle_FinalBomb.hpp"
#include "Scripts/Puzzle_Bomb.hpp"
#include "Scripts/UIButton_Quit.hpp"
#include "Scripts/Credits_Controller.hpp"
#include "Scripts/Credits_ReturnToMainMenu.hpp"
#include "Scripts/UIButton_SwitchToCredits.hpp"
#include "Scripts/UIButton_ResetVolumes.hpp"
#include "Scripts/SplashScreen_Controller.hpp"
#include "Scripts/SplashScreen_ReturnToMainMenu.hpp"
#include "Scripts/Misc_WireBlinking.hpp"
#include "Scripts/Misc_ForcePastOnCollision.hpp"
#include "Scripts/Misc_ObjectUI.hpp"
#include "Scripts/Interactable_RotateLock.hpp"
#include "Scripts/Puzzle_RotateLock.hpp"
#include "Scripts/Tasks/TaskManager.hpp"
#include "Scripts/Tasks/TaskCheckpoint.hpp"
#include "Scripts/UI_HoverTextColor.hpp"
#include "Scripts/UI_OverlayFadeIn.hpp"
#include "Scripts/UI_ApplySavedSettings.hpp"
#include "Scripts/UI_RestorePauseSettings.hpp"
#include "Scripts/UI_Settings.hpp"
#include "Scripts/Misc_PauseMenuHotkey.hpp"
#include "Scripts/TriggerParentSwitcher.hpp"
#include "Scripts/TriggerMaterialVectorSwitcher.hpp"
#include "Scripts/UIButton_PauseResume.hpp"
#include "Scripts/UIButton_CanvasSwitcher.hpp"
#include "Scripts/UI_SaveSettings.hpp"
#include "Scripts/FinalAreaManager.hpp"

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
        registrar->RegisterScript("Misc_WallToggle", []() -> NE::Scripting::IScript* {
            return new Misc_WallToggle();
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
        registrar->RegisterScript("Misc_TransitionTeleporter", []() -> NE::Scripting::IScript* {
            return new Misc_TransitionTeleporter();
            });
        registrar->RegisterScript("UIButton_SwitchSceneThree", []() -> NE::Scripting::IScript* {
            return new UIButton_SwitchSceneThree();
            });
        registrar->RegisterScript("UIButton_SwitchScene", []() -> NE::Scripting::IScript* {
            return new UIButton_SwitchScene();
            });
        registrar->RegisterScript("UIButton_SwitchSceneTwo", []() -> NE::Scripting::IScript* {
            return new UIButton_SwitchSceneTwo();
            });
        registrar->RegisterScript("UI_MasterVolumeButtons", []() -> NE::Scripting::IScript* {
            return new UI_MasterVolumeButtons();
            });
        registrar->RegisterScript("BackgroundAudio", []() -> NE::Scripting::IScript* {
            return new BackgroundAudio();
            });
        registrar->RegisterScript("Camera_FOVPulse", []() -> NE::Scripting::IScript* {
            return new Camera_FOVPulse();
            });
        registrar->RegisterScript("UI_Notes", []() -> NE::Scripting::IScript* {
            return new UI_Notes();
            });
        registrar->RegisterScript("PhoneBooth", []() -> NE::Scripting::IScript* {
            return new PhoneBooth();
            });
        registrar->RegisterScript("Misc_TimeFogLighting", []() -> NE::Scripting::IScript* {
            return new Misc_TimeFogLighting();
            });
        registrar->RegisterScript("Puzzle_BatteryPanel", []() -> NE::Scripting::IScript* {
            return new Puzzle_BatteryPanel();
            });
        registrar->RegisterScript("Interactable_Battery", []() -> NE::Scripting::IScript* {
            return new Interactable_Battery();
            });
        registrar->RegisterScript("Cutscene_Controller", []() -> NE::Scripting::IScript* {
            return new Cutscene_Controller();
            });
        registrar->RegisterScript("Dialouge_Marker", []() -> NE::Scripting::IScript* {
            return new DialogueMarker();
            });

        registrar->RegisterScript("Misc_TimeAnimationController", []() -> NE::Scripting::IScript* {
            return new Misc_TimeAnimationController();
            });

        registrar->RegisterScript("Misc_TrolleyPush", []() -> NE::Scripting::IScript* {
            return new Misc_TrolleyPush();
            });

        registrar->RegisterScript("Interactable_TeleportToTop", []() -> NE::Scripting::IScript* {
            return new Interactable_TeleportToTop();
            });
        registrar->RegisterScript("Misc_ExplosionCameraShakeOnCollision", []() -> NE::Scripting::IScript* {
            return new Misc_ExplosionCameraShakeOnCollision();
            });
        registrar->RegisterScript("Camera_ExplosionShake", []() -> NE::Scripting::IScript* {
            return new Camera_ExplosionShake();
            });
        registrar->RegisterScript("UI_MainMenu", []() -> NE::Scripting::IScript* {
            return new UI_MainMenu();
            });
        registrar->RegisterScript("UI_Settings", []() -> NE::Scripting::IScript* {
            return new UI_MainMenu();
            });
        registrar->RegisterScript("UI_ApplySavedSettings", []() -> NE::Scripting::IScript* {
            return new UI_ApplySavedSettings();
            });
        registrar->RegisterScript("UI_RestorePauseSettings", []() -> NE::Scripting::IScript* {
            return new UI_RestorePauseSettings();
            });
        registrar->RegisterScript("UI_BGMVolumeButtons", []() -> NE::Scripting::IScript* {
            return new UI_BGMVolumeButtons();
            });
        registrar->RegisterScript("UI_GammaSlider", []() -> NE::Scripting::IScript* {
            return new UI_GammaSlider();
            });
        registrar->RegisterScript("UI_SFXVolumeButtons", []() -> NE::Scripting::IScript* {
            return new UI_SFXVolumeButtons();
            });
        registrar->RegisterScript("UI_AmbienceVolumeButtons", []() -> NE::Scripting::IScript* {
            return new UI_AmbienceVolumeButtons();
            });
        registrar->RegisterScript("Interactable_MirrorTile", []() -> NE::Scripting::IScript* {
            return new Interactable_MirrorTile();
            });
        registrar->RegisterScript("Misc_StopAnimationOnEvent", []() -> NE::Scripting::IScript* {
            return new Misc_StopAnimationOnEvent();
            });
        registrar->RegisterScript("Misc_SolvedMaterialOverride", []() -> NE::Scripting::IScript* {
            return new Misc_SolvedMaterialOverride();
            });
        //registrar->RegisterScript("Puzzle_FinalBomb", []() -> NE::Scripting::IScript* {
        //    return new Puzzle_FinalBomb();
        //    });
        registrar->RegisterScript("Puzzle_Bomb", []() -> NE::Scripting::IScript* {
            return new Puzzle_Bomb();
            });
        registrar->RegisterScript("UIButton_Quit", []() -> NE::Scripting::IScript* {
            return new UIButton_Quit();
            });
        registrar->RegisterScript("Credits_Controller", []() -> NE::Scripting::IScript* {
            return new Credits_Controller();
            });
        registrar->RegisterScript("Credits_ReturnToMainMenu", []() -> NE::Scripting::IScript* {
            return new Credits_ReturnToMainMenu();
            });
        registrar->RegisterScript("UIButton_SwitchToCredits", []() -> NE::Scripting::IScript* {
            return new UIButton_SwitchToCredits();
            });
        registrar->RegisterScript("UIButton_ResetVolumes", []() -> NE::Scripting::IScript* {
            return new UIButton_ResetVolumes();
            });
        registrar->RegisterScript("SplashScreen_Controller", []() -> NE::Scripting::IScript* {
            return new SplashScreen_Controller();
            });
        registrar->RegisterScript("SplashScreen_ReturnToMainMenu", []() -> NE::Scripting::IScript* {
            return new SplashScreen_ReturnToMainMenu();
            });
        registrar->RegisterScript("Misc_WireBlinking", []() -> NE::Scripting::IScript* {
            return new Misc_WireBlinking();
            });
        registrar->RegisterScript("Misc_ForcePastOnCollision", []() -> NE::Scripting::IScript* {
            return new Misc_ForcePastOnCollision();
            });
        registrar->RegisterScript("Misc_ObjectUI", []() -> NE::Scripting::IScript* {
            return new Misc_ObjectUI();
            });
        registrar->RegisterScript("Interactable_RotateLock", []() -> NE::Scripting::IScript* {
            return new Interactable_RotateLock();
            });
        registrar->RegisterScript("Puzzle_RotateLock", []() -> NE::Scripting::IScript* {
            return new Puzzle_RotateLock();
            });


        registrar->RegisterScript("UI_HoverTextColor", []() -> NE::Scripting::IScript* {
            return new UI_HoverTextColor();
            });
        registrar->RegisterScript("UI_OverlayFadeIn", []() -> NE::Scripting::IScript* {
            return new UI_OverlayFadeIn();
            });
        registrar->RegisterScript("Misc_PauseMenuHotkey", []() -> NE::Scripting::IScript* {
            return new Misc_PauseMenuHotkey();
            });
        registrar->RegisterScript("UIButton_PauseResume", []() -> NE::Scripting::IScript* {
            return new UIButton_PauseResume();
            });
        registrar->RegisterScript("UIButton_CanvasSwitcher", []() -> NE::Scripting::IScript* {
            return new UIButton_CanvasSwitcher();
            });


        registrar->RegisterScript("TaskManager", []() -> NE::Scripting::IScript* {
            return new TaskManager();
            });
        registrar->RegisterScript("TaskCheckpoint", []() -> NE::Scripting::IScript* {
            return new TaskCheckpoint();
            });
        registrar->RegisterScript("TriggerParentSwitcher", []() -> NE::Scripting::IScript* {
            return new TriggerParentSwitcher();
            });
        registrar->RegisterScript("TriggerMaterialVectorSwitcher", []() -> NE::Scripting::IScript* {
            return new TriggerMaterialVectorSwitcher();
            });
        registrar->RegisterScript("UI_SaveSettings", []() -> NE::Scripting::IScript* {
            return new UI_SaveSettings();
            });
        registrar->RegisterScript("FinalAreaManager", []() -> NE::Scripting::IScript* {
            return new FinalAreaManager();
            });
        

    }
}
