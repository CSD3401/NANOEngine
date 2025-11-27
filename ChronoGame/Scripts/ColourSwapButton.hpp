#pragma once
#include "EngineAPI.hpp"

/**
 * ColourSwapButton - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class ColourSwapButton : public IScript {
public:
	ColourSwapButton() {
		// Register any editable fields here
		// Example: SCRIPT_FIELD(speed, Float);
		SCRIPT_FIELD(puzzleIndex, Int);
		SCRIPT_FIELD(leftIndex, Int);
	}

	~ColourSwapButton() override = default;

	// === Lifecycle Methods ===

	void Awake() override {
		// Called when the script component is first created
	}

	void Initialize(Entity entity) override {
		// Called to initialize the script with its entity
	}

	void Start() override {
		// Called when the script is enabled and play mode starts
	}

	void Update(double deltaTime) override {
		// Called every frame while the script is enabled
		if (Input::WasKeyReleased('N')) {
			Interact();
		}
		else if (Input::WasKeyReleased('M')) {
			//Interact();
			Events::Send("Lever0");
		}
	}

	void OnDestroy() override {
		// Called when the script is about to be destroyed
	}

	// === Optional Callbacks ===

	void OnEnable() override {
		// Called when the script is enabled
	}

	void OnDisable() override {
		// Called when the script is disabled
	}

	void OnValidate() override {
		// Called when a field value is changed in the editor
	}

	const char* GetTypeName() const override {
		return "ColourSwapButton";
	}

	// === Collision Callbacks ===

	void OnCollisionEnter(Entity other) override {
		// Called when this entity starts colliding with another
	}

	void OnCollisionExit(Entity other) override {
		// Called when this entity stops colliding with another
	}

	void OnTriggerEnter(Entity other) override {
		// Called when this entity enters a trigger
	}

	void OnTriggerExit(Entity other) override {
		// Called when this entity exits a trigger
	}

	void Interact() {
		// Event bus
		std::string event = eventSend + std::to_string(puzzleIndex);
		Events::Send(event.c_str(), &leftIndex);
	}

private:
	// Add your private member variables here
	// Example: float speed = 5.0f;
	int leftIndex = 0;
	int puzzleIndex = 0;

	std::string eventSend = "ColourSwapPuzzle";
};
