#pragma once
#include "EngineAPI.hpp"


class Misc_WireBlinking : public IScript {
public:
    Misc_WireBlinking()
    {
        SCRIPT_FIELD(wireIndex, Int);
        SCRIPT_FIELD(wirePuzzleIndex, Int);
        SCRIPT_FIELD(correctSide, Bool);

        SCRIPT_COMPONENT_REF(originalRef, MaterialRef);
        SCRIPT_COMPONENT_REF(highlightRef, MaterialRef);
        SCRIPT_FIELD(blinkInterval, Float);
        SCRIPT_FIELD(isBlinking, Bool);
        SCRIPT_FIELD(buttonSolved, Bool);
    }
    ~Misc_WireBlinking() override = default;

    // == Custom Methods ==


    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {}
    void Update(double deltaTime) override
    {
        if (isBlinking)
        {
            blinkTimer += (float)deltaTime;
            if (blinkTimer > blinkInterval)
            {
                isHighlighted = !isHighlighted;
                blinkTimer = 0.0f;
                if (isHighlighted)
                {
                    SetMaterialRef(GetRendererRef(GetEntity()), highlightRef);
                }
                else
                {
                    SetMaterialRef(GetRendererRef(GetEntity()), originalRef);
                }
            }
        }

    }
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {
        std::string listenToMessage = "WireButtonPressed" + std::to_string(wirePuzzleIndex);
        Events::Listen(listenToMessage.c_str(), [this](void* data) {
            this->RecieveIndexData(data);
            });

        std::string listenToMessage2 = "ButtonSolved" + std::to_string(wirePuzzleIndex);
        Events::Listen(listenToMessage2.c_str(), [this](void* data) {
            this->SolvedListener(data);
            });

    }
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_WireBlinking"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    bool GetCorrectSide()
    {
        return correctSide;
    }

    void RecieveIndexData(void* data)
    {
        if (buttonSolved)
            return;

        std::string indexData = *reinterpret_cast<std::string*>(data);
        int wSide = std::stoi(indexData.substr(0, 1));
        int wIndex = std::stoi(indexData.substr(1));

        int side = correctSide ? 1 : 0;

        if (side == wSide && wireIndex == wIndex)
        {
            isBlinking = true;
        }
        else
        {
            if (side == wSide && wireIndex != wIndex)
            {
                TurnOffBlinking();
            }
        }
    }

    void TurnOffBlinking()
    {
        // switch the material back to normal here
        isBlinking = false;

        SetMaterialRef(GetRendererRef(GetEntity()), originalRef);
    }

    void SolvedListener(void* data)
    {

        std::string indexData = *reinterpret_cast<std::string*>(data);

        int wSide = std::stoi(indexData.substr(0, 1));
        int wIndex = std::stoi(indexData.substr(1));

        int side = correctSide ? 1 : 0;
        if (side == wSide && wireIndex == wIndex)
        {
            buttonSolved = true;
            TurnOffBlinking();
        }
    }
private:
    int wireIndex = 0;
    int wirePuzzleIndex = 0;
    bool buttonSolved = false;
    bool correctSide = false; // false -> top row, true -> bottom row
    std::string wireData;



    MaterialRef originalRef;
    MaterialRef highlightRef;
    float blinkInterval = 0.5f;
    float blinkTimer = 0.0f;
    bool isBlinking = false;
    bool isHighlighted = false;
};