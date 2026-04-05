#pragma once
#include "EngineAPI.hpp"
#include <ScriptSDK/UI.h>

/**
 * Credits_Controller
 *
 * Scrolls a single tall UIImage upward to create a rolling credits sequence.
 * After the image fully scrolls off screen, fires "CreditsDone" and stops.
 *
 * ── Inspector Fields ─────────────────────────────────────────────────────────
 *
 *  creditImage       (GameObject)  The UIImage entity to scroll.
 *                                  Position it in the editor so its top edge
 *                                  starts just below the bottom of the screen.
 *
 *  scrollSpeed       (Float)  Pixels per second to scroll. Default: 100.
 *                             If the image moves DOWN instead of UP, set this
 *                             to a negative value.
 *
 *  totalScrollDist   (Float)  Total pixels to scroll before credits end.
 *                             Set this to roughly:
 *                               imageHeight + screenHeight  (e.g. 4000 + 1080)
 *                             When this distance is reached, "CreditsDone" fires.
 *
 * ── Scene Setup ──────────────────────────────────────────────────────────────
 *  1. Add a UICanvas to the scene.
 *  2. Add a UIImage child — assign your long credits texture to it.
 *     Set its UIRectTransform so the image starts below the visible screen.
 *     (e.g. if anchor is centre, set y to -(screenHeight/2 + imageHeight/2))
 *  3. Add an empty GameObject, attach this script.
 *  4. Drag the UIImage entity into creditImage.
 *  5. Set scrollSpeed and totalScrollDist in the inspector.
 *
 * ── End of Credits ────────────────────────────────────────────────────────────
 *  When done, fires the event "CreditsDone".
 *  Hook up a listener or use the engine's scene order to transition out.
 */
class Credits_Controller : public IScript {
public:
    Credits_Controller() {
        SCRIPT_GAMEOBJECT_REF(creditImage);
        SCRIPT_FIELD(scrollSpeed,     Float);
        SCRIPT_FIELD(totalScrollDist, Float);
    }

    ~Credits_Controller() override = default;

    void Awake()            override {}
    void Initialize(Entity) override {}
    void OnDestroy()        override {}
    void OnEnable()         override {}
    void OnDisable()        override {}
    void OnValidate()       override {}
    const char* GetTypeName() const override { return "Credits_Controller"; }
    void OnCollisionEnter(Entity o) override { (void)o; }
    void OnCollisionExit(Entity o)  override { (void)o; }
    void OnCollisionStay(Entity o)  override { (void)o; }
    void OnTriggerEnter(Entity o)   override { (void)o; }
    void OnTriggerExit(Entity o)    override { (void)o; }
    void OnTriggerStay(Entity o)    override { (void)o; }

    void Start() override {
        if (!creditImage.IsValid()) {
            LOG_ERROR("Credits_Controller: creditImage is not set!");
            done = true;
            return;
        }

        if (scrollSpeed     == 0.0f) scrollSpeed     = 100.0f;
        if (totalScrollDist <= 0.0f) totalScrollDist = 5080.0f;

        scrolled = 0.0f;
        done     = false;

        LOG_DEBUG("Credits_Controller: starting. scrollSpeed=" << scrollSpeed
            << " totalScrollDist=" << totalScrollDist);
    }

    void Update(double deltaTime) override {
        if (done) return;

        Entity imgEntity = creditImage.GetEntity();
        if (imgEntity == 0) return;

        if (!NE::ECS::Query::HasUIRectTransform(imgEntity)) {
            // No UIRectTransform on credit image - skip move (e.g. wrong ref or 3D object)
            return;
        }

        float move = scrollSpeed * static_cast<float>(deltaTime);
        scrolled += move;

        // Move the credits image via UIRectTransform. Mutate y and mark layout dirty
        // so the engine recalculates. Decreasing y scrolls UP in this UI coordinate system.
        auto& rt = NE::ECS::Command::GetUIRectTransform(imgEntity);
        rt.y -= move;
        rt.worldMatrixDirty = true;
        rt.worldRectCached = false;

        if (scrolled >= totalScrollDist) {
            done = true;
            Events::Send("CreditsDone");
            LOG_INFO("Credits_Controller: done. Fired 'CreditsDone'.");
        }
    }

private:
    // ── Inspector fields ──────────────────────────────────────────────────
    GameObjectRef creditImage;
    float         scrollSpeed     = 100.0f;   // pixels per second
    float         totalScrollDist = 5080.0f;  // imageHeight + screenHeight

    // ── Runtime state ─────────────────────────────────────────────────────
    float scrolled = 0.0f;
    bool  done     = false;
};