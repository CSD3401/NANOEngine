#pragma once
#include "EngineAPI.hpp"

/**
 * Pickable - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class AudioHolder : public IScript {
public:
	AudioHolder() {
		// Register any editable fields here
		// Example: SCRIPT_FIELD(speed, Float);
	}

	~AudioHolder() override = default;

	// === Lifecycle Methods ===

	void Awake() override {
		// Called when the script component is first created
	}

	void Initialize(Entity entity) override {
		// Called to initialize the script with its entity
	}

	void Start() override {

	}

	void Update(double deltaTime) override {
		if (Input::WasKeyPressed('1')) {
			LOG_INFO("send key lock solved");

			Events::Send("KeyLockSolved");
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
		return "AudioHolder";
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



private:

};
