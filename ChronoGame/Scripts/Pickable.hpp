#pragma once
#include "EngineAPI.hpp"

/**
 * Pickable - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class Pickable : public IScript {
public:
	Pickable() {
		// Register any editable fields here
		// Example: SCRIPT_FIELD(speed, Float);
		SCRIPT_COMPONENT_REF(materialA, MaterialRef);
		SCRIPT_FIELD(picked, Bool);
	}

	~Pickable() override = default;
	
	// === Lifecycle Methods ===

	void Awake() override {
		// Called when the script component is first created
	}

	void Initialize(Entity entity) override {
		// Called to initialize the script with its entity
	}

	void Start() override {
		// Called when the script is enabled and play mode starts
		Events::Listen("OnCameraRaycastHit",
			[this](void* data) {
				Picked(data);
			}
		);
	}

	void Update(double deltaTime) override {
		if (picked) {
			Vec3 forward = GetForward(pickedBy);
			SetPosition(forward * pickDistance);

			if (Input::WasMouseReleased(0)) picked = false;
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
		return "Pickable";
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

	void Picked(void* data) {
		auto* entityPtr = static_cast<std::pair<uint32_t, uint32_t>*>(data);
		uint32_t entity = entityPtr->first;


		//LOG_CRITICAL("Picked Called");
		//LOG_INFO("Picked Entity: " << entity);
		if (entity == GetEntity()) {
			NE::Renderer::Command::AssignMaterial(entity, materialA);
			picked = true;
			pickedBy = entityPtr->second;
		}
	}
	
private:
	// Add your private member variables here
	// Example: float speed = 5.0f;
	MaterialRef materialA{};
	bool picked = false;
	Entity pickedBy;

	float pickDistance = 4.f;
};
