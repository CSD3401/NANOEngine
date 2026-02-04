#pragma once
#include "EngineAPI.hpp"
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Puzzle_ is the parent class for all puzzles in the game.
* It holds enum PuzzleKey for identification and methods for solving,
* unsolving, and receiving inputs.
*/

class Puzzle_ : public IScript {
public:
    Puzzle_() {
		SCRIPT_FIELD(puzzleKey, Int);
    }
    ~Puzzle_() override = default;

    // === Custom Methods ===
    void Solve()
    {
		Events::Send("PuzzleSolved", (void*)puzzleKey);
    }
    void Unsolve()
    {
		Events::Send("PuzzleUnsolved", (void*)puzzleKey);
    }
    virtual void ReceiveInput(bool input)
    {
        LOG_WARNING("ReceiveInput(bool) not implemented in " << this->GetTypeName());
    }
    virtual void ReceiveInput(char input)
    {
        LOG_WARNING("ReceiveInput(char) not implemented in " << this->GetTypeName());
    }
    virtual void ReceiveInput(int input)
    {
        LOG_WARNING("ReceiveInput(int) not implemented in " << this->GetTypeName());
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
    const char* GetTypeName() const override { return "Puzzle_"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
	int puzzleKey;
};