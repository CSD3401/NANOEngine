#pragma once
#include "EngineAPI.hpp"
#include "Puzzle_.hpp"
#include "Interactable_.hpp"
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Interactable_TwoWaySwitch is a two-way switch that is toggled when interacted with,
* sending its state to a Puzzle_ receiver.
*/

class Interactable_TwoWaySwitch : public Interactable_ {
public:
    Interactable_TwoWaySwitch() {}
    ~Interactable_TwoWaySwitch() override = default;

    // === Custom Methods ===
    void Initialise(Puzzle_* receiver, bool state)
    {
		_receiver = receiver;
		_state = state;
    }
    void Interact() override
    {
        if (_receiver)
        { 
            _receiver->ReceiveInput(_state);
			_state = !_state;
        }
        else
        {
            LOG_DEBUG("No receiver assigned for TwoWaySwitch!");
        }
    }

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {}
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Interactable_TwoWaySwitch"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    Puzzle_* _receiver;
    bool _state;
};