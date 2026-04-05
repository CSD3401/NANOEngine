#pragma once
#include "EngineAPI.hpp"

/*
 * Puzzle_FinalBomb 
 * 
 * M5 LEGACY CODE DONT USE : RF
 * 
 *
 * Listens to all 8 required events. Once all are received,
 * triggers end game / cutscene.
 *
 * Events required:
 *   - FINALmirrorsolved
 *   - FINALsequencersolved
 *   - FINALwiresolved
 *   - bomb1solved
 *   - bomb2solved
 *   - bomb3solved
 *   - bomb4solved
 *   - bomb5solved
 */

class Puzzle_FinalBomb : public IScript {
public:
    Puzzle_FinalBomb() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_FIELD(hearingDistance, Float);
        SCRIPT_FIELD(finalBombBgm, String);
        SCRIPT_FIELD(heartbeatBgm, String);
    }
    ~Puzzle_FinalBomb() override = default;

    void Awake()    override {}
    void Initialize(Entity) override {}
    void OnDestroy() override { StopAllAudio(); }
    void OnEnable()  override {}
    void OnDisable() override { StopAllAudio(); }
    void OnValidate() override {}

    const char* GetTypeName() const override { return "Puzzle_FinalBomb"; }

    void Start() override {
        // Reset all flags
        isFinalMirrorSolved = false;
        isFinalSequencerSolved = false;
        isFinalWireSolved = false;
        isBomb1Solved = false;
        isBomb2Solved = false;
        isBomb3Solved = false;
        isBomb4Solved = false;
        isBomb5Solved = false;
        doOnce = false;
        isAudioPlaying = false;
        pendingSceneSwitch = false;

        if (hearingDistance <= 0.0f)
            hearingDistance = 5.0f;

        if (playerRef.IsValid())
            playerEntity = playerRef.GetEntity();

        Events::Listen("FINALmirrorsolved", [this](void*) { isFinalMirrorSolved = true; LogStatus(); });
        Events::Listen("FINALsequencersolved", [this](void*) { isFinalSequencerSolved = true; LogStatus(); });
        Events::Listen("FINALwiresolved", [this](void*) { isFinalWireSolved = true; LogStatus(); });
        Events::Listen("bomb1solved", [this](void*) { isBomb1Solved = true; LogStatus(); });
        Events::Listen("bomb2solved", [this](void*) { isBomb2Solved = true; LogStatus(); });
        Events::Listen("bomb3solved", [this](void*) { isBomb3Solved = true; LogStatus(); });
        Events::Listen("bomb4solved", [this](void*) { isBomb4Solved = true; LogStatus(); });
        Events::Listen("bomb5solved", [this](void*) { isBomb5Solved = true; LogStatus(); });

        LOG_DEBUG("[Puzzle_FinalBomb] Initialized. Waiting for all 8 events.");
    }

    void Update(double) override {
        if (doOnce) return;

        // Distance check - play/stop looping audio
        if (playerRef.IsValid()) {
            Vec3 diff = TF_GetPosition(playerEntity) - TF_GetPosition();
            float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            bool inRange = distSq <= (hearingDistance * hearingDistance);

            if (inRange && !isAudioPlaying) {
                PlayAudio("event:/" + finalBombBgm);
                PlayAudio("event:/" + heartbeatBgm);
                isAudioPlaying = true;
                LOG_DEBUG("[Puzzle_FinalBomb] Audio started.");
            }
            else if (!inRange && isAudioPlaying) {
                StopAudio("event:/" + finalBombBgm);
                StopAudio("event:/" + heartbeatBgm);
                isAudioPlaying = false;
                LOG_DEBUG("[Puzzle_FinalBomb] Audio stopped.");
            }
        }


        //if (isFinalMirrorSolved &&
        //    isFinalSequencerSolved &&
        //    isFinalWireSolved &&
        //    isBomb1Solved &&
        //    isBomb2Solved &&
        //    isBomb3Solved &&
        //    isBomb4Solved &&
        //    isBomb5Solved)
        //{
        //    doOnce = true;
        //    LOG_DEBUG("[Puzzle_FinalBomb] END GAME - PLAY CUTSCENE");
        //    // TODO: Events::Send("PlayEndCutscene");
        //}

        // For m5 submission
        if (isFinalMirrorSolved &&
            isFinalSequencerSolved)
        {
            doOnce = true;
            StopAllAudio();
            LOG_ERROR("[Puzzle_FinalBomb] END GAME - PLAY CUTSCENE");
            // TODO: Events::Send("PlayEndCutscene");
            pendingSceneSwitch = true;
        }

        if (pendingSceneSwitch) {
            pendingSceneSwitch = false;
            NE::Scripting::SwitchScene("7c7bd1dd-30c6-414a-8514-b045d3b54acd");
        }
    }

    void OnCollisionEnter(Entity o) override { (void)o; }
    void OnCollisionExit(Entity o)  override { (void)o; }
    void OnCollisionStay(Entity o)  override { (void)o; }
    void OnTriggerEnter(Entity o)   override { (void)o; }
    void OnTriggerExit(Entity o)    override { (void)o; }
    void OnTriggerStay(Entity o)    override { (void)o; }

private:
    bool isFinalMirrorSolved = false;
    bool isFinalSequencerSolved = false;
    bool isFinalWireSolved = false;
    bool isBomb1Solved = false;
    bool isBomb2Solved = false;
    bool isBomb3Solved = false;
    bool isBomb4Solved = false;
    bool isBomb5Solved = false;
    bool doOnce = false;
    bool isAudioPlaying = false;
    bool pendingSceneSwitch = false;
    float hearingDistance = 5.0f;
    std::string finalBombBgm = "FINAL_BOMB";
    std::string heartbeatBgm = "HEARTBEAT";
    GameObjectRef playerRef;
    Entity playerEntity;

    void StopAllAudio() {
        if (isAudioPlaying) {
            StopAudio("event:/" + finalBombBgm);
            StopAudio("event:/" + heartbeatBgm);
            isAudioPlaying = false;
        }
    }

    void LogStatus() {
        LOG_DEBUG("[Puzzle_FinalBomb] Status: "
            "Mirror=" + std::to_string(isFinalMirrorSolved) + " "
            "Sequencer=" + std::to_string(isFinalSequencerSolved) + " "
            "Wire=" + std::to_string(isFinalWireSolved) + " "
            "Bomb1=" + std::to_string(isBomb1Solved) + " "
            "Bomb2=" + std::to_string(isBomb2Solved) + " "
            "Bomb3=" + std::to_string(isBomb3Solved) + " "
            "Bomb4=" + std::to_string(isBomb4Solved) + " "
            "Bomb5=" + std::to_string(isBomb5Solved));
    }
};