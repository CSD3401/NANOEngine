#pragma once
#include "EngineAPI.hpp"
/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Puzzle_ is the parent class for all puzzles in the game.
* It holds enum PuzzleKey for identification and methods for solving,
* unsolving, and receiving inputs.
*/

enum class PuzzleKey {
    _1,
    _2,
    _3,
    _4,
    _5,
    _6

};

class Puzzle_ : public IScript {
public:
    Puzzle_() {}
    ~Puzzle_() override = default;

    // === Custom Methods ===
    void Solve()
    {
		Events::Send("PuzzleSolved", &puzzleKey);
    }
    void Unsolve()
    {
		Events::Send("PuzzleUnsolved", &puzzleKey);
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
    void Initialize(Entity entity) override {
        SCRIPT_ENUM_FIELD(puzzleKey, "_1", "_2", "_3", "_4", "_5", "_6");
    }
    void Start() override {}
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Puzzle_"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
	PuzzleKey puzzleKey;
};