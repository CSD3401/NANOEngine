#pragma once
#include <algorithm>
#include <set>
#include "EngineAPI.hpp"
#include "Puzzle_.hpp"

/*
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Listener_ is the parent class for all puzzle listeners in the game.
* It provides interface for different types of listeners to inherit from.
*/

enum class ListenerType {
	ANY_SOLVE,   // As long as one in the list is solved, puzzle is solved
	ALL_SOLVE    // All in the list must be solved for puzzle to be solved
};

class Listener_ : public IScript {
public:
    Listener_()
        : listenerType{ ListenerType::ANY_SOLVE }
        , puzzleKeys{}
        , solvedPuzzles{}
		, unsolvedPuzzles{}
    {
    }
    ~Listener_() override = default;

    // === Custom Methods ===
    virtual void Solve() {};
	virtual void Unsolve() {};

    void ListenSolve(PuzzleKey k)
    {
        // Reject keys that don't matter to us
        std::vector<PuzzleKey>::iterator it = std::find(puzzleKeys.begin(), puzzleKeys.end(), k);
        if (it == puzzleKeys.end()) { return; }

        // Update sets
        if (unsolvedPuzzles.contains(k))
        {
            unsolvedPuzzles.erase(k);
        }
        solvedPuzzles.insert(k);

        // Logic
        switch (listenerType)
        {
        case ListenerType::ANY_SOLVE:
        {
            if (solvedPuzzles.size() > 0)
            {
                Solve();
            }
            break;
        }
        case ListenerType::ALL_SOLVE:
        {
            if (unsolvedPuzzles.size() == 0)
            {
                Solve();
            }
            break;
        }
        default:
            break; // Should never call
        }
    }

    void ListenUnsolve(PuzzleKey k)
    {
        // Reject keys that don't matter to us
        std::vector<PuzzleKey>::iterator it = std::find(puzzleKeys.begin(), puzzleKeys.end(), k);
        if (it == puzzleKeys.end()) { return; }

        // Update sets
        if (solvedPuzzles.contains(k))
        {
            solvedPuzzles.erase(k);
        }
        unsolvedPuzzles.insert(k);

        // Logic
        switch (listenerType)
        {
        case ListenerType::ANY_SOLVE:
        {
            if (solvedPuzzles.size() == 0)
            {
                Unsolve();
            }
            break;
        }
        case ListenerType::ALL_SOLVE:
        {
            if (unsolvedPuzzles.size() > 0)
            {
                Unsolve();
            }
            break;
        }
        default:
            break; // Should never call
        }
	}

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {
        SCRIPT_ENUM_FIELD(listenerType, "ANY_SOLVE", "ALL_SOLVE");
        SCRIPT_ENUM_VECTOR_FIELD(puzzleKeys, "_1", "_2", "_3", "_4", "_5", "_6");
    }
    void Start() override {
        LOG_DEBUG("Listener_ Start called!");
        for (PuzzleKey key : puzzleKeys)
        {
			unsolvedPuzzles.insert(key);
        }
    }
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {
        // Listen for puzzle solved events
        Events::Listen("PuzzleSolved", [this](void* data) {
            
			// Get the PuzzleKey from data
			PuzzleKey k = *static_cast<PuzzleKey*>(data);
			ListenSolve(k);

            });
        Events::Listen("PuzzleUnsolved", [this](void* data) {
            // Get the PuzzleKey from data
            PuzzleKey k = *static_cast<PuzzleKey*>(data);
            ListenUnsolve(k);

            });
    }
    void OnDisable() override {
		// In the future, we might want to unregister listeners here
    }
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Listener_"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
	ListenerType listenerType;
	std::vector<PuzzleKey> puzzleKeys;

	std::set<PuzzleKey> solvedPuzzles;
	std::set<PuzzleKey> unsolvedPuzzles;
};