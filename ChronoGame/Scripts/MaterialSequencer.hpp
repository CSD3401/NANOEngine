#pragma once
#include <string>
#include <array>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include "EngineAPI.hpp"

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

        SCRIPT_COMPONENT_REF(target1, TransformRef);
        SCRIPT_COMPONENT_REF(target2, TransformRef);
        SCRIPT_COMPONENT_REF(target3, TransformRef);
        SCRIPT_COMPONENT_REF(target4, TransformRef);
        SCRIPT_COMPONENT_REF(target5, TransformRef);

        SCRIPT_COMPONENT_REF(attached1, TransformRef);
        SCRIPT_COMPONENT_REF(attached2, TransformRef);
        SCRIPT_COMPONENT_REF(attached3, TransformRef);
        SCRIPT_COMPONENT_REF(attached4, TransformRef);
        SCRIPT_COMPONENT_REF(attached5, TransformRef);

        // Ray setup
        SCRIPT_COMPONENT_REF(clickRayOrigin, TransformRef);  // usually the Camera
        SCRIPT_COMPONENT_REF(rayParent, TransformRef);       // set to Player if camera is a child
        SCRIPT_FIELD(rayDistance, Float);
        SCRIPT_FIELD(layerMask, Int);
    }

    void Initialize(Entity) override {
        if (rayDistance <= 0.f) rayDistance = 100.f;
        if (layerMask == 0) layerMask = -1; // default to "all"
        if (autoRun && !m_hasQueued) QueueSequence();
    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        // what am I looking at debug
        DebugPrintLook(deltaTime);

        // Start a round
        if (!m_hasQueued && !m_waitingForClicks && Input::WasKeyPressed('M')) {
            QueueSequence();
        }

        // Click validation
        if (m_waitingForClicks && Input::WasMousePressed(0)) {
            HandleClick();
        }

        // Keys 1..5 (optional)
        if (m_waitingForClicks) {
            if (Input::WasKeyPressed('1')) HandleKey(1);
            else if (Input::WasKeyPressed('2')) HandleKey(2);
            else if (Input::WasKeyPressed('3')) HandleKey(3);
            else if (Input::WasKeyPressed('4')) HandleKey(4);
            else if (Input::WasKeyPressed('5')) HandleKey(5);
        }
    }

    void Activate() {
        if (!m_hasQueued && !m_waitingForClicks) QueueSequence();
    }

    // Unused events
    void OnCollisionEnter(Entity) override {}
    void OnCollisionExit(Entity) override {}
    void OnTriggerEnter(Entity) override {}
    void OnTriggerExit(Entity) override {}

private:
    // === Exposed Fields ===
    bool isActive = true;
    bool autoRun = false;
    float delayBetween = 0.25f;
    MaterialRef materialA{};
    MaterialRef materialB{};
    MaterialRef successMaterial{};

    TransformRef target1{}, target2{}, target3{}, target4{}, target5{};
    TransformRef attached1{}, attached2{}, attached3{}, attached4{}, attached5{};
    TransformRef clickRayOrigin{};
    TransformRef rayParent{};                 // <-- NEW
    float rayDistance = 100.f;
    int   layerMask = -1; // -1 => 0xFFFFFFFF
    std::string solvedEventName = "MaterialSequencerSolved";
    // === Internal ===
    bool m_hasQueued = false;
    bool m_waitingForClicks = false;
    int  m_clickIndex = 0;
    std::array<int, 5> m_order{};
    std::vector<Entity> m_targetsCache;
    std::array<Vec3, 5> m_attachedOriginalRot{};
    std::array<bool, 5> m_attachedIsRotated{};

    // debug look-at throttling
    bool   m_debugPrintAim = true;
    double m_lookTimer = 0.0;
    double m_lookPrintEvery = 0.15; // seconds
    int    m_lastLookEntity = -2;   // -2 uninitialized, -1 no-hit, >=0 entity id

    std::array<TransformRef, 5> GetTargets() const {
        return { target1, target2, target3, target4, target5 };
    }
    std::array<TransformRef, 5> GetAttached() const {
        return { attached1, attached2, attached3, attached4, attached5 };
    }

    // --- helpers: build WORLD ray when camera is parented ---
    static inline float DegToRad(float v) { return v * 0.017453292519943295f; }

    // Engine's own GetForward() uses: y = -sin(pitch), z = -cos(pitch)*cos(yaw)
    Vec3 ForwardFromEuler(float pitchDeg, float yawDeg) {
        float pitch = DegToRad(pitchDeg);
        float yaw = DegToRad(yawDeg);
        Vec3 fwd;
        fwd.x = std::cos(pitch) * std::sin(yaw);
        fwd.y = -std::sin(pitch);
        fwd.z = -std::cos(pitch) * std::cos(yaw);
        return fwd.Normalized();
    }

    // Compose a world forward when camera is a child:
    // - yaw = parent.yaw + camera.localYaw (covers both styles)
    // - pitch = camera.localPitch (typical FPS)
    Vec3 ComputeRayDir() {
        if (!clickRayOrigin.IsValid()) return GetForward().Normalized();

        Vec3 camEuler = GetRotation(clickRayOrigin); // LOCAL from SDK
        float pitch = camEuler.x;
        float yaw = camEuler.y;

        if (rayParent.IsValid()) {
            Vec3 parentEuler = GetRotation(rayParent); // LOCAL for parent (often == world if parent has no parent)
            yaw = parentEuler.y + yaw;                 // combine
        }
        return ForwardFromEuler(pitch, yaw);
    }

    // Compose a world origin when camera is a child:
    // worldOrigin = parentPos + RotY(parentYaw) * cameraLocalPos
    // (Assumes parent has no parent and no non-uniform scale; fits common player/camera rigs)
    Vec3 ComputeRayOrigin() {
        if (!clickRayOrigin.IsValid())
            return GetPosition();

        Vec3 camLocalPos = GetPosition(clickRayOrigin); // LOCAL
        if (!rayParent.IsValid())
            return camLocalPos; // if camera is root, local == world

        Vec3 parentPos = GetPosition(rayParent);      // LOCAL for parent (root => world)
        Vec3 parentEuler = GetRotation(rayParent);
        float yaw = DegToRad(parentEuler.y);

        // rotate local offset by parent yaw around Y
        float sx = std::sin(yaw), cx = std::cos(yaw);
        Vec3 rotated(
            camLocalPos.x * cx + camLocalPos.z * sx,
            camLocalPos.y,
            -camLocalPos.x * sx + camLocalPos.z * cx
        );
        return parentPos + rotated;
    }

    // “what am I looking at” debug
    void DebugPrintLook(double dt) {
        if (!m_debugPrintAim) return;
        m_lookTimer += dt;
        if (m_lookTimer < m_lookPrintEvery) return;
        m_lookTimer = 0.0;

        if (!clickRayOrigin.IsValid()) {
            LOG_WARNING("LOOK: clickRayOrigin is invalid (assign Camera Transform)");
            return;
        }

        Vec3 origin = ComputeRayOrigin();
        Vec3 fwd = ComputeRayDir();

        // small bias to avoid starting inside player collider
        origin = origin + fwd * 0.1f;

        uint32_t mask = (layerMask < 0) ? 0xFFFFFFFFu : static_cast<uint32_t>(layerMask);
        float dist = (rayDistance > 0 ? rayDistance : 100.f);
        RaycastHit hit = Raycast(origin, fwd, dist, mask);

        int id = hit.hasHit ? static_cast<int>(hit.entity) : -1;
        if (id != m_lastLookEntity) {
            if (hit.hasHit) {
                LOG_INFO("LOOK: entity=" << id
                    << "  origin=(" << origin.x << "," << origin.y << "," << origin.z << ")"
                    << "  dir=(" << fwd.x << "," << fwd.y << "," << fwd.z << ")"
                    << "  dist=" << dist << "  mask=" << (unsigned)mask);
            }
            else {
                LOG_INFO("LOOK: no hit  origin=(" << origin.x << "," << origin.y << "," << origin.z << ")"
                    << "  dir=(" << fwd.x << "," << fwd.y << "," << fwd.z << ")"
                    << "  dist=" << dist << "  mask=" << (unsigned)mask);
            }
            m_lastLookEntity = id;
        }
    }

    void QueueSequence() {
        m_hasQueued = true;
        m_waitingForClicks = false;
        m_clickIndex = 0;
        m_targetsCache.clear();

        auto trefs = GetTargets();
        for (const auto& r : trefs) {
            if (r.IsValid()) {
                Entity e = r.GetEntity();
                if (e != 0) m_targetsCache.push_back(e);
            }
        }
        if (m_targetsCache.empty()) { m_hasQueued = false; return; }

        
        // Cache original rotations of attached switches and clear rotation flags
        {
            auto attached = GetAttached();
            for (int i = 0; i < 5; ++i) {
                if (attached[i].IsValid()) {
                    m_attachedOriginalRot[i] = GetRotation(attached[i]);
                } else {
                    m_attachedOriginalRot[i] = Vec3(0.f,0.f,0.f);
                }
                m_attachedIsRotated[i] = false;
            }
        }
std::vector<int> idx;
        for (int i = 0; i < 5; ++i)
            if (trefs[i].IsValid() && trefs[i].GetEntity() != 0) idx.push_back(i);

        std::random_device rd; std::mt19937 gen(rd());
        std::shuffle(idx.begin(), idx.end(), gen);

        m_order.fill(-1);
        for (size_t i = 0; i < idx.size(); ++i) m_order[i] = idx[i];

        Coroutines::Handle h = Coroutines::Create();

        for (size_t step = 0; step < idx.size(); ++step) {
            int i = m_order[step];
            Entity e = trefs[i].GetEntity();
            Coroutines::AddAction(h, [e, b = materialB]() { NE::Renderer::Command::AssignMaterial(e, b); });
            Coroutines::AddWait(h, delayBetween);
        }

        Coroutines::AddAction(h, [trefs, a = materialA]() {
            for (const auto& ref : trefs) {
                if (ref.IsValid()) {
                    Entity e = ref.GetEntity();
                    if (e != 0) NE::Renderer::Command::AssignMaterial(e, a);
                }
            }
            });

        Coroutines::AddAction(h, [this]() {
            m_waitingForClicks = true;
            m_clickIndex = 0;
            });
    }

    // ====== SINGLE-RAYCAST CLICK CHECK ======
    void HandleClick() {
        if (!clickRayOrigin.IsValid() || clickRayOrigin.GetEntity() == 0) {
            LOG_WARNING("MaterialSequencer: clickRayOrigin not set/invalid");
            FailAndReset(); return;
        }

        Vec3 origin = ComputeRayOrigin();
        Vec3 fwd = ComputeRayDir();
        origin = origin + fwd * 0.1f; // bias out of player

        uint32_t mask = (layerMask < 0) ? 0xFFFFFFFFu : static_cast<uint32_t>(layerMask);
        float dist = (rayDistance > 0 ? rayDistance : 100.f);

        RaycastHit hit = Raycast(origin, fwd, dist, mask);
        if (!hit.hasHit) {
            LOG_INFO("Raycast: no hit (origin=" << origin.x << "," << origin.y << "," << origin.z
                << "  dir=" << fwd.x << "," << fwd.y << "," << fwd.z
                << "  dist=" << dist << "  mask=" << (unsigned)mask << ")");
            FailAndReset(); return;
        }

        // Validate order index
        if (m_clickIndex < 0 || m_clickIndex >= 5 || m_order[m_clickIndex] < 0) {
            LOG_WARNING("Click phase: invalid m_clickIndex=" << m_clickIndex
                << "  order=" << m_order[0] << "," << m_order[1] << ","
                << m_order[2] << "," << m_order[3] << "," << m_order[4]);
            FailAndReset(); return;
        }

        // Expected attached entity for this step
        auto attached = GetAttached();
        int expectedIdx = m_order[m_clickIndex];
        Entity expected = (attached[expectedIdx].IsValid()) ? attached[expectedIdx].GetEntity() : 0;
        if (expected == 0) {
            LOG_WARNING("Attached block not set/valid for expectedIdx=" << expectedIdx);
            FailAndReset(); return;
        }

        LOG_INFO("Raycast hit entity=" << (int)hit.entity
            << "  expected=" << (int)expected
            << "  step=" << m_clickIndex
            << "  idx=" << expectedIdx);

        if (hit.entity == expected) {
            // Rotate the clicked attached switch by +180 degrees around Y to indicate activation
            if (attached[expectedIdx].IsValid()) {
                if (!m_attachedIsRotated[expectedIdx]) {
                    Vec3 r = GetRotation(attached[expectedIdx]);
                    SetRotation(attached[expectedIdx], Vec3(r.x, r.y + 180.f, r.z));
                    m_attachedIsRotated[expectedIdx] = true;
                }
            }
            m_clickIndex++;
            if (m_clickIndex >= CountOrder()) {
                // Apply succeed material to all targets
                if (successMaterial.IsValid()) {
                    auto trefs = GetTargets();
                    for (const auto& ref : trefs) {
                        if (ref.IsValid()) {
                            Entity e2 = ref.GetEntity();
                            if (e2 != 0) NE::Renderer::Command::AssignMaterial(e2, successMaterial);
                        }
                    }
                }
                LOG_INFO("i pass");
                Events::Send(solvedEventName.c_str(), nullptr);
                m_waitingForClicks = false;
                m_hasQueued = false;
            }
        }
        else {
            FailAndReset();
        }
    }
    // =======================================

    void HandleKey(int pressedIdx1Based) {
        if (!m_waitingForClicks) return;
        if (pressedIdx1Based < 1 || pressedIdx1Based > 5) return;

        if (m_clickIndex < 0 || m_clickIndex >= 5 || m_order[m_clickIndex] < 0) {
            FailAndReset(); return;
        }
        int expectedIdx = m_order[m_clickIndex];
        int pressedIdx0 = pressedIdx1Based - 1;

        bool isValidIndex = false;
        for (int i = 0;i < 5;++i) if (m_order[i] == pressedIdx0) { isValidIndex = true; break; }
        if (!isValidIndex) { FailAndReset(); return; }

        if (pressedIdx0 == expectedIdx) {
            m_clickIndex++;
            if (m_clickIndex >= CountOrder()) {
                // Apply succeed material to all targets
                if (successMaterial.IsValid()) {
                    auto trefs = GetTargets();
                    for (const auto& ref : trefs) {
                        if (ref.IsValid()) {
                            Entity e2 = ref.GetEntity();
                            if (e2 != 0) NE::Renderer::Command::AssignMaterial(e2, successMaterial);
                        }
                    }
                }
                Events::Send(solvedEventName.c_str(), nullptr); 
                LOG_INFO("i pass");
                m_waitingForClicks = false;
                m_hasQueued = false;
            }
        }
        else {
            FailAndReset();
        }
    }

    int CountOrder() const {
        int n = 0; for (int i = 0;i < 5;++i) if (m_order[i] >= 0) ++n; return n;
    }

    void FailAndReset() {
        // Revert all attached switches back to original rotation
        {
            auto attached = GetAttached();
            for (int i = 0; i < 5; ++i) {
                if (attached[i].IsValid()) {
                    SetRotation(attached[i], m_attachedOriginalRot[i]);
                }
                m_attachedIsRotated[i] = false;
            }
        }
        LOG_INFO("fail");
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
