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
    void Awake() override {}
    void Initialize(Entity entity) override {
        Puzzle_::Initialize(entity);
    }
    void Start() override {
		// Grab object references
        if (twoStateRotaterObject.IsValid())
        {
            twoStateRotater = GameObject(twoStateRotaterObject).GetComponent<Misc_TwoStateRotater>();
        }
        else
        {
            LOG_WARNING("Puzzle_Lever: twoStateRotaterObject reference is invalid!");
        }

        if (twoWaySwitchObject.IsValid())
        {
            twoWaySwitch = GameObject(twoWaySwitchObject).GetComponent<Interactable_TwoWaySwitch>();
            
        }
        else
        {
            LOG_WARNING("Puzzle_Lever: twoWaySwitchObject reference is invalid!");
        }

		// If we get both components successfully, initialise the two-way switch
        if (twoWaySwitch && twoStateRotater)
        {
            twoWaySwitch->Initialise(this, !twoStateRotater->GetStartingState());
        }
        else
        {
            LOG_WARNING("Puzzle_Lever: Missing components!");
        }
    }
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Puzzle_Lever"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
	GameObjectRef twoStateRotaterObject;
	GameObjectRef twoWaySwitchObject;

	Misc_TwoStateRotater* twoStateRotater;
	Interactable_TwoWaySwitch* twoWaySwitch;
};