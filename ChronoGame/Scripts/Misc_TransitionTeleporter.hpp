#pragma once
#include "EngineAPI.hpp"
#include "Player_Controller.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

/**
 * Misc_TransitionTeleporter
 *
 * Same flow as Misc_Teleporter (trigger + optional event, or instant on trigger),
 * but runs a transition: freeze player → fade OUT black UI → teleport → restore player → fade IN.
 *
 * Editor:
 * - overlayCanvas: full-screen UICanvas (black UIImage child). Prefer **screen-space** canvas so the fade
 *   is independent of camera rotation (world-space UI can look like it “moves with the mouse”).
 *   Can start inactive; script activates it.
 * - fadeOutDuration / fadeInDuration: seconds for 0→1 and 1→0 alpha on the canvas.
 * - eventBased + eventName: wait for both player-in-zone AND Events::Send(eventName) (e.g. lift solved).
 *
 * Sequence (always: teleport happens BEFORE fade-in reveals the new area):
 * 1) Disable Player_Controller (movement/camera input frozen).
 * 2) Show overlay — either fade alpha 0→1 (fadeOutDuration > 0) or instant black (fadeOutDuration == 0).
 * 3) CC_SetPosition(player, target transform position) while screen is fully black.
 * 4) Optional hold at black (holdBlackDuration).
 * 5) Re-enable Player_Controller (player can move/look while overlay still black).
 * 6) Fade alpha 1→0 (fadeInDuration), then hide overlay.
 *
 * Tip: Set fadeOutDuration = 0 in the editor for “cut to black → teleport → fade in” with no fade-out animation.
 */
class Misc_TransitionTeleporter : public IScript {
public:
    Misc_TransitionTeleporter() {
        SCRIPT_GAMEOBJECT_REF(player);
        SCRIPT_COMPONENT_REF(targetTransformRef, TransformRef);
        SCRIPT_GAMEOBJECT_REF(overlayCanvas);
        SCRIPT_FIELD(fadeOutDuration, Float);
        SCRIPT_FIELD(fadeInDuration, Float);
        SCRIPT_FIELD(holdBlackDuration, Float);
        SCRIPT_FIELD(eventBased, Bool);
        SCRIPT_FIELD(eventName, String);
        SCRIPT_FIELD(impactAudioName, String);
    }

    ~Misc_TransitionTeleporter() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { (void)entity; }

    void Start() override {
        playerInZone = false;
        msgReceived = false;

        if (eventBased && !eventName.empty()) {
            if (!m_eventCallbackAlive)
                m_eventCallbackAlive = std::make_shared<std::atomic<bool>>(true);
            std::shared_ptr<std::atomic<bool>> alive = m_eventCallbackAlive;
            Events::Listen(eventName.c_str(), [alive, this](void*) {
                if (!alive->load(std::memory_order_acquire))
                    return;
                msgReceived = true;
                LOG_DEBUG("[Misc_TransitionTeleporter] Event received: " + eventName);
                TryStartTransition();
            });
        }
    }

    void Update(double deltaTime) override {
        const float dt = static_cast<float>(deltaTime);
        if (phase == Phase::IDLE)
            return;

        switch (phase) {
        case Phase::FADE_OUT: {
            fadeTimer += dt;
            const float t = fadeOutDuration > 0.0f
                ? std::min(1.0f, fadeTimer / fadeOutDuration)
                : 1.0f;
            SetOverlayAlpha(t);
            if (t >= 1.0f - 1e-4f) {
                SetOverlayAlpha(1.0f);
                DoTeleport();
                if (holdBlackDuration > 0.0f) {
                    phase = Phase::HOLD_BLACK;
                    fadeTimer = 0.0f;
                } else {
                    // Unfreeze *before* fade-in phase so Player_Controller gets input this frame
                    // even if it runs earlier in the script update order than this component.
                    FreePlayerForFadeIn();
                    phase = Phase::FADE_IN;
                    fadeTimer = 0.0f;
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
            // Backup if some path entered FADE_IN without unfreezing (should not happen).
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

    void OnDestroy() override {
        if (m_eventCallbackAlive)
            m_eventCallbackAlive->store(false, std::memory_order_release);
    }

    void OnEnable() override {
        if (m_eventCallbackAlive)
            m_eventCallbackAlive->store(true, std::memory_order_release);
    }
    void OnDisable() override {
        if (m_eventCallbackAlive)
            m_eventCallbackAlive->store(false, std::memory_order_release);
        if (phase == Phase::IDLE)
            return;
        if (overlayCanvas.IsValid()) {
            NE::ECS::Command::SetUICanvasAlpha(overlayCanvas.GetEntity(), 0.0f);
            SetActive(false, overlayCanvas.GetEntity());
        }
        if (!m_playerFreedForFadeIn)
            RestorePlayerController();
        m_playerFreedForFadeIn = false;
        phase = Phase::IDLE;
    }
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Misc_TransitionTeleporter";
    }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }

    void OnTriggerEnter(Entity other) override {
        (void)other;
        if (!eventBased) {
            if (!impactAudioName.empty())
                PlayAudio("event:/" + impactAudioName);
            TryStartTransition();
        } else {
            playerInZone = true;
            LOG_DEBUG("[Misc_TransitionTeleporter] Player entered zone, waiting for event: " + eventName);
            TryStartTransition();
        }
    }

    void OnTriggerExit(Entity other) override {
        (void)other;
        if (eventBased) {
            playerInZone = false;
            LOG_DEBUG("[Misc_TransitionTeleporter] Player left zone.");
        }
    }

    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef player;
    TransformRef targetTransformRef;
    GameObjectRef overlayCanvas;
    float fadeOutDuration = 0.5f;
    float fadeInDuration = 0.5f;
    float holdBlackDuration = 0.0f;
    bool eventBased = false;
    std::string eventName;
    std::string impactAudioName = "PLAYER_IMPACT";

    bool playerInZone = false;
    bool msgReceived = false;

    enum class Phase { IDLE, FADE_OUT, HOLD_BLACK, FADE_IN };
    Phase phase = Phase::IDLE;
    float fadeTimer = 0.0f;

    std::shared_ptr<std::atomic<bool>> m_eventCallbackAlive;
    bool cachedPlayerControllerWasEnabled = true;
    bool m_playerFreedForFadeIn = false;

    void TryStartTransition() {
        if (phase != Phase::IDLE)
            return;

        if (eventBased) {
            if (!(playerInZone && msgReceived))
                return;
        }

        ResolvePlayerRef();
        if (!player.IsValid()) {
            LOG_ERROR("[Misc_TransitionTeleporter] Player reference invalid.");
            return;
        }

        if (!overlayCanvas.IsValid()) {
            LOG_ERROR("[Misc_TransitionTeleporter] overlayCanvas not set.");
            return;
        }

        if (!targetTransformRef.IsValid()) {
            LOG_ERROR("[Misc_TransitionTeleporter] targetTransformRef not set.");
            return;
        }

        if (fadeInDuration < 0.0f)
            fadeInDuration = 0.0f;

        if (!impactAudioName.empty())
            PlayAudio("event:/" + impactAudioName);

        m_playerFreedForFadeIn = false;
        CacheAndDisablePlayerController();

        const Entity canvasEntity = overlayCanvas.GetEntity();
        SetActive(true, canvasEntity);

        if (eventBased) {
            playerInZone = false;
            msgReceived = false;
        }

        // Instant black (no fade-out): teleport immediately, then only fade in.
        if (fadeOutDuration <= 0.0f) {
            NE::ECS::Command::SetUICanvasAlpha(canvasEntity, 1.0f);
            DoTeleport();
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

    void DoTeleport() {
        CC_SetPosition(GetPosition(targetTransformRef), player.GetEntity());
        LOG_DEBUG("[Misc_TransitionTeleporter] Teleported player to S2 target.");
    }

    void FinishTransition() {
        if (overlayCanvas.IsValid()) {
            NE::ECS::Command::SetUICanvasAlpha(overlayCanvas.GetEntity(), 0.0f);
            SetActive(false, overlayCanvas.GetEntity());
        }
        // Player was already restored at start of FADE_IN; safety if fade-in was skipped.
        if (!m_playerFreedForFadeIn)
            RestorePlayerController();
        m_playerFreedForFadeIn = false;
        phase = Phase::IDLE;
    }

    void SetOverlayAlpha(float alpha) {
        if (overlayCanvas.IsValid())
            NE::ECS::Command::SetUICanvasAlpha(overlayCanvas.GetEntity(), alpha);
    }

    /** Call as soon as we're ready to fade in (after teleport / hold): before phase = FADE_IN. */
    void FreePlayerForFadeIn() {
        if (m_playerFreedForFadeIn)
            return;
        RestorePlayerController();
        m_playerFreedForFadeIn = true;
    }

    void CacheAndDisablePlayerController() {
        cachedPlayerControllerWasEnabled = true;

        GameObject playerGO(player.GetEntity());
        if (!playerGO.IsValid())
            return;

        Player_Controller* pc = playerGO.GetComponent<Player_Controller>();
        if (!pc)
            return;

        cachedPlayerControllerWasEnabled = pc->IsEnabled();
        // Do NOT call Reset() — it zeros lookRotation while the camera still shows the old view → snap on unfreeze.
        pc->ResetMovementOnly();
        pc->SetEnabled(false);
    }

    void RestorePlayerController() {
        if (!player.IsValid())
            return;
        GameObject playerGO(player.GetEntity());
        if (!playerGO.IsValid())
            return;
        Player_Controller* pc = playerGO.GetComponent<Player_Controller>();
        if (pc) {
            // Do NOT call Reset() — preserves lookRotation / camera aim through the fade.
            pc->ResetMovementOnly();
            pc->SetEnabled(cachedPlayerControllerWasEnabled);
        }
    }

    void ResolvePlayerRef() {
        if (player.IsValid())
            return;

        auto players = GameObject::FindObjectsOfType<Player_Controller>();
        if (players.size() == 0)
            return;
        if (players.size() > 1)
            LOG_WARNING("[Misc_TransitionTeleporter] Multiple Player_Controller found; using first.");
        player.SetEntity(players.begin()->GetEntityId());
    }
};
