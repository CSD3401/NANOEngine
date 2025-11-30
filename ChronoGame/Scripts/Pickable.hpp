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
		Events::Listen("OnCameraRaycastHit",
			[this](void* data) {
				Picked(data);
			}
		);
	}

	void Update(double deltaTime) override {
		if (picked) {
			// FIXED positioning
			TransformRef cameraTransform = GetTransformRef(pickedBy);
			Vec3 cameraPos = GetPosition(cameraTransform);
			Vec3 forward = GetForward(cameraTransform);
			SetPosition(cameraPos + forward * pickDistance);

			if (Input::WasMouseReleased(0))
			{
				picked = false;

				// Keys
				if (tag == 0 && color.length() > 0)
				{
					// Create struct to pass both color AND entity
					struct KeyData {
						std::string color;
						uint32_t keyEntity;
					};

					KeyData data;
					data.color = color;
					data.keyEntity = GetEntity();  // The entity this script is attached to

					Events::Send("GetKeyColor", &data);
					PlayAudio("event:/UNGRAB");
				}
				// Objects for Pressure Plates
				else if (tag == 1)
				{
					// Raycast down to see what's below
					//Vec3 down(0, -1, 0);
					//auto hit = Raycast(GetPosition(), down, 2.0f);

					//if (hit.entity != NE::Scripting::INVALID_ENTITY) 
					//{
					//	LOG_INFO("Dropped object on entity: " << hit.entity);

					//	struct DropData {
					//		uint32_t droppedItem;
					//		uint32_t plateEntity;
					//	};
					//	DropData dropData;
					//	dropData.droppedItem = GetEntity();
					//	dropData.plateEntity = hit.entity;

					//	Events::Send("ObjectDroppedOnPlate", &dropData);
					//}
					//else 
					//{
					//	LOG_INFO("Dropped object on nothing");
					//}

					// Uk what im too lazy to debug this shit
					SetPosition(hardcodedPosition);
				}
			}
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
	bool picked = false;
	Entity pickedBy;
	float pickDistance = 4.f;
	std::string color;
	int tag = 1;  // 0 = Key, 1 = Object
	Vec3 hardcodedPosition = Vec3(0, 0, 0);
};
