#pragma once
#include "EngineAPI.hpp"

class Lock : public IScript {
public:
	Lock() {
		SCRIPT_COMPONENT_REF(materialA, MaterialRef);
		SCRIPT_FIELD(picked, Bool);
		SCRIPT_FIELD(color, String);
	}

	~Lock() override = default;

	void Awake() override {}

	void Initialize(Entity entity) override {}

	void Start() override {
		Events::Listen("TryToUnlock",
			[this](void* data) {
				TryToUnlock(data);
			}
		);
	}

	void Update(double deltaTime) override {

		if (setObjInactiveNextFrame)
		{
			// Deactivate the key
			SetActive(false, keyEntity);

			// Deactivate this lock
			SetActive(false, GetEntity());

			successfulUnlocks++;  // Increment static counter
			LOG_WARNING("Total successful unlocks: " << successfulUnlocks);

			if (successfulUnlocks == 3)
			{
				LOG_INFO("All locks unlocked! Unlocking Door");
				shouldSendKeyLockSolved = true;  // Defer to next frame
			}
		}

		// Deferred event sending
		if (shouldSendKeyLockSolved) {
			LOG_INFO("Sending KeyLockSolved event");
			Events::Send("KeyLockSolved");
			shouldSendKeyLockSolved = false;

			// Deactivate the key
			SetActive(false, keyEntity);

			// Deactivate this lock
			SetActive(false, GetEntity());
		}
	}

	void OnDestroy() override {}
	void OnEnable() override {}
	void OnDisable() override {}
	void OnValidate() override {}

	const char* GetTypeName() const override {
		return "Lock";
	}

	void OnCollisionEnter(Entity other) override {}
	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

	void TryToUnlock(void* data)
	{
		struct UnlockData {
			std::string color;
			uint32_t keyEntity;
		};

		auto* unlockData = static_cast<UnlockData*>(data);
		std::string keyColor = unlockData->color;
		keyEntity = unlockData->keyEntity;

		LOG_INFO("Attempting to unlock " << keyColor << " lock\n");
		LOG_INFO("Key entity: " << keyEntity);

		if (this->color == keyColor)
		{
			LOG_INFO("Successfully Unlock, Key and Lock Pair SetInactive");

			setObjInactiveNextFrame = true;

			//// Deactivate the key
			//SetActive(false, keyEntity);

			//// Deactivate this lock
			//SetActive(false, GetEntity());

			//successfulUnlocks++;  // Increment static counter
			//LOG_WARNING("Total successful unlocks: " << successfulUnlocks);

			//if (successfulUnlocks == 3)
			//{
			//	LOG_INFO("All locks unlocked! Unlocking Door");
			//	shouldSendKeyLockSolved = true;  // Defer to next frame
			//}
		}
		else
		{
			LOG_INFO("Failed to Unlock, Key and Lock Pair Color Mismatch");
		}
	}

	static int GetSuccessfulUnlocks() {
		return successfulUnlocks;
	}

	static void ResetCounter() {
		successfulUnlocks = 0;
	}

private:
	MaterialRef materialA{};
	bool picked = false;
	Entity pickedBy;
	float pickDistance = 4.f;
	std::string color;
	
	bool setObjInactiveNextFrame = false;
	bool shouldSendKeyLockSolved = false;  // Flag to defer event
	Entity keyEntity; 
	// Static counter shared across all Lock instances
	static inline int successfulUnlocks = 0;
};