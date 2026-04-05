#pragma once
#include "EngineAPI.hpp"
#include "Player_Controller.hpp"
#include <ScriptSDK/UI.h>
#include <algorithm>
#include <string>

/*
 * Misc_PlayerRespawn
 * Handles player death + respawn. Optional black UICanvas overlay matches
 * Misc_TransitionTeleporter: fade out → teleport to checkpoint → hold → fade in.
 *
 * Editor:
 * - playerRef / checkpointRef: required.
 * - overlayCanvas: full-screen black UICanvas (same setup as transition teleporter).
 *   If unset, respawn teleports instantly (legacy behavior).
 * - fadeOutDuration / fadeInDuration / holdBlackDuration: same meaning as Misc_TransitionTeleporter.
 *
 * RespawnNow(): plays hurt audio first, then fade-out / teleport (or audio then instant teleport if no overlay).
 * Trigger/collision respawn: no hurt audio (same as before).
 */

class Misc_PlayerRespawn : public IScript {
public:
    Misc_PlayerRespawn() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(checkpointRef);
        SCRIPT_GAMEOBJECT_REF(overlayCanvas);
        SCRIPT_FIELD(fadeOutDuration, Float);
        SCRIPT_FIELD(fadeInDuration, Float);
        SCRIPT_FIELD(holdBlackDuration, Float);
    }

    ~Misc_PlayerRespawn() override = default;

    void Awake() override { ValidateReferences(); }

    void Initialize(Entity entity) override { (void)entity; }

    void Start() override { ValidateReferences(); }

    void Update(double deltaTime) override {
        const float dt = static_cast<float>(deltaTime);
        if (phase == Phase::IDLE) return;

        switch (phase) {
        case Phase::FADE_OUT: {
            fadeTimer += dt;
            const float t = fadeOutDuration > 0.0f
                ? std::min(1.0f, fadeTimer / fadeOutDuration)
                : 1.0f;
            SetOverlayAlpha(t);
            if (t >= 1.0f - 1e-4f) {
                SetOverlayAlpha(1.0f);
                TeleportToCheckpoint();
                fadeTimer = 0.0f;
                if (holdBlackDuration > 0.0f) {
                    phase = Phase::HOLD_BLACK;
                } else {
                    FreePlayerForFadeIn();
                    phase = Phase::FADE_IN;
                }
            }
            break;
        }
        case Phase::HOLD_BLACK: {
            fadeTimer += dt;
            if (fadeTimer >= holdBlackDuration) {
                FreePlayerForFadeIn();
                phase = Phase::FADE_IN;
                fadeTimer = 0.0f;
            }
            break;
        }
        case Phase::FADE_IN: {
            if (!m_playerFreedForFadeIn)
                FreePlayerForFadeIn();
            if (fadeInDuration <= 0.0f) {
                SetOverlayAlpha(0.0f);
                FinishTransition();
                break;
            }
            fadeTimer += dt;
            const float t = std::max(0.0f, 1.0f - fadeTimer / fadeInDuration);
            SetOverlayAlpha(t);
            if (fadeTimer >= fadeInDuration - 1e-4f) {
                SetOverlayAlpha(0.0f);
                FinishTransition();
            }
            break;
        }
        default:
            break;
        }
    }

    void OnDestroy() override {}

    void OnEnable() override {}
    void OnDisable() override {
        if (phase == Phase::IDLE)
            return;
        RestorePlayerController();
        if (overlayCanvas.IsValid()) {
            NE::ECS::Command::SetUICanvasAlpha(overlayCanvas.GetEntity(), 0.0f);
            SetActive(false, overlayCanvas.GetEntity());
        }
        m_playerFreedForFadeIn = false;
        phase = Phase::IDLE;
    }
    void OnValidate() override { ValidateReferences(); }

    const char* GetTypeName() const override {
        return "Misc_PlayerRespawn";
    }

    void RespawnNow() {
        RequestRespawn(/*playHurtAudio*/ true);
    }

    void OnCollisionEnter(Entity other) override { HandlePlayerEnter(other); }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { HandlePlayerEnter(other); }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef playerRef;
    GameObjectRef checkpointRef;
    GameObjectRef overlayCanvas;
    float fadeOutDuration = 0.5f;
    float fadeInDuration = 0.5f;
    float holdBlackDuration = 0.0f;

    enum class Phase { IDLE, FADE_OUT, HOLD_BLACK, FADE_IN };
    Phase phase = Phase::IDLE;
    float fadeTimer = 0.0f;

    bool cachedPlayerControllerWasEnabled = true;
    bool m_playerFreedForFadeIn = false;

    void ValidateReferences() {
        if (!playerRef.IsValid()) {
            LOG_WARNING("Misc_PlayerRespawn: missing Player reference");
        }
        if (!checkpointRef.IsValid()) {
            LOG_WARNING("Misc_PlayerRespawn: missing Checkpoint reference");
        }
    }

    void HandlePlayerEnter(Entity other) {
        if (!playerRef.IsValid() || !checkpointRef.IsValid()) {
            return;
        }
        if (other != playerRef.GetEntity()) {
            return;
        }
        RequestRespawn(/*playHurtAudio*/ false);
    }

    void RequestRespawn(bool playHurtAudio) {
        if (phase != Phase::IDLE) return;

        if (!playerRef.IsValid() || !checkpointRef.IsValid()) {
            LOG_WARNING("Misc_PlayerRespawn: missing Player or Checkpoint reference");
            return;
        }

        if (!overlayCanvas.IsValid()) {
            if (playHurtAudio)
                PlayAudio("event:/PLAYER_HURT");
            TeleportToCheckpoint();
            LOG_DEBUG("Misc_PlayerRespawn: instant respawn (no overlay)");
            return;
        }

        if (fadeInDuration < 0.0f) fadeInDuration = 0.0f;

        if (playHurtAudio)
            PlayAudio("event:/PLAYER_HURT");

        m_playerFreedForFadeIn = false;
        CacheAndDisablePlayerController();

        const Entity canvasEntity = overlayCanvas.GetEntity();
        SetActive(true, canvasEntity);
        if (NE::ECS::Query::HasUICanvas(canvasEntity)) {
            auto& canvas = NE::ECS::Command::GetUICanvas(canvasEntity);
            canvas.isActive = true;
        }

        if (fadeOutDuration <= 0.0f) {
            NE::ECS::Command::SetUICanvasAlpha(canvasEntity, 1.0f);
            TeleportToCheckpoint();
            fadeTimer = 0.0f;
            if (holdBlackDuration > 0.0f)
                phase = Phase::HOLD_BLACK;
            else {
                FreePlayerForFadeIn();
                phase = Phase::FADE_IN;
            }
            return;
        }

        NE::ECS::Command::SetUICanvasAlpha(canvasEntity, 0.0f);
        fadeTimer = 0.0f;
        phase = Phase::FADE_OUT;
    }

    void TeleportToCheckpoint() {
        if (!playerRef.IsValid() || !checkpointRef.IsValid()) {
            return;
        }
        const Entity playerEntity = playerRef.GetEntity();
        const Entity checkpointEntity = checkpointRef.GetEntity();
        Vec3 checkpointPos = TF_GetPosition(checkpointEntity);
        CC_SetPosition(checkpointPos, playerEntity);
        LOG_DEBUG(("Misc_PlayerRespawn: teleported to checkpoint (" +
            std::to_string(checkpointPos.x) + ", " +
            std::to_string(checkpointPos.y) + ", " +
            std::to_string(checkpointPos.z) + ")").c_str());
    }

    void FinishTransition() {
        if (overlayCanvas.IsValid()) {
            NE::ECS::Command::SetUICanvasAlpha(overlayCanvas.GetEntity(), 0.0f);
            SetActive(false, overlayCanvas.GetEntity());
        }
        if (!m_playerFreedForFadeIn)
            RestorePlayerController();
        m_playerFreedForFadeIn = false;
        phase = Phase::IDLE;
    }

    void SetOverlayAlpha(float alpha) {
        if (overlayCanvas.IsValid())
            NE::ECS::Command::SetUICanvasAlpha(overlayCanvas.GetEntity(), alpha);
    }

    void FreePlayerForFadeIn() {
        if (m_playerFreedForFadeIn)
            return;
        RestorePlayerController();
        m_playerFreedForFadeIn = true;
    }

    void CacheAndDisablePlayerController() {
        cachedPlayerControllerWasEnabled = true;

        GameObject playerGO(playerRef.GetEntity());
        if (!playerGO.IsValid())
            return;

        Player_Controller* pc = playerGO.GetComponent<Player_Controller>();
        if (!pc)
            return;

        cachedPlayerControllerWasEnabled = pc->IsEnabled();
        pc->ResetMovementOnly();
        pc->SetEnabled(false);
    }

    void RestorePlayerController() {
        if (!playerRef.IsValid())
            return;
        GameObject playerGO(playerRef.GetEntity());
        if (!playerGO.IsValid())
            return;
        Player_Controller* pc = playerGO.GetComponent<Player_Controller>();
        if (pc) {
            pc->ResetMovementOnly();
            pc->SetEnabled(cachedPlayerControllerWasEnabled);
        }
    }
};
