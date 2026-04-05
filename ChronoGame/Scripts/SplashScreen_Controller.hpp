#pragma once
#include "EngineAPI.hpp"
#include <ScriptSDK/UI.h>

/**
 * SplashScreen_Controller
 *
 * Displays a series of splash slides before the game starts.
 * Each slide is a UICanvas entity — toggled active/inactive per slide,
 * with alpha faded via SetUICanvasAlpha.
 * After all slides finish, the engine advances to the next scene automatically.
 *
 * Timing per slide:  fade in -> hold -> fade out  (all 2s by default)
 *
 * ── Inspector Fields ─────────────────────────────────────────────────────────
 *
 *  fadeInDuration   (Float)  Seconds to fade 0->1.  Default: 2.0s
 *  holdDuration     (Float)  Seconds to hold full opacity. Default: 2.0s
 *  fadeOutDuration  (Float)  Seconds to fade 1->0.  Default: 2.0s
 *
 *  slideCanvases    (GameObject Vector)
 *                            One UICanvas entity per slide, in display order.
 *                            e.g. DigiPen logo canvas, FMOD logo canvas, Game logo canvas.
 *                            Set all INACTIVE in the editor.
 *
 * ── Scene Setup ──────────────────────────────────────────────────────────────
 *  1. Create SplashScreen.scene, set it first in the engine scene order.
 *  2. For each logo: add a UICanvas with a UIImage child (and optional UIText).
 *     Mark each UICanvas INACTIVE in the editor.
 *  3. Add an empty GameObject, attach this script.
 *  4. Drag the UICanvas entities into slideCanvases in order.
 */
class SplashScreen_Controller : public IScript {
public:
    SplashScreen_Controller() {
        SCRIPT_FIELD(fadeInDuration,  Float);
        SCRIPT_FIELD(holdDuration,    Float);
        SCRIPT_FIELD(fadeOutDuration, Float);
        RegisterGameObjectRefVectorField("slideCanvases", &slideCanvases);
    }

    ~SplashScreen_Controller() override = default;

    void Awake()            override {}
    void Initialize(Entity) override {}
    void OnDestroy()        override {}
    void OnEnable()         override {}
    void OnDisable()        override {}
    void OnValidate()       override {}
    const char* GetTypeName() const override { return "SplashScreen_Controller"; }
    void OnCollisionEnter(Entity o) override { (void)o; }
    void OnCollisionExit(Entity o)  override { (void)o; }
    void OnCollisionStay(Entity o)  override { (void)o; }
    void OnTriggerEnter(Entity o)   override { (void)o; }
    void OnTriggerExit(Entity o)    override { (void)o; }
    void OnTriggerStay(Entity o)    override { (void)o; }

    void Start() override {
        if (fadeInDuration  <= 0.0f) fadeInDuration  = 2.0f;
        if (holdDuration    <= 0.0f) holdDuration    = 2.0f;
        if (fadeOutDuration <= 0.0f) fadeOutDuration = 2.0f;

        if (slideCanvases.empty()) {
            LOG_WARNING("SplashScreen_Controller: no slideCanvases set.");
            done = true;
            return;
        }

        // Ensure all canvases start hidden
        for (auto& ref : slideCanvases)
            if (ref.IsValid())
                SetActive(false, ref.GetEntity());

        currentIndex = 0;
        BeginSlide(currentIndex);
    }

    void Update(double deltaTime) override {
        if (done) return;

        timer += static_cast<float>(deltaTime);

        switch (phase) {
            case Phase::FADE_IN: {
                float t = timer / fadeInDuration;
                if (t >= 1.0f) {
                    SetSlideAlpha(currentIndex, 1.0f);
                    phase = Phase::HOLD;
                    timer = 0.0f;
                } else {
                    SetSlideAlpha(currentIndex, t);
                }
                break;
            }

            case Phase::HOLD: {
                if (timer >= holdDuration) {
                    phase = Phase::FADE_OUT;
                    timer = 0.0f;
                }
                break;
            }

            case Phase::FADE_OUT: {
                float t = 1.0f - (timer / fadeOutDuration);
                if (t <= 0.0f) {
                    // Set alpha to 0 this frame and let it render before toggling off.
                    SetSlideAlpha(currentIndex, 0.0f);
                    phase = Phase::ADVANCE;
                    timer = 0.0f;
                } else {
                    SetSlideAlpha(currentIndex, t);
                }
                break;
            }

            case Phase::ADVANCE: {
                // Alpha=0 was rendered last frame. Safe to toggle canvas off now.
                SetActive(false, slideCanvases[currentIndex].GetEntity());
                AdvanceSlide();
                break;
            }

            case Phase::DONE:
                break;
        }
    }

private:
    // ── Inspector fields ──────────────────────────────────────────────────
    float                      fadeInDuration  = 2.0f;
    float                      holdDuration    = 2.0f;
    float                      fadeOutDuration = 2.0f;
    std::vector<GameObjectRef> slideCanvases;

    // ── Runtime state ─────────────────────────────────────────────────────
    enum class Phase { FADE_IN, HOLD, FADE_OUT, ADVANCE, DONE };

    int   currentIndex = 0;
    float timer        = 0.0f;
    Phase phase        = Phase::FADE_IN;
    bool  done         = false;

    // ── Helpers ───────────────────────────────────────────────────────────

    void BeginSlide(int index) {
        if (!IsValid(index)) { done = true; phase = Phase::DONE; return; }

        Entity canvas = slideCanvases[index].GetEntity();
        NE::ECS::Command::SetUICanvasAlpha(canvas, 0.0f);
        SetActive(true, canvas);
        timer = 0.0f;
        phase = Phase::FADE_IN;

        LOG_DEBUG("SplashScreen_Controller: slide " << (index + 1)
            << " / " << slideCanvases.size());
    }

    void AdvanceSlide() {
        ++currentIndex;
        if (currentIndex < static_cast<int>(slideCanvases.size())) {
            BeginSlide(currentIndex);
        } else {
            done  = true;
            phase = Phase::DONE;
            Events::Send("SplashScreenDone");
            LOG_INFO("SplashScreen_Controller: all slides done.");
        }
    }

    void SetSlideAlpha(int index, float alpha) {
        if (IsValid(index))
            NE::ECS::Command::SetUICanvasAlpha(
                slideCanvases[index].GetEntity(), alpha);
    }

    bool IsValid(int index) const {
        return index >= 0
            && index < static_cast<int>(slideCanvases.size())
            && slideCanvases[index].IsValid();
    }
};
