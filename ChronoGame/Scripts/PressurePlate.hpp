#pragma once
#include "EngineAPI.hpp"

class PressurePlate : public IScript {
public:
	PressurePlate() {
		SCRIPT_FIELD(isActivated, Bool);
	}

	~PressurePlate() override = default;

	void Awake() override {}

	void Initialize(Entity entity) override {}

	void Start() override {
		Events::Listen("ObjectDroppedOnPlate",
			[this](void* data) {
				OnObjectDropped(data);
			}
		);
	}

	void Update(double deltaTime) override {}

	void OnDestroy() override {}
	void OnEnable() override {}
	void OnDisable() override {}
	void OnValidate() override {}

	const char* GetTypeName() const override {
		return "PressurePlate";
	}

	void OnCollisionEnter(Entity other) override {}
	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

	void OnObjectDropped(void* data) {
		struct DropData {
			uint32_t droppedItem;
			uint32_t plateEntity;
		};

		auto* dropData = static_cast<DropData*>(data);

		// Check if the object was dropped on THIS pressure plate
		if (dropData->plateEntity == GetEntity()) {
			LOG_INFO("Object " << dropData->droppedItem << " dropped on pressure plate!");

			if (!isActivated) {
				isActivated = true;
				LOG_INFO("Pressure plate ACTIVATED!");
				Events::Send("PressurePlateActivated");
			}
		}
	}

private:
	bool isActivated = false;
};