#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "EngineAPI.hpp"

/*
    Multi-light sequencer puzzle (up to 9 lights via inspector slots).

    Behaviour:
      1) Plays a random sequence once (blink by swapping materials).
      2) Player repeats the sequence.
         - Keyboard: keys 1..9 (matching inspector slots light1..light9)
         - Click pads: call ReceiveInput(slotNumber) where slotNumber is 1..9

    Improvements:
      - Slot numbers are stable (1..9) even if you leave holes in the inspector.
        Example: you can assign light1, light3, light5 and it still works.
      - StartPuzzleIfIdle() helper for optional "start on first click".

    Blinking uses NE::Renderer::Command::AssignMaterial.
*/

class Puzzle_MultiLightSequencer : public IScript {
public:
    Puzzle_MultiLightSequencer() {
        // Light entities (drag from hierarchy)
        SCRIPT_GAMEOBJECT_REF(light1);
        SCRIPT_GAMEOBJECT_REF(light2);
        SCRIPT_GAMEOBJECT_REF(light3);
        SCRIPT_GAMEOBJECT_REF(light4);
        SCRIPT_GAMEOBJECT_REF(light5);
        SCRIPT_GAMEOBJECT_REF(light6);
        SCRIPT_GAMEOBJECT_REF(light7);
        SCRIPT_GAMEOBJECT_REF(light8);
        SCRIPT_GAMEOBJECT_REF(light9);

        // Materials (drag from asset browser)
        SCRIPT_FIELD(lightOnMaterial, MaterialRef);
        SCRIPT_FIELD(lightOffMaterial, MaterialRef);

        // Timing
        SCRIPT_FIELD(startDelay, Float);
        SCRIPT_FIELD(blinkOnTime, Float);
        SCRIPT_FIELD(blinkOffTime, Float);
        SCRIPT_FIELD(failReplayDelay, Float);

        // Sequence
        SCRIPT_FIELD(sequenceLength, Int);
        SCRIPT_FIELD(autoStart, Bool);
        SCRIPT_FIELD(logSequence, Bool);

        // Optional payload ID (you can ignore it in the listener if you want)
        SCRIPT_FIELD(puzzleKeyId, Int);
        SCRIPT_FIELD(solvedEventName, String);
    }

    ~Puzzle_MultiLightSequencer() override = default;

    // === Lifecycle ===
    void Awake() override {}
    void Initialize(Entity entity) override { (void)entity; }

    void Start() override {
        BuildSlotLists();

        if (m_numValidSlots < 2) {
            LOG_WARNING("[Sequencer] Need at least 2 assigned lights (light1..light9). Could not start.");
            m_state = State::Idle;
            return;
        }

        // Clamp/validate exposed values
        if (sequenceLength < 1) sequenceLength = 1;
        if (sequenceLength > 64) sequenceLength = 64; // sanity cap
        if (blinkOnTime < 0.01f) blinkOnTime = 0.01f;
        if (blinkOffTime < 0.01f) blinkOffTime = 0.01f;
        if (startDelay < 0.0f) startDelay = 0.0f;
        if (failReplayDelay < 0.0f) failReplayDelay = 0.0f;

        if (solvedEventName.empty()) solvedEventName = "PuzzleSolved";

        SetAllValidLights(false);

        if (autoStart) {
            BeginNewRound(true);
        }
    }

    void Update(double deltaTime) override {
        const float dt = static_cast<float>(deltaTime);

        // --- Show sequence (state machine) ---
        if (m_state == State::Starting) {
            m_timer -= dt;
            if (m_timer <= 0.0f) {
                m_state = State::ShowingOn;
                m_showIndex = 0;
                m_timer = blinkOnTime;
                ShowCurrentLight(true);
            }
            return;
        }

        if (m_state == State::ShowingOn) {
            m_timer -= dt;
            if (m_timer <= 0.0f) {
                ShowCurrentLight(false);
                m_state = State::ShowingOff;
                m_timer = blinkOffTime;
            }
            return;
        }

        if (m_state == State::ShowingOff) {
            m_timer -= dt;
            if (m_timer <= 0.0f) {
                ++m_showIndex;
                if (m_showIndex >= static_cast<int>(m_sequence.size())) {
                    // Done showing; wait for input
                    m_state = State::WaitingInput;
                    m_inputIndex = 0;
                    SetAllValidLights(false);
                }
                else {
                    m_state = State::ShowingOn;
                    m_timer = blinkOnTime;
                    ShowCurrentLight(true);
                }
            }
            return;
        }

        if (m_state == State::FailedDelay) {
            m_timer -= dt;
            if (m_timer <= 0.0f) {
                BeginNewRound(false); // replay same sequence
            }
            return;
        }

        // --- Input (keys 1..9) ---
        if (m_state == State::WaitingInput) {
            for (int k = 1; k <= 9; ++k) {
                if (Input::WasKeyPressed(static_cast<int>('0' + k))) {
                    ReceiveInput(k);
                    break; // accept one input per frame
                }
            }
        }
    }

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Puzzle_MultiLightSequencer"; }

    // === Collision Callbacks (required by IScript) ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    // Click-pads can call this to start the puzzle if autoStart=false.
    void StartPuzzleIfIdle(bool reseed) {
        if (m_state != State::Idle) return;
        if (m_numValidSlots < 2) return;
        BeginNewRound(reseed);
    }

    // Accepts either slotNumber 1..9 (recommended) or 0..8.
    void ReceiveInput(int input) {
        if (m_state != State::WaitingInput) return;
        if (m_sequence.empty()) return;

        // Convert to 0-based slot index
        int slot = input;
        if (slot >= 1 && slot <= 9) slot -= 1;
        if (slot < 0 || slot >= 9) return;
        if (!m_slots[slot].IsValid()) return;

        const int expectedSlot = m_sequence[m_inputIndex];
        const bool correct = (slot == expectedSlot);

        FlashFeedback(slot);

        if (!correct) {
            // "Unsolve" is a no-op here; we just replay the same sequence
            m_state = State::FailedDelay;
            m_timer = failReplayDelay;
            m_inputIndex = 0;
            return;
        }

        ++m_inputIndex;
        if (m_inputIndex >= static_cast<int>(m_sequence.size())) {
            SendSolvedEvent();
            m_state = State::Solved;
            SetAllValidLights(true);
        }
    }

private:
    enum class State {
        Idle,
        Starting,
        ShowingOn,
        ShowingOff,
        WaitingInput,
        FailedDelay,
        Solved,
    };

    void SendSolvedEvent() {
        const char* evt = solvedEventName.empty() ? "PuzzleSolved" : solvedEventName.c_str();
        void* payload = reinterpret_cast<void*>(static_cast<std::uintptr_t>(puzzleKeyId));
        //Events::Send(evt, payload); // this crashes - RF
        Events::Send(evt, &puzzleKeyId);
    }

    void BeginNewRound(bool reseed) {
        GenerateSequence(reseed);
        SetAllValidLights(false);
        m_state = State::Starting;
        m_timer = startDelay;
    }

    void BuildSlotLists() {
        std::array<GameObjectRef, 9> slots = {
            light1, light2, light3, light4, light5, light6, light7, light8, light9
        };

        m_slots = slots;
        m_validSlots.clear();
        m_validSlots.reserve(9);

        for (int s = 0; s < 9; ++s) {
            if (m_slots[s].IsValid()) {
                m_validSlots.push_back(s);
                m_defaultMaterialUUID[s] = NE::Renderer::Query::GetMaterial(m_slots[s].GetEntity());
            }
            else {
                m_defaultMaterialUUID[s].clear();
            }
        }

        m_numValidSlots = static_cast<int>(m_validSlots.size());
    }

    void GenerateSequence(bool reseed) {
        if (m_numValidSlots <= 0) return;

        if (reseed || !m_rngInitialized) {
            std::random_device rd;
            m_rng.seed(rd());
            m_rngInitialized = true;
        }

        std::uniform_int_distribution<int> dist(0, m_numValidSlots - 1);
        m_sequence.clear();
        m_sequence.reserve(static_cast<size_t>(sequenceLength));

        for (int i = 0; i < sequenceLength; ++i) {
            const int pick = dist(m_rng);
            m_sequence.push_back(m_validSlots[pick]); // store slot index 0..8
        }

        if (logSequence) {
            std::stringstream ss;
            ss << "[Sequencer] ValidSlots=" << m_numValidSlots << " Sequence: ";
            for (size_t i = 0; i < m_sequence.size(); ++i) {
                ss << (m_sequence[i] + 1);
                if (i + 1 < m_sequence.size()) ss << "-";
            }
            LOG_DEBUG(ss.str());
        }
    }

    void SetSlotLightState(int slotIndex, bool on) {
        if (slotIndex < 0 || slotIndex >= 9) return;
        if (!m_slots[slotIndex].IsValid()) return;

        const Entity e = m_slots[slotIndex].GetEntity();

        if (on) {
            if (lightOnMaterial.IsValid()) {
                NE::Renderer::Command::AssignMaterial(e, lightOnMaterial);
            }
            return;
        }

        // Off
        if (lightOffMaterial.IsValid()) {
            NE::Renderer::Command::AssignMaterial(e, lightOffMaterial);
        }
        else if (!m_defaultMaterialUUID[slotIndex].empty() && m_defaultMaterialUUID[slotIndex] != "empty uuid") {
            NE::Renderer::Command::AssignMaterial(e, m_defaultMaterialUUID[slotIndex]);
        }
    }

    void SetAllValidLights(bool on) {
        for (int slot : m_validSlots) {
            SetSlotLightState(slot, on);
        }
    }

    void ShowCurrentLight(bool on) {
        SetAllValidLights(false);
        if (m_showIndex < 0 || m_showIndex >= static_cast<int>(m_sequence.size())) return;
        SetSlotLightState(m_sequence[m_showIndex], on);
    }

    void FlashFeedback(int slotIndex) {
        if (slotIndex < 0 || slotIndex >= 9) return;
        if (!m_slots[slotIndex].IsValid()) return;

        Coroutines::Handle handle = Coroutines::Create();
        Coroutines::AddAction(handle, [this, slotIndex]() { SetSlotLightState(slotIndex, true); });
        Coroutines::AddWait(handle, 0.08f);
        Coroutines::AddAction(handle, [this, slotIndex]() {
            if (m_state == State::WaitingInput) SetSlotLightState(slotIndex, false);
            });
        Coroutines::Start(handle);
    }

private:
    // === Exposed fields ===
    GameObjectRef light1{}, light2{}, light3{}, light4{}, light5{}, light6{}, light7{}, light8{}, light9{};

    MaterialRef lightOnMaterial{};
    MaterialRef lightOffMaterial{};

    float startDelay = 0.5f;
    float blinkOnTime = 0.35f;
    float blinkOffTime = 0.20f;
    float failReplayDelay = 0.6f;

    int sequenceLength = 3;
    bool autoStart = true;
    bool logSequence = false;

    int puzzleKeyId = 1;
    std::string solvedEventName = "PuzzleSolved";

    // === Runtime ===
    std::array<GameObjectRef, 9> m_slots{};
    std::array<std::string, 9> m_defaultMaterialUUID{};
    std::vector<int> m_validSlots{}; // each entry is a slot index (0..8)
    int m_numValidSlots = 0;

    std::mt19937 m_rng{};
    bool m_rngInitialized = false;

    std::vector<int> m_sequence{}; // sequence of slot indices (0..8)
    State m_state = State::Idle;
    float m_timer = 0.0f;
    int m_showIndex = 0;
    int m_inputIndex = 0;
};
