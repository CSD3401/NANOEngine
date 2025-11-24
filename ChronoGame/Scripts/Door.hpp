#pragma once
#include <ScriptSDK/ScriptAPI.h>
#include <vector>

using namespace NE::Scripting;

/**
 * Door - Opens when triggered by pressure plate events
 *
 * FEATURES:
 * - Listens for "PressurePlateActivated" events
 * - Opens when specific plate IDs activate
 * - Smooth opening animation
 * - Can require multiple plates (AND logic) or single plate (OR logic)
 *
 * USAGE:
 * 1. Attach to door object
 * 2. Add plate IDs to requiredPlateIDs array
 * 3. Door automatically listens for those plates
 */
class Door : public IScript {
public:
	enum class OpeningStyle {
		SlideUp = 0,
		SlideDown = 1,
		SlideLeft = 2,
		SlideRight = 3
	};

	enum class ActivationLogic {
		RequireAll = 0,   // ALL plates must be activated (AND)
		RequireAny = 1    // ANY plate can activate (OR)
	};

	Door() {
		// Constructor empty - fields registered in Initialize
	}

	~Door() override = default;

	void Awake() override {
		// Register to listen for pressure plate activation events
		RegisterScriptEventListener("PressurePlateActivated", [this](void* data) {
			if (data) {
				std::string* plateID = static_cast<std::string*>(data);
				OnPlateActivated(*plateID);
			}
			});

		// Register to listen for pressure plate deactivation events
		RegisterScriptEventListener("PressurePlateDeactivated", [this](void* data) {
			if (data) {
				std::string* plateID = static_cast<std::string*>(data);
				OnPlateDeactivated(*plateID);
			}
			});

		if (debugMode) {
			LOG_INFO("Door is now listening for pressure plate events");
		}
	}

	void Initialize(Entity entity) override {
		// Register fields using SCRIPT_FIELD macro
		SCRIPT_FIELD_VECTOR(requiredPlateIDs, String);

		RegisterEnumField("openingStyle", &openingStyle, {
			"SlideUp",
			"SlideDown",
			"SlideLeft",
			"SlideRight"
			});

		RegisterEnumField("activationLogic", &activationLogic, {
			"RequireAll",
			"RequireAny"
			});

		SCRIPT_FIELD(openDistance, Float);
		SCRIPT_FIELD(openSpeed, Float);
		SCRIPT_FIELD(debugMode, Bool);
	}

	void Start() override {
		isOpen = false;
		isMoving = false;

		closedPosition = GetPosition();

		// Calculate open position based on opening style
		switch (openingStyle) {
		case OpeningStyle::SlideUp:
			openPosition = Vec3(closedPosition.x, closedPosition.y + openDistance, closedPosition.z);
			break;
		case OpeningStyle::SlideDown:
			openPosition = Vec3(closedPosition.x, closedPosition.y - openDistance, closedPosition.z);
			break;
		case OpeningStyle::SlideLeft:
			openPosition = Vec3(closedPosition.x - openDistance, closedPosition.y, closedPosition.z);
			break;
		case OpeningStyle::SlideRight:
			openPosition = Vec3(closedPosition.x + openDistance, closedPosition.y, closedPosition.z);
			break;
		}

		// Initialize activation tracking
		activatedPlates.clear();

		if (debugMode) {
			LOG_INFO("Door initialized - Listening for " << requiredPlateIDs.size() << " plates");
			for (size_t i = 0; i < requiredPlateIDs.size(); ++i) {
				LOG_INFO("  Required plate[" << i << "]: " << requiredPlateIDs[i]);
			}
		}
	}

	void Update(double deltaTime) override {
		// Animate door movement
		if (isMoving) {
			AnimateDoor(deltaTime);
		}
	}

	void OnDestroy() override {
		// Event listeners are automatically cleaned up by the engine
	}

	void OnEnable() override {}
	void OnDisable() override {}
	void OnValidate() override {}

	const char* GetTypeName() const override { return "Door"; }

	void OnCollisionEnter(Entity other) override {}
	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

private:
	void OnPlateActivated(const std::string& plateID) {
		// Check if this plate is one we care about
		bool isRelevant = false;
		for (const auto& requiredID : requiredPlateIDs) {
			if (requiredID == plateID) {
				isRelevant = true;
				break;
			}
		}

		if (!isRelevant) {
			if (debugMode) {
				LOG_INFO("Door ignoring plate '" << plateID << "' (not in required list)");
			}
			return;
		}

		// Add to activated list if not already there
		bool alreadyActivated = false;
		for (const auto& activatedID : activatedPlates) {
			if (activatedID == plateID) {
				alreadyActivated = true;
				break;
			}
		}

		if (!alreadyActivated) {
			activatedPlates.push_back(plateID);

			if (debugMode) {
				LOG_INFO("Plate '" << plateID << "' activated (" << activatedPlates.size() << "/" << requiredPlateIDs.size() << ")");
			}
		}

		// Check if door should open
		CheckShouldOpen();
	}

	void OnPlateDeactivated(const std::string& plateID) {
		// Remove from activated list
		for (size_t i = 0; i < activatedPlates.size(); ++i) {
			if (activatedPlates[i] == plateID) {
				activatedPlates.erase(activatedPlates.begin() + i);

				if (debugMode) {
					LOG_INFO("Plate '" << plateID << "' deactivated");
				}
				break;
			}
		}

		// Check if door should close
		CheckShouldClose();
	}

	void CheckShouldOpen() {
		if (isOpen) return;

		bool shouldOpen = false;

		if (activationLogic == ActivationLogic::RequireAll) {
			// ALL plates must be activated (AND logic)
			shouldOpen = (activatedPlates.size() == requiredPlateIDs.size());
		}
		else {
			// ANY plate can activate (OR logic)
			shouldOpen = (activatedPlates.size() > 0);
		}

		if (shouldOpen) {
			OpenDoor();
		}
	}

	void CheckShouldClose() {
		if (!isOpen) return;

		bool shouldClose = false;

		if (activationLogic == ActivationLogic::RequireAll) {
			// Close if not all plates are activated
			shouldClose = (activatedPlates.size() < requiredPlateIDs.size());
		}
		else {
			// Close if no plates are activated
			shouldClose = (activatedPlates.size() == 0);
		}

		if (shouldClose) {
			CloseDoor();
		}
	}

	void OpenDoor() {
		if (isOpen) return;

		isOpen = true;
		isMoving = true;

		if (debugMode) {
			LOG_INFO("====== DOOR OPENING ======");
		}
	}

	void CloseDoor() {
		if (!isOpen) return;

		isOpen = false;
		isMoving = true;

		if (debugMode) {
			LOG_INFO("====== DOOR CLOSING ======");
		}
	}

	void AnimateDoor(double deltaTime) {
		Vec3 currentPos = GetPosition();
		Vec3 targetPos = isOpen ? openPosition : closedPosition;

		// Calculate direction and distance to target
		Vec3 direction = Vec3(
			targetPos.x - currentPos.x,
			targetPos.y - currentPos.y,
			targetPos.z - currentPos.z
		);

		float distance = std::sqrt(
			direction.x * direction.x +
			direction.y * direction.y +
			direction.z * direction.z
		);

		// Check if we've reached the target
		if (distance < 0.1f) {
			SetPosition(targetPos);
			isMoving = false;

			if (debugMode) {
				LOG_INFO("Door finished " << (isOpen ? "opening" : "closing"));
			}
			return;
		}

		// Move toward target
		float moveAmount = openSpeed * deltaTime;
		if (moveAmount > distance) moveAmount = distance;

		Vec3 normalizedDir = Vec3(
			direction.x / distance,
			direction.y / distance,
			direction.z / distance
		);

		Translate(
			normalizedDir.x * moveAmount,
			normalizedDir.y * moveAmount,
			normalizedDir.z * moveAmount
		);
	}

	// === Exposed Fields ===
	std::vector<std::string> requiredPlateIDs;  // Which plates activate this door
	OpeningStyle openingStyle = OpeningStyle::SlideUp;
	ActivationLogic activationLogic = ActivationLogic::RequireAny;
	float openDistance = 3.0f;     // How far door moves
	float openSpeed = 2.0f;        // Speed of animation
	bool debugMode = false;

	// === Internal State ===
	bool isOpen = false;
	bool isMoving = false;
	Vec3 closedPosition;
	Vec3 openPosition;
	std::vector<std::string> activatedPlates;  // Currently active plates
};