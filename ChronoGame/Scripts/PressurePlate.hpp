#pragma once
#include <ScriptSDK/ScriptAPI.h>

using namespace NE::Scripting;

// I wanted to jus default set all floating point to double but SCRIPT_FIELD macro only supports Float (float)
/**
 * PressurePlate - Activates when object is placed on it
 *
 * FEATURES:
 * - Detects objects via trigger collision
 * - Broadcasts activation events
 * - Visual feedback (sinking animation)
 * - Can stay activated or deactivate when object leaves
 *
 * USAGE:
 * 1. Attach to platform with Collider (set as Trigger)
 * 2. Configure activation requirements
 * 3. Other objects listen for "PressurePlateActivated" event
 */
class PressurePlate : public IScript {
public:
	PressurePlate() {
		// Constructor is empty - fields registered in Initialize()
	}

	~PressurePlate() override = default;

	void Awake() override {}

	void Initialize(Entity entity) override {
		// Register fields using SCRIPT_FIELD macro in Initialize()
		SCRIPT_FIELD(plateID, String);
		SCRIPT_FIELD(requiresContinuousPressure, Bool);
		SCRIPT_FIELD(activationDelay, Float);
		SCRIPT_FIELD(sinkAmount, Float);
		SCRIPT_FIELD(debugMode, Bool);
	}

	void Start() override {
		isActivated = false;
		objectsOnPlate = 0;
		activationTimer = 0.0f;
		initialPosition = GetPosition();
		sunkenPosition = Vec3(
			initialPosition.x,
			initialPosition.y - sinkAmount,
			initialPosition.z
		);

		if (debugMode) {
			LOG_INFO("PressurePlate '" << plateID << "' initialized");
		}
	}

	void Update(double deltaTime) override {
		// Handle activation delay
		if (objectsOnPlate > 0 && !isActivated) {
			activationTimer += (float)deltaTime;

			if (activationTimer >= activationDelay) {
				Activate();
			}
		}

		// Handle continuous pressure requirement
		if (requiresContinuousPressure && objectsOnPlate == 0 && isActivated) {
			Deactivate();
		}

		// Animate plate sinking/rising
		AnimatePlate(deltaTime);
	}

	void OnDestroy() override {}
	void OnEnable() override {}
	void OnDisable() override {}

	void OnValidate() override {
		if (plateID.empty()) {
			plateID = "Plate1";
		}
		if (activationDelay < 0) activationDelay = 0;
	}

	const char* GetTypeName() const override { return "PressurePlate"; }

	void OnCollisionEnter(Entity other) override {}
	void OnCollisionExit(Entity other) override {}

	void OnTriggerEnter(Entity other) override {
		objectsOnPlate++;

		if (debugMode) {
			LOG_INFO("Object entered plate '" << plateID << "' (count: " << objectsOnPlate << ")");
		}

		// Reset timer if this is the first object
		if (objectsOnPlate == 1) {
			activationTimer = 0.0f;
		}
	}

	void OnTriggerExit(Entity other) override {
		if (objectsOnPlate > 0) {
			objectsOnPlate--;
		}

		if (debugMode) {
			LOG_INFO("Object left plate '" << plateID << "' (count: " << objectsOnPlate << ")");
		}

		// Reset activation timer when no objects remain
		if (objectsOnPlate == 0) {
			activationTimer = 0.0f;
		}
	}

private:
	void Activate() {
		if (isActivated) return;

		isActivated = true;

		if (debugMode) {
			LOG_INFO("PressurePlate '" << plateID << "' ACTIVATED!");
		}

		// Broadcast activation event using the correct NANOEngine API
		SendScriptEvent("PressurePlateActivated", &plateID);
	}

	void Deactivate() {
		if (!isActivated) return;

		isActivated = false;

		if (debugMode) {
			LOG_INFO("PressurePlate '" << plateID << "' deactivated");
		}

		// Broadcast deactivation event using the correct NANOEngine API
		SendScriptEvent("PressurePlateDeactivated", &plateID);
	}

	void AnimatePlate(double deltaTime) {
		Vec3 currentPos = GetPosition();
		Vec3 targetPos = (objectsOnPlate > 0) ? sunkenPosition : initialPosition;

		// Smooth interpolation
		float lerpSpeed = 5.0f;
		float t = 1.0f - std::exp(-lerpSpeed * (float)deltaTime);

		Vec3 newPos = Vec3(
			currentPos.x + (targetPos.x - currentPos.x) * t,
			currentPos.y + (targetPos.y - currentPos.y) * t,
			currentPos.z + (targetPos.z - currentPos.z) * t
		);

		SetPosition(newPos);
	}

	// === Exposed Fields (registered in Initialize) ===
	std::string plateID = "Plate1";           // Unique identifier
	bool requiresContinuousPressure = true;   // Deactivate when object leaves?
	float activationDelay = 0.5f;             // Seconds before activation
	float sinkAmount = 0.2f;                  // How far plate sinks when pressed
	bool debugMode = false;

	// === Internal State (not exposed) ===
	bool isActivated = false;
	int objectsOnPlate = 0;
	float activationTimer = 0.0f;
	Vec3 initialPosition;
	Vec3 sunkenPosition;
};