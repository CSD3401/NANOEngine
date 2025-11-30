#pragma once
#include "EngineAPI.hpp"

/**
 * Pickable - Objects that can be picked up and carried
 *
 * Tag values:
 * 0 = Key (sends color when released for Lock matching)
 * 1 = Object (can be dropped on pressure plates)
 */
class Pickable : public IScript {
public:
	Pickable() {
		// Register any editable fields here
		// Example: SCRIPT_FIELD(speed, Float);
		SCRIPT_COMPONENT_REF(materialA, MaterialRef);
		SCRIPT_FIELD(rbEntt, Int);
		SCRIPT_FIELD(picked, Bool);
		SCRIPT_FIELD(color, String);
		SCRIPT_FIELD(tag, Int);  // 0 = Key, 1 = Object
		SCRIPT_FIELD(hardcodedPosition, Vec3); 
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
		rb._SetEntity(rbEntt);
		Events::Listen("OnCameraRaycastHit",
			[this](void* data) {
				Picked(data);
			}
		);
	}

	void Update(double deltaTime) override {
		if (!picked)
			return;

		Vec3 cameraPos = GetWorldPosition(pickedBy);
		Vec3 forward = GetForward(pickedBy);

		Vec3 targetPos = cameraPos + forward * pickDistance;

		TransformRef selfTransform = GetTransformRef(GetEntity());
		Vec3 currentPos = GetPosition(selfTransform);

		Vec3 toTarget = targetPos - currentPos;
		float dist = toTarget.Length();
		

		if (Input::WasMouseReleased(0)) {
			picked = false;
			PlayAudio("event:/UNGRAB");
			return;
		}

		if (dist < 0.001f)
			return;

		Vec3 dir = toTarget.Normalized();

		const float stiffness = 120.0f;
		const float damping = 8.0f;
		const float maxForce = 300.0f;

		Vec3 force = dir * (stiffness * dist);

		if (rb) {
			Vec3 vel = GetVelocity(rb);
			force -= vel * damping;
		}

		float forceLen = force.Length();
		if (forceLen > maxForce) {
			force *= (maxForce / forceLen);
		}

		if (rb) {
			AddForce(rb, force);
		} else {
			float t = static_cast<float>(deltaTime) * 10.0f;
			t = std::min(t, 1.0f);
			Vec3 newPos = currentPos + toTarget * t;
			SetPosition(newPos);
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
		PlayAudio("event:/GRAB");
		auto* entityPtr = static_cast<std::pair<uint32_t, uint32_t>*>(data);
		uint32_t entity = entityPtr->first;

		//LOG_INFO("Picked Entity: " << entity);
		if (entity == GetEntity()) {
			NE::Renderer::Command::AssignMaterial(entity, materialA);
			picked = true;
			pickedBy = entityPtr->second;
		}
	}
	
private:
	// Add your private member variables here
	MaterialRef materialA{};
	int rbEntt;
	RigidbodyRef rb;
	bool picked = false;
	Entity pickedBy;
	float pickDistance = 2.f;
	std::string color;
	int tag = 1;  // 0 = Key, 1 = Object
	Vec3 hardcodedPosition = Vec3(0, 0, 0);
	static inline int counter = 0;
};
