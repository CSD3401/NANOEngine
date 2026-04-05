#pragma once
#include "EngineAPI.hpp"
#include "Player_Controller.hpp"
#include <ScriptSDK/UI.h>
#include <algorithm>
#include <cmath>
#include <string>

/**
 * FinalArea_Manager
 * -----------------
 * Manages the final area crisis sequence.
 *
 * FLOW:
 *   1) Player walks into the holding area trigger zone -> OnTriggerEnter fires ->
 *      countdown begins and "FINAL_AREA_STARTED" is sent (use this to lock time-switching).
 *   2) Player must solve all 4 puzzle conditions within countdownDuration seconds:
 *        - Mirror puzzle      (listens: "FINALmirrorsolved")
 *        - Sequencer puzzle   (listens: "FINALsequencersolved")
 *        - Wire puzzle        (listens: "FINALwiresolved")
 *        - All 5 mini-bombs   (listens: "bomb1solved".."bomb5solved")
 *   3a) ALL solved before time runs out -> sends "FINAL_SUCCESS" -> ending cutscene.
 *   3b) Timer reaches 0 with any condition unsolved -> sends "FINAL_FAIL" ->
 *       teleports player back to the holding area (uses Misc_PlayerRespawn pattern).
 *
 * EDITOR SETUP:
 *   - playerRef            : the Player GameObject
 *   - holdingAreaSpawnRef  : the Transform the player is sent back to on FINAL_FAIL
 *   - overlayCanvas        : full-screen black UICanvas for the fail fade transition
 *   - timerTextRef         : a UIText entity whose text is updated each frame with MM:SS
 *   - countdownDuration    : seconds (default 60)
 *   - fadeOutDuration      : seconds to fade to black on fail (default 0.5)
 *   - fadeInDuration       : seconds to fade back in on fail (default 0.5)
 *   - holdBlackDuration    : seconds to hold black before fade-in on fail (default 0.3)
 *
 * NOTES:
 *   - The trigger collider on this GameObject is the "holding area" boundary.
 *     Make it large enough to catch the player walking through the doorway.
 *   - The sequencer puzzle uses "FINALsequencersolved" as its solvedEventName —
 *     set that in the Puzzle_MultiLightSequencer inspector.
 *   - The mirror puzzle uses whatever eventName is set in MirrorPuzzle inspector;
 *     set it to "FINALmirrorsolved".
 *   - The wire puzzle uses the eventName field in Puzzle_Wire; set it to "FINALwiresolved".
 *   - Mini-bombs each have their solveMessage set to "bomb1solved".."bomb5solved"
 *     in their respective Puzzle_Bomb inspectors.
 *   - On FINAL_FAIL the manager resets fully so the player can retry after re-entering
 *     the trigger zone.
 */

class FinalAreaManager : public IScript {
public:
    FinalAreaManager() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(holdingAreaSpawnRef);
        SCRIPT_GAMEOBJECT_REF(overlayCanvas);
        SCRIPT_GAMEOBJECT_REF(timerTextRef);
        SCRIPT_FIELD(countdownDuration, Float);
        SCRIPT_FIELD(fadeOutDuration, Float);
        SCRIPT_FIELD(fadeInDuration, Float);
        SCRIPT_FIELD(holdBlackDuration, Float);
    }

    ~FinalAreaManager() override = default;

    void Awake()  override {}
    void OnDestroy() override {}
    void OnEnable()  override {}
    void OnDisable() override {}
    void OnValidate() override {}

    void Initialize(Entity entity) override { m_self = entity; }

    void Start() override {
        // Defaults
        if (countdownDuration <= 0.0f) countdownDuration = 60.0f;
        if (fadeOutDuration < 0.0f) fadeOutDuration = 0.5f;
        if (fadeInDuration < 0.0f) fadeInDuration = 0.5f;
        if (holdBlackDuration < 0.0f) holdBlackDuration = 0.3f;

        ResetState();
        BindPuzzleEvents();

        LOG_DEBUG("[FinalArea_Manager] Ready. Waiting for player to enter holding area trigger.");
    }

    void Update(double deltaTime) override {
        const float dt = static_cast<float>(deltaTime);

        // ── Countdown ──────────────────────────────────────────────
        if (m_phase == Phase::COUNTDOWN) {
            m_timeRemaining -= dt;

            UpdateTimerText(std::max(0.0f, m_timeRemaining));

            if (CheckAllSolved()) {
                TriggerSuccess();
                return;
            }

            if (m_timeRemaining <= 0.0f) {
                TriggerFail();
                return;
            }
        }

        // ── Fail fade-out ──────────────────────────────────────────
        if (m_phase == Phase::FAIL_FADE_OUT) {
            m_fadeTimer += dt;
            const float t = fadeOutDuration > 0.0f
                ? std::min(1.0f, m_fadeTimer / fadeOutDuration)
                : 1.0f;
            SetOverlayAlpha(t);
            if (t >= 1.0f - 1e-4f) {
                SetOverlayAlpha(1.0f);
                TeleportToHoldingArea();
                m_fadeTimer = 0.0f;
                m_phase = holdBlackDuration > 0.0f ? Phase::FAIL_HOLD : Phase::FAIL_FADE_IN;
                if (m_phase == Phase::FAIL_FADE_IN)
                    FreePlayer();
            }
            return;
        }

        // ── Fail hold black ────────────────────────────────────────
        if (m_phase == Phase::FAIL_HOLD) {
            m_fadeTimer += dt;
            if (m_fadeTimer >= holdBlackDuration) {
                m_fadeTimer = 0.0f;
                m_phase = Phase::FAIL_FADE_IN;
                FreePlayer();
            }
            return;
        }

        // ── Fail fade-in ───────────────────────────────────────────
        if (m_phase == Phase::FAIL_FADE_IN) {
            if (fadeInDuration <= 0.0f) {
                SetOverlayAlpha(0.0f);
                FinishFail();
                return;
            }
            m_fadeTimer += dt;
            const float t = std::max(0.0f, 1.0f - m_fadeTimer / fadeInDuration);
            SetOverlayAlpha(t);
            if (m_fadeTimer >= fadeInDuration - 1e-4f) {
                SetOverlayAlpha(0.0f);
                FinishFail();
            }
            return;
        }
    }

    const char* GetTypeName() const override { return "FinalArea_Manager"; }

    // ── Trigger: player enters the holding area ────────────────────
    void OnTriggerEnter(Entity other) override {
        if (m_phase != Phase::IDLE) return;
        if (!IsPlayer(other))       return;

        LOG_DEBUG("[FinalArea_Manager] Player entered holding area — countdown started.");
        m_phase = Phase::COUNTDOWN;
        m_timeRemaining = countdownDuration;
        UpdateTimerText(m_timeRemaining);

        // Notify other systems (e.g. time-switch lock script)
        Events::Send("FINAL_AREA_STARTED");
    }

    void OnTriggerExit(Entity other)  override { (void)other; }
    void OnTriggerStay(Entity other)  override { (void)other; }
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other)  override { (void)other; }
    void OnCollisionStay(Entity other)  override { (void)other; }

private:
    // ── Inspector fields ───────────────────────────────────────────
    GameObjectRef playerRef;
    GameObjectRef holdingAreaSpawnRef;
    GameObjectRef overlayCanvas;
    GameObjectRef timerTextRef;
    float countdownDuration = 60.0f;
    float fadeOutDuration = 0.5f;
    float fadeInDuration = 0.5f;
    float holdBlackDuration = 0.3f;

    // ── Runtime ────────────────────────────────────────────────────
    Entity m_self = 0;

    enum class Phase {
        IDLE,
        COUNTDOWN,
        SUCCESS,
        FAIL_FADE_OUT,
        FAIL_HOLD,
        FAIL_FADE_IN
    };
    Phase m_phase = Phase::IDLE;

    float m_timeRemaining = 0.0f;
    float m_fadeTimer = 0.0f;

    // Puzzle flags
    bool m_mirrorSolved = false;
    bool m_sequencerSolved = false;
    bool m_wireSolved = false;
    bool m_bomb1Solved = false;
    bool m_bomb2Solved = false;
    bool m_bomb3Solved = false;
    bool m_bomb4Solved = false;
    bool m_bomb5Solved = false;

    // Player freeze
    Player_Controller* m_cachedController = nullptr;
    bool               m_controllerWasEnabled = true;
    bool               m_playerFreedForFadeIn = false;

    // ── Helpers ───────────────────────────────────────────────────

    bool IsPlayer(Entity e) const {
        if (!playerRef.IsValid()) return false;
        return e == playerRef.GetEntity();
    }

    bool CheckAllSolved() const {
        return m_mirrorSolved
            && m_sequencerSolved
            && m_wireSolved
            && m_bomb1Solved
            && m_bomb2Solved
            && m_bomb3Solved
            && m_bomb4Solved
            && m_bomb5Solved;
    }

    void ResetState() {
        m_phase = Phase::IDLE;
        m_timeRemaining = countdownDuration;
        m_fadeTimer = 0.0f;

        m_mirrorSolved = false;
        m_sequencerSolved = false;
        m_wireSolved = false;
        m_bomb1Solved = false;
        m_bomb2Solved = false;
        m_bomb3Solved = false;
        m_bomb4Solved = false;
        m_bomb5Solved = false;

        m_cachedController = nullptr;
        m_controllerWasEnabled = true;
        m_playerFreedForFadeIn = false;

        // Show full time on the text immediately
        UpdateTimerText(countdownDuration);
    }

    void BindPuzzleEvents() {
        Events::Listen("FINALmirrorsolved", [this](void*) {
            m_mirrorSolved = true;
            LOG_DEBUG("[FinalArea_Manager] Mirror solved.");
            LogStatus();
            });
        Events::Listen("FINALsequencersolved", [this](void*) {
            m_sequencerSolved = true;
            LOG_DEBUG("[FinalArea_Manager] Sequencer solved.");
            LogStatus();
            });
        Events::Listen("FINALwiresolved", [this](void*) {
            m_wireSolved = true;
            LOG_DEBUG("[FinalArea_Manager] Wire solved.");
            LogStatus();
            });
        Events::Listen("bomb1solved", [this](void*) { m_bomb1Solved = true; LOG_DEBUG("[FinalArea_Manager] Bomb 1 solved."); LogStatus(); });
        Events::Listen("bomb2solved", [this](void*) { m_bomb2Solved = true; LOG_DEBUG("[FinalArea_Manager] Bomb 2 solved."); LogStatus(); });
        Events::Listen("bomb3solved", [this](void*) { m_bomb3Solved = true; LOG_DEBUG("[FinalArea_Manager] Bomb 3 solved."); LogStatus(); });
        Events::Listen("bomb4solved", [this](void*) { m_bomb4Solved = true; LOG_DEBUG("[FinalArea_Manager] Bomb 4 solved."); LogStatus(); });
        Events::Listen("bomb5solved", [this](void*) { m_bomb5Solved = true; LOG_DEBUG("[FinalArea_Manager] Bomb 5 solved."); LogStatus(); });
    }

    // ── Timer text (MM:SS) ────────────────────────────────────────
    void UpdateTimerText(float seconds) {
        if (!timerTextRef.IsValid()) return;
        const Entity textEntity = timerTextRef.GetEntity();
        if (textEntity == 0) return;

        const int totalSec = static_cast<int>(std::ceil(seconds));
        const int mins = totalSec / 60;
        const int secs = totalSec % 60;

        // Format as M:SS
        std::string mStr = std::to_string(mins);
        std::string sStr = (secs < 10 ? "0" : "") + std::to_string(secs);
        std::string display = mStr + ":" + sStr;

        NE::Scripting::SetUIText(textEntity, display.c_str());
    }

    // ── Outcome handlers ─────────────────────────────────────────

    void TriggerSuccess() {
        if (m_phase == Phase::SUCCESS) return;
        m_phase = Phase::SUCCESS;

        UpdateTimerText(0.0f); // freeze display at 0:00 (won't reach 0 in practice)
        LOG_DEBUG("[FinalArea_Manager] ALL PUZZLES SOLVED — sending FINAL_SUCCESS.");
        Events::Send("FINAL_SUCCESS");
    }

    void TriggerFail() {
        m_phase = Phase::FAIL_FADE_OUT;
        m_fadeTimer = 0.0f;

        UpdateTimerText(0.0f);
        LOG_DEBUG("[FinalArea_Manager] Time's up — sending FINAL_FAIL, teleporting back.");
        Events::Send("FINAL_FAIL");

        // Freeze player during the transition
        FreezePlayer();

        // Show overlay
        if (overlayCanvas.IsValid()) {
            const Entity canvas = overlayCanvas.GetEntity();
            SetActive(true, canvas);
            if (NE::ECS::Query::HasUICanvas(canvas)) {
                auto& c = NE::ECS::Command::GetUICanvas(canvas);
                c.isActive = true;
            }
            // Instant black if no fade-out
            if (fadeOutDuration <= 0.0f) {
                NE::ECS::Command::SetUICanvasAlpha(canvas, 1.0f);
                TeleportToHoldingArea();
                m_phase = holdBlackDuration > 0.0f ? Phase::FAIL_HOLD : Phase::FAIL_FADE_IN;
                if (m_phase == Phase::FAIL_FADE_IN)
                    FreePlayer();
            }
            else {
                NE::ECS::Command::SetUICanvasAlpha(canvas, 0.0f);
            }
        }
        else {
            // No overlay — instant teleport and reset
            TeleportToHoldingArea();
            FinishFail();
        }
    }

    void TeleportToHoldingArea() {
        if (!playerRef.IsValid() || !holdingAreaSpawnRef.IsValid()) {
            LOG_WARNING("[FinalArea_Manager] Cannot teleport — playerRef or holdingAreaSpawnRef not set.");
            return;
        }
        const Vec3 spawnPos = TF_GetPosition(holdingAreaSpawnRef.GetEntity());
        CC_SetPosition(spawnPos, playerRef.GetEntity());
        LOG_DEBUG("[FinalArea_Manager] Player teleported back to holding area.");
    }

    void FinishFail() {
        // Hide overlay
        if (overlayCanvas.IsValid()) {
            NE::ECS::Command::SetUICanvasAlpha(overlayCanvas.GetEntity(), 0.0f);
            SetActive(false, overlayCanvas.GetEntity());
        }
        // Restore player if not already done
        if (!m_playerFreedForFadeIn)
            FreePlayer();

        // Full reset so the player can retry by walking back into the trigger
        ResetState();
        LOG_DEBUG("[FinalArea_Manager] Reset complete — player can retry.");
    }

    // ── Player freeze helpers ─────────────────────────────────────

    void FreezePlayer() {
        m_cachedController = nullptr;
        m_controllerWasEnabled = true;
        m_playerFreedForFadeIn = false;

        if (!playerRef.IsValid()) return;
        GameObject go(playerRef.GetEntity());
        if (!go.IsValid()) return;

        m_cachedController = go.GetComponent<Player_Controller>();
        if (!m_cachedController) return;

        m_controllerWasEnabled = m_cachedController->IsEnabled();
        m_cachedController->ResetMovementOnly();
        m_cachedController->SetEnabled(false);
    }

    void FreePlayer() {
        if (m_playerFreedForFadeIn) return;
        if (m_cachedController) {
            m_cachedController->ResetMovementOnly();
            m_cachedController->SetEnabled(m_controllerWasEnabled);
        }
        m_cachedController = nullptr;
        m_playerFreedForFadeIn = true;
    }

    void SetOverlayAlpha(float alpha) {
        if (!overlayCanvas.IsValid()) return;
        NE::ECS::Command::SetUICanvasAlpha(overlayCanvas.GetEntity(), alpha);
    }

    void LogStatus() const {
        LOG_DEBUG("[FinalArea_Manager] Status — "
            "Mirror=" + std::to_string(m_mirrorSolved) +
            " Seq=" + std::to_string(m_sequencerSolved) +
            " Wire=" + std::to_string(m_wireSolved) +
            " Bombs=" + std::to_string(m_bomb1Solved) +
            std::to_string(m_bomb2Solved) +
            std::to_string(m_bomb3Solved) +
            std::to_string(m_bomb4Solved) +
            std::to_string(m_bomb5Solved) +
            " Time=" + std::to_string(static_cast<int>(m_timeRemaining)) + "s");
    }
};