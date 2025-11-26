#pragma once
#include "EngineAPI.hpp"

/**
 * PlayerCameraRaycast - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class PlayerCameraRaycast : public IScript {
public:
	PlayerCameraRaycast() {
		// Register any editable fields here
		// Example: REGISTER_FIELD(speed);
		// Example: REGISTER_VECTOR(enemies);
		SCRIPT_FIELD(interactableLayerMask, Int);
		SCRIPT_FIELD(raycastDist, Float);
		SCRIPT_FIELD(raycastCooldown, Float);
	}

	~PlayerCameraRaycast() override = default;

	// === Lifecycle Methods ===

	void Awake() override {
		// Called when the script component is first created
	}

	void Initialize(Entity entity) override {
		// Called to initialize the script with its entity
	}

	void Start() override {
		// Called when the script is enabled and play mode starts
		raycastTimer = raycastCooldown;
	}

	void Update(double deltaTime) override {
		// Called every frame while the script is enabled
		if (Input::WasKeyReleased('P')) { // <- WILL ONLY WORK FOR LEVERS
			if (prevEntity != NULL)
			{
				Events::Send("Lever0");
			}
		}

		raycastTimer -= deltaTime;
		if (raycastTimer <= 0.0f)
		{
			TransformRef t = GetTransformRef(GetEntity());
			Vec3 forward = GetForward(GetEntity());
			RaycastHit h = Raycast(GetPosition(GetEntity()), forward, raycastDist);
			std::string log = "FORWARD VEC = X:" + std::to_string(forward.x) + ", Y: " + std::to_string(forward.y) + ", Z: " + std::to_string(forward.z);
			LOG_DEBUG(log.c_str());
			Vec3 pos = GetPosition(t);
			log = "POS VEC = X:" + std::to_string(pos.x) + ", Y: " + std::to_string(pos.y) + ", Z: " + std::to_string(pos.z);
			LOG_DEBUG(log.c_str());
			Vec3 combi = pos + forward;
			log = "COMBI VEC = X:" + std::to_string(combi.x) + ", Y: " + std::to_string(combi.y) + ", Z: " + std::to_string(combi.z);
			LOG_DEBUG(log.c_str());
			if (h.hasHit)
			{
				prevEntity = h.entity;
				LOG_DEBUG("ENTITY HIT BY RAYCAST");
				
			}
			else
			{
				prevEntity = NULL;
			}
			raycastTimer = raycastCooldown;
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
		return "PlayerCameraRaycast";
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
	// Add your private member variables here
	// Example: float speed = 5.0f;
	int interactableLayerMask = 69;
	float raycastDist = 50.0f;
	float raycastCooldown= 10.0f;
	float raycastTimer = 0.1f;
	Entity prevEntity;
};
