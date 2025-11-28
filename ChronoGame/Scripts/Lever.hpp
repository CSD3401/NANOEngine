#pragma once
#include "EngineAPI.hpp"
#include "Interactable.hpp"

/**
 * Lever - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class Lever : public Interactable{
public:
	Lever() {
		// Register any editable fields here
		// Example: REGISTER_FIELD(speed);
		// Example: REGISTER_VECTOR(enemies);
		SCRIPT_FIELD(eventMessage, String);
		SCRIPT_FIELD(layerMask, Int);
	}

	~Lever() override = default;

	// === Lifecycle Methods ===

	void Awake() override {
		// Called when the script component is first created
	}

	void Initialize(Entity entity) override {
		// Called to initialize the script with its entity
		
	}

	void Start() override {
		// Called when the script is enabled and play mode starts
		startingRot = GetRotation(GetTransformRef(GetEntity()));
	}

	void Update(double deltaTime) override {
		// Called every frame while the script is enabled

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
		return "Lever";
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

	void Interact() override{
		Events::Send(eventMessage.c_str());

		//Tweener::StartVec3([this](const Vec3& rot) { SetPosition(rot); },
		//	startingRot, targetRot, 0.5f, NE::Scripting::TweenType::EASE_BOTH, GetEntity());
	}

private:
	// Add your private member variables here
	// Example: float speed = 5.0f;
	std::string eventMessage = "Lever0";
	Vec3 startingRot;
	Vec3 targetRot;
	int layerMask = 69;
};
