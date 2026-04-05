#pragma once
#include "EngineAPI.hpp"
#include "Player_Controller.hpp"

/**
 * Misc_PauseMenuHotkey
 * --------------------
 * In-scene pause menu toggle.
 * Press P to show/hide the assigned pause menu UI.
 * While paused, Player_Controller is disabled and cursor is unlocked/visible.
 *
 * Attach to any active entity in gameplay levels (e.g. empty GameObject or manager).
 *
 * Inspector:
 *   pauseMenuRoot   Root UI entity of the pause menu (set INACTIVE by default).
 *   playerRef       Player entity with Player_Controller (optional but recommended).
 *   enableHotkey    Turn off to disable P without removing the script.
 *   startPaused     Start scene with pause menu active.
 */
class Misc_PauseMenuHotkey : public IScript {
public:
    Misc_PauseMenuHotkey() {
        SCRIPT_GAMEOBJECT_REF(pauseMenuRoot);
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_FIELD(enableHotkey, Bool);
        SCRIPT_FIELD(startPaused, Bool);
    }

    ~Misc_PauseMenuHotkey() override = default;

    void Awake() override {}
    void Initialize(Entity /*entity*/) override {}
    void Start() override {
        CachePlayerController();
        m_isPaused = startPaused;
        ApplyPauseState();
    }

    void Update(double /*deltaTime*/) override {
        if (!enableHotkey) return;
        if (!Input::WasKeyPressed(static_cast<int>('P'))) return;

        TogglePause();
    }

    void OnDestroy() override {
        // Do not touch Player_Controller here — it may already be destroyed. Cursor/lock only.
        Input::SetMouseLocked(true);
        NE::Scripting::SetMouseVisible(false);
    }
    void OnEnable() override {
        CachePlayerController();
    }
    void OnDisable() override {
        if (m_isPaused) {
            if (pauseMenuRoot.IsValid())
                SetActive(false, pauseMenuRoot.GetEntity());
            if (m_cachedPlayerController)
                m_cachedPlayerController->SetEnabled(m_playerControllerWasEnabledBeforePause);
            Input::SetMouseLocked(true);
            NE::Scripting::SetMouseVisible(false);
            m_isPaused = false;
        }
        m_cachedPlayerController = nullptr;
    }
    void OnValidate() override {}

    const char* GetTypeName() const override { return "Misc_PauseMenuHotkey"; }

    void SetPaused(bool paused) {
        if (m_isPaused == paused) return;
        m_isPaused = paused;
        ApplyPauseState();
    }

    void TogglePause() {
        m_isPaused = !m_isPaused;
        ApplyPauseState();
    }

    bool IsPaused() const { return m_isPaused; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef pauseMenuRoot;
    GameObjectRef playerRef;
    bool enableHotkey = true;
    bool startPaused = false;
    bool m_isPaused = false;
    Player_Controller* m_cachedPlayerController = nullptr;
    bool m_playerControllerWasEnabledBeforePause = true;

    void CachePlayerController() {
        m_cachedPlayerController = nullptr;
        if (!playerRef.IsValid()) return;

        GameObject playerGO(playerRef.GetEntity());
        if (!playerGO.IsValid()) return;

        m_cachedPlayerController = playerGO.GetComponent<Player_Controller>();
    }

    void ApplyPauseState() {
        if (pauseMenuRoot.IsValid()) {
            SetActive(m_isPaused, pauseMenuRoot.GetEntity());
        }

        if (m_cachedPlayerController) {
            if (m_isPaused) {
                m_playerControllerWasEnabledBeforePause = m_cachedPlayerController->IsEnabled();
                m_cachedPlayerController->ResetMovementOnly();
                m_cachedPlayerController->SetEnabled(false);
            } else {
                m_cachedPlayerController->ResetMovementOnly();
                m_cachedPlayerController->SetEnabled(m_playerControllerWasEnabledBeforePause);
            }
        }

        Input::SetMouseLocked(!m_isPaused);
        NE::Scripting::SetMouseVisible(m_isPaused);
    }
};
