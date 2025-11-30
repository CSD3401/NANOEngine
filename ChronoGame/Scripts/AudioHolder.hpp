#pragma once
#include "EngineAPI.hpp"
using namespace NE::Scripting;

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
	}

	void Initialize(Entity entity) override {
		// Called to initialize the script with its entity
	}

	void Start() override {
		// Called when the script component is first created
		PlayAudio("event:/BGM_NIGHTSHIFT_LOW");
	}

	// CHEATCODE
	void Update(double deltaTime) override {
		if (Input::WasKeyPressed('1')) {
			LOG_INFO("send key lock solved");

			Events::Send("KeyLockSolved");
		}

		if (Input::WasKeyPressed('2')) {
			LOG_INFO("send MaterialSequencerSolved");

			Events::Send("MaterialSequencerSolved");
		}


		//if (Input::WasKeyPressed('2')) {
		//	PlayAudio("event:/VOICEOVER1");
		//}

		//if (Input::WasKeyPressed('3')) {
		//	PlayAudio("event:/VOICEOVER2");
		//}

		//if (Input::WasKeyPressed('4')) {
		//	PlayAudio("event:/VOICEOVER3");
		//}

		//if (Input::WasKeyPressed('5')) {
		//	PlayAudio("event:/VOICEOVER4");
		//}

		//if (Input::WasKeyPressed('6')) {
		//	PlayAudio("event:/VOICEOVER5");
		//}
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
