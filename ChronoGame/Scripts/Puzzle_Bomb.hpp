#pragma once
#include "EngineAPI.hpp"
#include "Interactable_.hpp"

/*
 * Puzzle_Bomb
 *
 * Cipher wheel bomb defusal puzzle.
 * Outer ring rotates via scroll wheel when player has clicked to enter interaction mode.
 * 3 hardcoded pairs (letter index A=0..H=7, number index 1=0..8=7) checked each frame.
 * When all 3 pairs align simultaneously, sends solveMessage event.
 *
 * Hierarchy expected:
 *   Bomb Puzzle (this script on root entity)
 *     Inner     <- fixed, numbers 1-8
 *     Outer     <- rotates, letters A-H
 *     Center    <- decorative
 *
 * Setup:
 *   1. Attach this script to the Bomb Puzzle root entity
 *   2. Assign outerRef to the Outer child
 *   3. Set the 3 pairs in inspector (letterIndex 0-7, numberIndex 0-7)
 *   4. Set solveMessage (default "BombPuzzleSolved")
 *   5. Make sure root entity is on the raycast target layer with a collider
 */

class Puzzle_Bomb : public Interactable_ {
public:
    Puzzle_Bomb() {
        SCRIPT_GAMEOBJECT_REF(outerRef);

        // Pair 1
        SCRIPT_FIELD(pair1_Letter, Int);   // 0=A, 1=B ... 7=H
        SCRIPT_FIELD(pair1_Number, Int);   // 0=1, 1=2 ... 7=8

        // Pair 2
        SCRIPT_FIELD(pair2_Letter, Int);
        SCRIPT_FIELD(pair2_Number, Int);

        // Pair 3
        SCRIPT_FIELD(pair3_Letter, Int);
        SCRIPT_FIELD(pair3_Number, Int);

        SCRIPT_FIELD(solveMessage, String);
        SCRIPT_FIELD(rotateAudioName, String);  // e.g. "BOMB_ROTATE" (without event:/)
        SCRIPT_FIELD(interactAudioName, String); // oneshot on click, default "BOMB_INTERACT"
        SCRIPT_FIELD(tickAudioName, String);     // looping while active, default "BOMB_TICK"
        SCRIPT_FIELD(fuseAudioName, String);     // oneshot on solve, default "BOMB_FUSE"
    }

    ~Puzzle_Bomb() override = default;

    void Awake()    override {}
    void Initialize(Entity entity) override { Interactable_::Initialize(entity); }
    void OnDestroy()  override { StopTickAudio(); }
    void OnEnable()   override {}
    void OnDisable()  override { StopTickAudio(); }
    void OnValidate() override {}

    const char* GetTypeName() const override { return "Puzzle_Bomb"; }

    void Start() override {
        outerRotation = 0;
        inInteractionMode = false;
        isSolved = false;
        pair1_matched = false;
        pair2_matched = false;
        pair3_matched = false;

        // Snapshot the Outer ring's base local rotation once.
        // RotateOuter will only ever modify the Y component from this baseline,
        // keeping X and Z (e.g. the artist's -90 X tilt) untouched.
        if (outerRef.IsValid()) {
            outerBaseRot = TF_GetLocalRotation(outerRef.GetEntity());
            LOG_DEBUG("[Puzzle_Bomb] Outer base local rot: X=" + std::to_string(outerBaseRot.x)
                + " Y=" + std::to_string(outerBaseRot.y)
                + " Z=" + std::to_string(outerBaseRot.z));
        }

        LOG_DEBUG("[Puzzle_Bomb] Ready. Solve message: " + solveMessage);
        LOG_DEBUG("[Puzzle_Bomb] Pairs: " +
            PairLabel(pair1_Letter, pair1_Number) + ", " +
            PairLabel(pair2_Letter, pair2_Number) + ", " +
            PairLabel(pair3_Letter, pair3_Number));
    }

    // Called by Player_Raycast on left click
    void Interact() override {
        if (isSolved) return;
        inInteractionMode = !inInteractionMode;
        LOG_DEBUG(inInteractionMode
            ? "[Puzzle_Bomb] Entered interaction mode."
            : "[Puzzle_Bomb] Exited interaction mode.");

        // Oneshot click sound every time
        if (!interactAudioName.empty())
            PlayAudio("event:/" + interactAudioName);

        // Start looping tick when entering, stop when exiting
        if (inInteractionMode) {
            StartTickAudio();
        }
        else {
            StopTickAudio();
        }
    }

    void Update(double deltaTime) override {
        (void)deltaTime;

        if (!inInteractionMode) return;
        if (isSolved) return;

        // === Scroll to rotate outer wheel ===
        auto [scrollX, scrollY] = Input::GetScrollDelta();

        if (scrollY > 0.0) {
            RotateOuter(+1);
            //StartRotateAudio();
        }
        else if (scrollY < 0.0) {
            RotateOuter(-1);
            //StartRotateAudio();
        }
        else {
            //StopRotateAudio();
        }

        // === Check pairs every frame while in interaction mode ===
        CheckPairs();
    }

    // Collision callbacks
    void OnCollisionEnter(Entity o) override { (void)o; }
    void OnCollisionExit(Entity o)  override { (void)o; }
    void OnCollisionStay(Entity o)  override { (void)o; }
    void OnTriggerEnter(Entity o)   override { (void)o; }
    void OnTriggerExit(Entity o)    override { (void)o; }
    void OnTriggerStay(Entity o)    override { (void)o; }

private:
    // Inspector fields
    GameObjectRef outerRef;

    int pair1_Letter = 0;   // A
    int pair1_Number = 0;   // 1

    int pair2_Letter = 2;   // C
    int pair2_Number = 7;   // 8

    int pair3_Letter = 4;   // E
    int pair3_Number = 3;   // 4

    std::string solveMessage = "BombPuzzleSolved";

    // Runtime
    static const int SEGMENTS = 8;
    int  outerRotation = 0;
    bool inInteractionMode = false;
    bool isSolved = false;
    bool isRotateAudioPlaying = false;
    std::string rotateAudioName = "";
    std::string interactAudioName = "BOMB_INTERACT";
    std::string tickAudioName = "BOMB_TICK";
    std::string fuseAudioName = "BOMB_FUSE";
    bool isTickPlaying = false;

    bool pair1_matched = false;
    bool pair2_matched = false;
    bool pair3_matched = false;

    Vec3 outerBaseRot = { 0.0f, 0.0f, 0.0f }; // snapshotted in Start()

    // ----------------------------------------------------------

    void RotateOuter(int delta) {
        outerRotation = (outerRotation + delta + SEGMENTS) % SEGMENTS;

        if (outerRef.IsValid()) {
            // Absolute Y offset from base - never touches X or Z
            float zDegrees = outerBaseRot.z + outerRotation * (360.0f / SEGMENTS);
            TF_SetRotation(outerBaseRot.x, outerBaseRot.y, zDegrees, outerRef.GetEntity());
        }

        LOG_DEBUG("[Puzzle_Bomb] Scrolled! Step: " + std::to_string(outerRotation));
    }

    // Aligned index of a letter after applying outerRotation
    int AlignedIndex(int letterIndex) const {
        return (letterIndex + outerRotation) % SEGMENTS;
    }

    bool IsPairAligned(int letterIdx, int numberIdx) const {
        return AlignedIndex(letterIdx) == numberIdx;
    }

    void CheckPairs() {
        bool p1 = IsPairAligned(pair1_Letter, pair1_Number);
        bool p2 = IsPairAligned(pair2_Letter, pair2_Number);
        bool p3 = IsPairAligned(pair3_Letter, pair3_Number);

        // Log on state change only
        if (p1 != pair1_matched) {
            pair1_matched = p1;
            LOG_DEBUG(p1 ? "[Puzzle_Bomb] Pair 1 match!" : "[Puzzle_Bomb] Pair 1 lost.");
        }
        if (p2 != pair2_matched) {
            pair2_matched = p2;
            LOG_DEBUG(p2 ? "[Puzzle_Bomb] Pair 2 match!" : "[Puzzle_Bomb] Pair 2 lost.");
        }
        if (p3 != pair3_matched) {
            pair3_matched = p3;
            LOG_DEBUG(p3 ? "[Puzzle_Bomb] Pair 3 match!" : "[Puzzle_Bomb] Pair 3 lost.");
        }

        // Solve when all 3 align simultaneously
        if (p1 && p2 && p3 && !isSolved) {
            isSolved = true;
            inInteractionMode = false;
            //StopRotateAudio();
            StopTickAudio();
            if (!fuseAudioName.empty())
                PlayAudio("event:/" + fuseAudioName);
            LOG_DEBUG("[Puzzle_Bomb] ALL PAIRS MATCHED - Solved! Sending: " + solveMessage);
            Events::Send(solveMessage.c_str());
        }
    }

    void StartTickAudio() {
        if (isTickPlaying || tickAudioName.empty()) return;
        PlayAudio("event:/" + tickAudioName);
        isTickPlaying = true;
    }

    void StopTickAudio() {
        if (!isTickPlaying || tickAudioName.empty()) return;
        StopAudio("event:/" + tickAudioName);
        isTickPlaying = false;
    }

    void StartRotateAudio() {
        if (isRotateAudioPlaying || rotateAudioName.empty()) return;
        //PlayAudio("event:/" + rotateAudioName);
        //isRotateAudioPlaying = true;
    }

    void StopRotateAudio() {
        if (!isRotateAudioPlaying || rotateAudioName.empty()) return;
        //StopAudio("event:/" + rotateAudioName);
        //isRotateAudioPlaying = false;
    }

    std::string PairLabel(int letter, int number) const {
        char l = static_cast<char>('A' + letter);
        return std::string(1, l) + "-" + std::to_string(number + 1);
    }
};