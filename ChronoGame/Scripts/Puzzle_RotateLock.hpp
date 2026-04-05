#pragma once
#include "EngineAPI.hpp"
#include "Puzzle_.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */

enum class ALPHA_CODE {
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H
};
enum class NUM_CODE {
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
    _8
};
class Puzzle_RotateLock : public Puzzle_ {
public:
    Puzzle_RotateLock() {
        // Register any editable fields here
        // Example: SCRIPT_FIELD(speed, float);
        // Example: SCRIPT_FIELD_VECTOR(blingstring, String);;

        SCRIPT_FIELD(inUse, Bool);
        SCRIPT_FIELD(rotationSpeed, Float);
        SCRIPT_FIELD(numCorrect, Int);
        SCRIPT_FIELD(checkTimerAmount, Float);
        SCRIPT_FIELD(rotationMargin, Float);

        SCRIPT_COMPONENT_REF(outerRingRef, TransformRef);
        SCRIPT_FIELD(rotateLockIndex, String);

        SCRIPT_ENUM_VECTOR_FIELD(correctAlphas, "A", "B", "C", "D", "E", "F", "G", "H");
        SCRIPT_ENUM_VECTOR_FIELD(correctNums, "_1", "_2", "_3", "_4", "_5", "_6", "_7", "_8");

        SCRIPT_FIELD(solvedEvent, String);
    }

    ~Puzzle_RotateLock() override = default;

    // === Lifecycle Methods ===

    void Awake() override {
        // Called when the script component is first created
    }

    void Initialize(Entity entity) override {
        // Called to initialize the script with its entity

        Puzzle_::Initialize(entity);
    }

    void Start() override {
        // Called when the script is enabled and play mode starts

    }

    void Update(double deltaTime) override {
        // Called every frame while the script is enabled

        if (inUse)
        {
            RotateLock(deltaTime);

        }
    }

    void OnDestroy() override {
        // Called when the script is about to be destroyed
    }

    // === Optional Callbacks ===

    void OnEnable() override {
        // Called when the script is enabled
        Events::Listen(rotateLockIndex.c_str(), [this](void* data) {

            // Get the PuzzleKey from data
            ToggleUse();

            });

        // Hard set to not be in use
        inUse = false;
    }

    void OnDisable() override {
        // Called when the script is disabled
    }

    void OnValidate() override {
        // Called when a field value is changed in the editor
    }

    const char* GetTypeName() const override {
        return "Puzzle_RotateLock";
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }

    void OnTriggerEnter(Entity other) override {
        (void)other;
    }

    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    void ToggleUse()
    {
        inUse = !inUse;
        // Toggle the use of the camera/movement
        if (inUse)
        {
            // switch them off
        }
        else
        {
            // switch them on
        }
    }

    void RotateLock(double deltaTime)
    {
        auto [scrollX, scrollY] = Input::GetScrollDelta();
        Vec3 currRot = TF_GetLocalRotation(outerRingRef.GetEntity());

        bool hasInput = false;

        if (scrollY > 0.0f || Input::IsKeyDown(VK_RIGHT))
        {
            checkTimer = checkTimerAmount;
            currRot.z += rotationSpeed * (float)deltaTime;
            TF_SetRotation(currRot, outerRingRef.GetEntity());
            currentAngle = currRot.z;
            hasInput = true;
        }
        else if (scrollY < 0.0f || Input::IsKeyDown(VK_LEFT))
        {
            checkTimer = checkTimerAmount;
            currRot.z -= rotationSpeed * (float)deltaTime;
            TF_SetRotation(currRot, outerRingRef.GetEntity());
            currentAngle = currRot.z;
            hasInput = true;
        }

        if (m_rotateSfxCooldown > 0.0f)
            m_rotateSfxCooldown -= static_cast<float>(deltaTime);

        if (hasInput && m_rotateSfxCooldown <= 0.0f)
        {
            PlayAudio("event:/BOMB_ROTATE");
            m_rotateSfxCooldown = ROTATE_SFX_INTERVAL;
        }

        CheckCurrentRotation(deltaTime);
    }

    void CheckCurrentRotation(double deltaTime)
    {
        int totalSteps = static_cast<int>(std::min(correctAlphas.size(), correctNums.size()));
        if (numCorrect >= totalSteps)
            return;

        if (checkTimer >= 0.0f)
        {
            checkTimer -= static_cast<float>(deltaTime);
            return;
        }

        float angle = WrapAngle(currentAngle);

        int alphaIndex = static_cast<int>(correctAlphas[numCorrect]);
        int numIndex = static_cast<int>(correctNums[numCorrect]);

        int targetSteps = (numIndex - alphaIndex + 8) % 8;
        float targetAngle = targetSteps * 45.0f;

        float diff = fabs(angle - targetAngle);
        if (diff > 180.0f)
            diff = 360.0f - diff;

        if (diff <= rotationMargin)
        {
            numCorrect++;
            checkTimer = checkTimerAmount;
            LOG_DEBUG("CORRECT ROTATION STEP");

            PlayAudio("event:/SLOT_IN"); // change this audio RF

            // Send message here to turn like the lights on the bomb or smth
            // message should be the rotatelockindex string var + numcorrect
            // eg; rotate_011 for getting the first correct
            // can have another script to catch the message

            if (numCorrect >= totalSteps)
            {
                PlayAudio("event:/SMALL_BOMB_SOLVED");
                LOG_DEBUG("SOLVED THE ROTATION PUZZLE");
                Solve();
                if (!solvedEvent.empty())
                {
                    Events::Send(solvedEvent.c_str());
                    LOG_DEBUG("[Puzzle_RotateLock] Sent solved event: " + solvedEvent);
                }
            }
        }

    }

    float WrapAngle(float angle)
    {
        float wrapped = fmod(angle, 360.0f);
        if (wrapped < 0.0f)
            wrapped += 360.0f;
        return wrapped;
    }

private:
    // So outer ring only rotates(alphabet ring)
    // must line up w the inner(number) ring
    // check the z rotation of the 
    // CHECK AGAINST THE NUMBER
    // Inner ring will always be set at 0 Z-Rot
    bool inUse = false;
    float rotationSpeed = 50.0f;

    int numCorrect = 0;
    bool isCorrectRot = false;
    float checkTimerAmount = 0.5f;
    float checkTimer = 0.5f;// maybe needed? need to be on correct rotation for x seconds to light up the thing
    float rotationMargin = 10.0f; // within +- X degrees

    TransformRef outerRingRef;

    std::string rotateLockIndex = "rotate_01";
    std::string solvedEvent = "";

    // ANSWERS HERE
    std::vector<ALPHA_CODE> correctAlphas;
    std::vector<NUM_CODE> correctNums;


    float currentAngle;

    static constexpr float ROTATE_SFX_INTERVAL = 0.3f; // seconds between one-shot triggers
    float m_rotateSfxCooldown = 0.0f;
};