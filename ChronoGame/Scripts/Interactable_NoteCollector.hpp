#pragma once
#include "EngineAPI.hpp"
#include "Interactable_.hpp"
#include <vector>

/*
* Interactable_NoteCollector
* Finds closest note within range and moves it to camera for reading.
*/

class Interactable_NoteCollector : public Interactable_ {
public:
    Interactable_NoteCollector() = default;
    ~Interactable_NoteCollector() override = default;

    // === Lifecycle Methods ===
    void Awake() override {}

    void Initialize(Entity entity) override {
        SCRIPT_GAMEOBJECT_REF(notesParentRef);
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(cameraRef);
        SCRIPT_FIELD(pickupRange, Float);
        SCRIPT_FIELD(noteOffsetZ, Float);
        SCRIPT_FIELD(matchCameraRotation, Bool);
    }

    void Start() override {
        InitializeNotesCache();
    }

    void Update(double deltaTime) override {
        if (!isViewing) {
            return;
        }
        MoveCurrentNoteToCamera();
    }

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Interactable_NoteCollector";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

    void ToggleClosestNote() {
        if (isViewing) {
            RestoreCurrentNote();
            return;
        }
        BeginViewClosestNote();
    }

    void Interact() override {
        ToggleClosestNote();
    }

private:
    GameObjectRef notesParentRef;
    GameObjectRef playerRef;
    GameObjectRef cameraRef;
    std::vector<GameObjectRef> noteEntities;
    std::vector<Vec3> originalLocalPositions;
    std::vector<Vec3> originalLocalRotations;

    float pickupRange = 2.0f;
    float noteOffsetZ = 1.0f;
    bool matchCameraRotation = true;

    bool isViewing = false;
    int currentNoteIndex = -1;
    bool hasCached = false;

    void InitializeNotesCache() {
        if (hasCached && originalLocalPositions.size() == noteEntities.size()) {
            return;
        }
        if (noteEntities.empty() && notesParentRef.IsValid()) {
            const Entity parent = notesParentRef.GetEntity();
            const size_t childCount = GetChildCount(parent);
            noteEntities.reserve(childCount);
            for (size_t i = 0; i < childCount; ++i) {
                const Entity child = GetChild(i, parent);
                GameObjectRef ref(child);
                noteEntities.push_back(ref);
            }
        }
        CacheOriginalTransforms();
    }

    void CacheOriginalTransforms() {
        originalLocalPositions.clear();
        originalLocalRotations.clear();
        originalLocalPositions.reserve(noteEntities.size());
        originalLocalRotations.reserve(noteEntities.size());
        for (const auto& noteRef : noteEntities) {
            if (!noteRef.IsValid()) {
                originalLocalPositions.push_back(Vec3::Zero());
                originalLocalRotations.push_back(Vec3::Zero());
                continue;
            }
            const Entity noteEntity = noteRef.GetEntity();
            originalLocalPositions.push_back(TF_GetLocalPosition(noteEntity));
            originalLocalRotations.push_back(TF_GetLocalRotation(noteEntity));
        }
        hasCached = !originalLocalPositions.empty();
    }

    int FindClosestNoteIndex() const {
        if (!playerRef.IsValid()) {
            return -1;
        }
        const Entity playerEntity = playerRef.GetEntity();
        Vec3 playerPos = TF_GetPosition(playerEntity);
        float bestDistSq = pickupRange * pickupRange;
        int bestIndex = -1;
        for (size_t i = 0; i < noteEntities.size(); ++i) {
            if (!noteEntities[i].IsValid()) {
                continue;
            }
            const Entity noteEntity = noteEntities[i].GetEntity();
            Vec3 notePos = TF_GetPosition(noteEntity);
            Vec3 delta = notePos - playerPos;
            float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            if (distSq <= bestDistSq) {
                bestDistSq = distSq;
                bestIndex = static_cast<int>(i);
            }
        }
        return bestIndex;
    }

    void BeginViewClosestNote() {
        if (!cameraRef.IsValid()) {
            LOG_WARNING("Misc_NoteCollector: missing camera reference");
            return;
        }
        if (noteEntities.empty()) {
            return;
        }
        if (!hasCached || originalLocalPositions.size() != noteEntities.size()) {
            return;
        }
        int index = FindClosestNoteIndex();
        if (index < 0) {
            return;
        }
        currentNoteIndex = index;
        isViewing = true;
        MoveCurrentNoteToCamera();
    }

    void MoveCurrentNoteToCamera() {
        if (currentNoteIndex < 0 || currentNoteIndex >= static_cast<int>(noteEntities.size())) {
            return;
        }
        if (!noteEntities[currentNoteIndex].IsValid() || !cameraRef.IsValid()) {
            return;
        }
        const Entity noteEntity = noteEntities[currentNoteIndex].GetEntity();
        const Entity camEntity = cameraRef.GetEntity();
        Vec3 camPos = TF_GetPosition(camEntity);
        Vec3 camForward = TF_GetForward(camEntity);
        Vec3 targetPos = camPos + (camForward * noteOffsetZ);
        TF_SetPosition(targetPos, noteEntity);
        if (matchCameraRotation) {
            Vec3 camRot = TF_GetRotation(camEntity);
            TF_SetRotation(camRot, noteEntity);
        }
    }

    void RestoreCurrentNote() {
        if (currentNoteIndex < 0 || currentNoteIndex >= static_cast<int>(noteEntities.size())) {
            isViewing = false;
            currentNoteIndex = -1;
            return;
        }
        if (noteEntities[currentNoteIndex].IsValid()) {
            const Entity noteEntity = noteEntities[currentNoteIndex].GetEntity();
            TF_SetPosition(originalLocalPositions[currentNoteIndex], noteEntity);
            TF_SetRotation(originalLocalRotations[currentNoteIndex], noteEntity);
        }
        isViewing = false;
        currentNoteIndex = -1;
    }
};
