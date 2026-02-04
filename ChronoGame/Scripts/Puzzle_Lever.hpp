#pragma once
#include "EngineAPI.hpp"
#include "Puzzle_.hpp"
#include "Misc_TwoStateRotater.hpp"
#include "Interactable_TwoWaySwitch.hpp"
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
*/

class Puzzle_Lever : public Puzzle_ {
public:
    Puzzle_Lever() {
        SCRIPT_GAMEOBJECT_REF(twoStateRotaterObject);
        SCRIPT_GAMEOBJECT_REF(twoWaySwitchObject);
    }
    ~Puzzle_Lever() override = default;

    // === Custom Methods ===
    void ReceiveInput(bool input)
    {
        twoStateRotater->SetState(input);
        if (input)
        {
            Solve();
        }
        else
        {
            Unsolve();
        }
    }

    // === Lifecycle Methods ===
    void Awake() override
    {
        if (twoStateRotaterObject.IsValid())
        {
            twoStateRotater = GameObject(twoStateRotaterObject).GetComponent<Misc_TwoStateRotater>();
            if (!twoStateRotater)
            {
				LOG_DEBUG("Puzzle_Lever: twoStateRotater component not found on referenced object!");
            }
        }
        if (twoWaySwitchObject.IsValid())
        {
            twoWaySwitch = GameObject(twoWaySwitchObject).GetComponent<Interactable_TwoWaySwitch>();
        }
    }
    void Initialize(Entity entity) override {}
    void Start() override {}
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Puzzle_Lever"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
	GameObjectRef twoStateRotaterObject;
	GameObjectRef twoWaySwitchObject;

	Misc_TwoStateRotater* twoStateRotater;
	Interactable_TwoWaySwitch* twoWaySwitch;
};