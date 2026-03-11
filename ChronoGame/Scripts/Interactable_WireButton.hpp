#pragma once
#include "EngineAPI.hpp"
#include "Interactable_.hpp"
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Interactable_ is the parent class for all interactable objects in the game.
* It simply provides a virtual function Interact that can be overridden by child classes.
*/

// Legacy class
class Interactable_WireButton : public Interactable_ {
public:
    Interactable_WireButton()
    {
        SCRIPT_FIELD(leftWireIndex, Int);
        SCRIPT_FIELD(wirePuzzleIndex, Int);
    }
    ~Interactable_WireButton() override = default;

    // == Custom Methods ==
    virtual void Interact()
    {
        if (puzzleSolved)
            return;

        std::string message = "WireButtonPressed" + std::to_string(wirePuzzleIndex);
        LOG_DEBUG("BUTTON PRESSED:" + message);
        Events::Send(message.c_str(), &leftWireIndex);
        PlayAudio("event:/CLICK_WIRE_BUTTON");

    }

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {}
    void Update(double deltaTime) override
    {
        //if (Input::WasKeyReleased('N') && leftWireIndex == 0)
        //{
        //    std::string message = "WireButtonPressed" + std::to_string(wirePuzzleIndex);
        //    LOG_DEBUG("BUTTON PRESSED:" + message);
        //    Events::Send(message.c_str(), &leftWireIndex);
        //}
        //if (Input::WasKeyReleased('M') && leftWireIndex == 1)
        //{
        //    std::string message = "WireButtonPressed" + std::to_string(wirePuzzleIndex);
        //    LOG_DEBUG("BUTTON PRESSED:" + message);
        //    Events::Send(message.c_str(), &leftWireIndex);

        //}
        //if (Input::WasKeyReleased(',') && leftWireIndex == 2)
        //{
        //    std::string message = "WireButtonPressed" + std::to_string(wirePuzzleIndex);
        //    LOG_DEBUG("BUTTON PRESSED:" + message);
        //    Events::Send(message.c_str(), &leftWireIndex);

        //}
        //if (Input::WasKeyReleased('B'))
        //{
        //    Events::Send("MOVE");
        //}

    }
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Interactable_WireButton"; }

    void PuzzleSolved()
    {
        puzzleSolved = true;
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    int leftWireIndex = 0;
    int wirePuzzleIndex = 0;
    bool puzzleSolved = false;
};