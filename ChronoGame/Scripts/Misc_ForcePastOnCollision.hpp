#pragma once
#include "EngineAPI.hpp"
#include "Watch_Controller.hpp"

/*
* RF: THIS IS BASICALLY THE MANAGER FOR THE FINAL AREA
*
* Misc_ForcePastOnCollision
* When the player collides or triggers this object, the watch is locked into
* the past forever:
*   - switches to past immediately (if not already there)
*   - stops the drain / auto-return timer
*   - ignores manual Q toggles back to present
*
* Objective text display (requires objectiveTextRef assigned):
*   Mirror Puzzle    (Solved) / (Unsolved)
*   Wire Puzzle      (Solved) / (Unsolved)
*   Sequencer Puzzle (Solved) / (Unsolved)
*   Bomb Defused     X/5
*
* Setup:
* 1. Attach this script to the collision/trigger object.
* 2. Assign playerRef to the player entity.
* 3. Assign watchRef to the entity that has Watch_Controller.
* 4. Assign objectiveTextRef to a UIText entity in the HUD.
*/
class Misc_ForcePastOnCollision : public IScript {
public:
    Misc_ForcePastOnCollision() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(watchRef);
        SCRIPT_FIELD(disableAfterTrigger, Bool);
        SCRIPT_FIELD(refillClockToFull, Bool);
        SCRIPT_GAMEOBJECT_REF(objectiveTextRef);
    }

    ~Misc_ForcePastOnCollision() override = default;

    void Awake() override {
        CacheWatch();
    }

    void Initialize(Entity entity) override {
        (void)entity;
    }

    void Start() override {
        CacheWatch();
        BindPuzzleEvents();
        UpdateObjectiveText();
    }

    void Update(double deltaTime) override {
        (void)deltaTime;
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {
        CacheWatch();
    }

    const char* GetTypeName() const override {
        return "Misc_ForcePastOnCollision";
    }

    void OnCollisionEnter(Entity other) override {
        HandlePlayerEnter(other);
    }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }

    void OnTriggerEnter(Entity other) override {
        HandlePlayerEnter(other);
    }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef playerRef;
    GameObjectRef watchRef;
    GameObjectRef objectiveTextRef;

    Watch_Controller* watch = nullptr;
    bool disableAfterTrigger = true;
    bool refillClockToFull = true;
    bool hasTriggered = false;

    // Puzzle state — all 8 must be true to fire playFinalCutscene
    bool m_mirrorSolved = false;
    bool m_wireSolved = false;
    bool m_sequencerSolved = false;
    bool m_bomb1Solved = false;
    bool m_bomb2Solved = false;
    bool m_bomb3Solved = false;
    bool m_bomb4Solved = false;
    bool m_bomb5Solved = false;
    bool m_cutsceneFired = false;

    // ── Event binding ─────────────────────────────────────────────
    void BindPuzzleEvents() {
        Events::Listen("FINALmirrorsolved", [this](void*) {
            m_mirrorSolved = true;
            UpdateObjectiveText();
            CheckAllSolved();
            });
        Events::Listen("FINALwiresolved", [this](void*) {
            m_wireSolved = true;
            UpdateObjectiveText();
            CheckAllSolved();
            });
        Events::Listen("FINALsequencersolved", [this](void*) {
            m_sequencerSolved = true;
            UpdateObjectiveText();
            CheckAllSolved();
            });
        //Events::Listen("bomb1solved", [this](void*) { m_bomb1Solved = true; UpdateObjectiveText(); CheckAllSolved(); });
        //Events::Listen("bomb2solved", [this](void*) { m_bomb2Solved = true; UpdateObjectiveText(); CheckAllSolved(); });
        //Events::Listen("bomb3solved", [this](void*) { m_bomb3Solved = true; UpdateObjectiveText(); CheckAllSolved(); });
        //Events::Listen("bomb4solved", [this](void*) { m_bomb4Solved = true; UpdateObjectiveText(); CheckAllSolved(); });
        //Events::Listen("bomb5solved", [this](void*) { m_bomb5Solved = true; UpdateObjectiveText(); CheckAllSolved(); });
    }

    // ── Win condition ────────────────────────────────────────────
    void CheckAllSolved() {
        if (m_cutsceneFired) return;
        if (m_mirrorSolved && m_wireSolved && m_sequencerSolved) {
            m_cutsceneFired = true;
            LOG_INFO("Misc_ForcePastOnCollision: All puzzles solved - sending playFinalCutscene");
            DeferEvent("playFinalCutscene");
        }
    }

    void DeferEvent(const char* eventName) {
        std::string name(eventName);
        Coroutines::Handle h = Coroutines::Create();
        Coroutines::AddWait(h, 0.0f);
        Coroutines::AddAction(h, [name]() {
            Events::Send(name.c_str());
            });
        Coroutines::Start(h);
    }

    // ── Objective text ────────────────────────────────────────────
    void UpdateObjectiveText() {
        if (!objectiveTextRef.IsValid()) return;
        const Entity textEntity = objectiveTextRef.GetEntity();
        if (textEntity == 0) return;

        std::string mirror = std::string("Mirror Puzzle    ") + (m_mirrorSolved ? "(Solved)" : "(Unsolved)");
        std::string wire = std::string("Wire Puzzle      ") + (m_wireSolved ? "(Solved)" : "(Unsolved)");
        std::string sequencer = std::string("Sequencer Puzzle ") + (m_sequencerSolved ? "(Solved)" : "(Unsolved)");
        int bombCount = (int)m_bomb1Solved + (int)m_bomb2Solved + (int)m_bomb3Solved + (int)m_bomb4Solved + (int)m_bomb5Solved;
        std::string bombs = std::string("Bomb Defused     ") + std::to_string(bombCount) + "/5";

        std::string display = mirror + "\n" + wire + "\n" + sequencer + "\n" + bombs;

        NE::Scripting::SetUIText(textEntity, display.c_str());
    }

    // ── Watch cache ───────────────────────────────────────────────
    void CacheWatch() {
        if (!watchRef.IsValid()) {
            watch = nullptr;
            return;
        }

        watch = GameObject(watchRef).GetComponent<Watch_Controller>();
        if (!watch) {
            LOG_WARNING("Misc_ForcePastOnCollision: watchRef has no Watch_Controller");
        }
    }

    // ── Trigger handler ───────────────────────────────────────────
    void HandlePlayerEnter(Entity other) {
        if (hasTriggered) {
            return;
        }

        if (!IsActiveInHierarchy()) {
            return;
        }

        if (!playerRef.IsValid()) {
            LOG_WARNING("Misc_ForcePastOnCollision: missing playerRef");
            return;
        }

        if (other != playerRef.GetEntity()) {
            return;
        }

        if (!watch) {
            CacheWatch();
        }

        if (!watch) {
            LOG_WARNING("Misc_ForcePastOnCollision: Watch_Controller not found");
            return;
        }

        watch->ForcePastForever(refillClockToFull);
        hasTriggered = true;

        LOG_INFO("Misc_ForcePastOnCollision: Player entered trigger - forcing permanent past state");

        if (disableAfterTrigger) {
            SetActive(false, GetEntity());
        }
    }
};