#pragma once
#include <string>
#include <array>
#include <vector>
#include <random>
#include "EngineAPI.hpp"

/**
 * MaterialSequencer (Event-driven, no attached transforms)
 * - Uses explicit entity IDs for 5 buttons
 * - Listens to "OnCameraRaycastHit" OR number keys 1..5
 * - Correct press: set that button's X rotation to the NEGATIVE of current X (e.g., 45 -> -45)
 * - Fail: set ALL button X back to +45 and targets to materialA
 * - Success: APPLY successMaterial to targets AND set ALL buttons to -45, then PERMANENTLY LOCK (no further resets)
 */
class MaterialSequencer : public IScript {
public:
    MaterialSequencer() {
        // Exposed fields
        SCRIPT_FIELD(isActive, Bool);
        SCRIPT_FIELD(autoRun, Bool);
        SCRIPT_FIELD(delayBetween, Float);
        SCRIPT_COMPONENT_REF(materialA, MaterialRef);
        SCRIPT_COMPONENT_REF(materialB, MaterialRef);
        SCRIPT_COMPONENT_REF(successMaterial, MaterialRef);
        SCRIPT_FIELD(solvedEventName, String);

        // Targets that get flashed
        SCRIPT_COMPONENT_REF(target1, TransformRef);
        SCRIPT_COMPONENT_REF(target2, TransformRef);
        SCRIPT_COMPONENT_REF(target3, TransformRef);
        SCRIPT_COMPONENT_REF(target4, TransformRef);
        SCRIPT_COMPONENT_REF(target5, TransformRef);

        // Buttons (entity IDs) that are pressed in sequence
        SCRIPT_FIELD(button1Entity, Int);
        SCRIPT_FIELD(button2Entity, Int);
        SCRIPT_FIELD(button3Entity, Int);
        SCRIPT_FIELD(button4Entity, Int);
        SCRIPT_FIELD(button5Entity, Int);
    }

    // === IScript required ===
    void Initialize(Entity) override {
        if (delayBetween <= 0.f) delayBetween = 0.25f;
        if (autoRun && !m_hasQueued && !m_solved) QueueSequence();

        // Listen to camera raycast hit
        Events::Listen("OnCameraRaycastHit", [this](void* payload) {
            if (!isActive || m_solved || !m_waitingForClicks || !payload) return;
            auto* data = static_cast<std::pair<uint32_t, uint32_t>*>(payload);
            uint32_t hit = data->first;
            int pressed = MapEntityToButtonIndex(hit); // 1..5, 0 if not ours
            if (pressed > 0) HandleKey(pressed);
            });
    }

    void Update(double) override {
        if (!isActive || m_solved) return;
        // Number keys for test input
        if (Input::WasKeyPressed('1')) HandleKey(1);
        if (Input::WasKeyPressed('2')) HandleKey(2);
        if (Input::WasKeyPressed('3')) HandleKey(3);
        if (Input::WasKeyPressed('4')) HandleKey(4);
        if (Input::WasKeyPressed('5')) HandleKey(5);

        if (!m_hasQueued && !m_waitingForClicks && autoRun && !m_solved) QueueSequence();
    }

    // Satisfy pure virtuals (unused here)
    void OnCollisionEnter(Entity) override {}
    void OnCollisionExit(Entity) override {}
    void OnTriggerEnter(Entity) override {}
    void OnTriggerExit(Entity) override {}
    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}

    // === Designer-set fields ===
    bool isActive = true;
    bool autoRun = false;
    float delayBetween = 0.25f;
    MaterialRef materialA{};
    MaterialRef materialB{};
    MaterialRef successMaterial{};
    std::string solvedEventName = "MaterialSequencerSolved";

    TransformRef target1{}, target2{}, target3{}, target4{}, target5{};

    // Button entity IDs
    int button1Entity = 0;
    int button2Entity = 0;
    int button3Entity = 0;
    int button4Entity = 0;
    int button5Entity = 0;

private:
    // Internal state
    bool m_hasQueued = false;
    bool m_waitingForClicks = false;
    bool m_solved = false;              // <-- lock flag after success
    int  m_clickIndex = 0;
    std::array<int, 5> m_order{ -1, -1, -1, -1, -1 };

    std::array<TransformRef, 5> GetTargets() const {
        return { target1, target2, target3, target4, target5 };
    }
    std::array<int, 5> GetButtons() const {
        return { button1Entity, button2Entity, button3Entity, button4Entity, button5Entity };
    }

    int MapEntityToButtonIndex(uint32_t entity) const {
        auto ids = GetButtons();
        for (int i = 0; i < 5; ++i) if (ids[i] != 0 && (uint32_t)ids[i] == entity) return i + 1; // 1-based
        return 0;
    }

    // Helpers for button rotations
    void SetButtonXDeg(int idx0, float xDeg) {
        auto ids = GetButtons();
        if (idx0 < 0 || idx0 >= 5) return;
        uint32_t e = (uint32_t)ids[idx0];
        if (e == 0) return;
        Vec3 r = GetRotation((Entity)e);
        SetRotation(Vec3(xDeg, r.y, r.z), (Entity)e);
    }
    void NegateButtonX(int idx0) {
        auto ids = GetButtons();
        if (idx0 < 0 || idx0 >= 5) return;
        uint32_t e = (uint32_t)ids[idx0];
        if (e == 0) return;
        Vec3 r = GetRotation((Entity)e);
        SetRotation(Vec3(-std::abs(r.x), r.y, r.z), (Entity)e); // 45->-45, -30 stays -30
    }
    void SetAllButtonsXTo(float xDeg) {
        auto ids = GetButtons();
        for (int i = 0; i < 5; ++i) {
            if (ids[i] != 0) {
                uint32_t e = (uint32_t)ids[i];
                Vec3 r = GetRotation((Entity)e);
                SetRotation(Vec3(xDeg, r.y, r.z), (Entity)e);
            }
        }
    }
    void SetAllButtonsXToNeg45() { SetAllButtonsXTo(-45.0f); }

    void QueueSequence() {
        m_hasQueued = true;
        m_waitingForClicks = false;
        m_clickIndex = 0;

        // Build list of valid target indices
        auto trefs = GetTargets();
        std::vector<int> idx;
        idx.reserve(5);
        for (int i = 0; i < 5; ++i) if (trefs[i].IsValid() && trefs[i].GetEntity() != 0) idx.push_back(i);
        if (idx.empty()) { m_hasQueued = false; return; }

        // Shuffle the order
        std::random_device rd; std::mt19937 gen(rd());
        std::shuffle(idx.begin(), idx.end(), gen);
        m_order.fill(-1);
        for (size_t i = 0; i < idx.size(); ++i) m_order[i] = idx[i];

        // Flash B one-by-one
        Coroutines::Handle h = Coroutines::Create();
        for (size_t step = 0; step < idx.size(); ++step) {
            int i = m_order[step];
            Entity e = trefs[i].GetEntity();
            Coroutines::AddAction(h, [e, b = materialB]() { NE::Renderer::Command::AssignMaterial(e, b); });
            Coroutines::AddWait(h, delayBetween);
        }

        // Reset all targets to A
        Coroutines::AddAction(h, [trefs, a = materialA]() {
            for (const auto& ref : trefs) {
                if (ref.IsValid()) {
                    Entity e = ref.GetEntity();
                    if (e != 0) NE::Renderer::Command::AssignMaterial(e, a);
                }
            }
            });

        // Begin input
        Coroutines::AddAction(h, [this]() { if (!m_solved) { m_waitingForClicks = true; m_clickIndex = 0; } });
    }

    int CountOrder() const { int n = 0; for (int i = 0; i < 5; ++i) if (m_order[i] >= 0) ++n; return n; }

    // Handle 1..5 press (from event or keyboard)
    void HandleKey(int pressedIdx1Based) {
        PlayAudio("event:/SEQUENCER_CLICK");
        if (m_solved) return;
        if (!m_waitingForClicks) return;
        if (m_clickIndex < 0 || m_clickIndex >= 5 || m_order[m_clickIndex] < 0) { FailAndReset(); return; }

        int expectedIdx = m_order[m_clickIndex];  // 0..4
        int pressedIdx0 = pressedIdx1Based - 1;   // 0..4

        // Ensure pressed index is one of the randomized valid ones
        bool valid = false;
        for (int i = 0; i < 5; ++i) if (m_order[i] == pressedIdx0) { valid = true; break; }
        if (!valid) { FailAndReset(); return; }

        if (pressedIdx0 == expectedIdx) {
            // Rotate the BUTTON entity's X to negative of its current X
            NegateButtonX(pressedIdx0);

            m_clickIndex++;
            if (m_clickIndex >= CountOrder()) {
                // Success: paint success material and lock state
                if (successMaterial.IsValid()) {
                    auto trefs = GetTargets();
                    for (const auto& ref : trefs) {
                        if (ref.IsValid()) {
                            Entity e2 = ref.GetEntity();
                            if (e2 != 0) NE::Renderer::Command::AssignMaterial(e2, successMaterial);
                        }
                    }
                }
                // Force all buttons to -45 and lock
                SetAllButtonsXToNeg45();
                Events::Send(solvedEventName.c_str(), nullptr);
                PlayAudio("event:/VOICEOVER4");

                m_waitingForClicks = false;
                m_hasQueued = false;
                m_solved = true; // <-- LOCK
            }
        }
        else {
            FailAndReset();
        }
    }

    void FailAndReset() {
        if (m_solved) return; // Do nothing after solved
        // Reset ALL button X rotations to +45, keep Y/Z as-is
        SetAllButtonsXTo(45.0f);

        // Reset target materials to A
        auto trefs = GetTargets();
        for (const auto& ref : trefs) {
            if (ref.IsValid()) {
                Entity e = ref.GetEntity();
                if (e != 0) NE::Renderer::Command::AssignMaterial(e, materialA);
            }
        }
        m_waitingForClicks = false;
        m_hasQueued = false;
        m_clickIndex = 0;
    }
};
