#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
#include "ECS/Components/Transform.hpp"
#include "ExposedFieldRegistry.hpp"
#include "Events/EventBus.hpp"
#include "Core/Couroutine.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <Math/Vec3.hpp>
#include <Core/SpdLogger.hpp>

void DelayedPrintUpdate() {
	SPD_DEBUG("hi 3 seconds over player");
        
}

/**
 * Example player script demonstrating how to implement IScript.
 * Now uses the built-in field system from IScript base class.
 */
class PlayerScript : public IScript {
public:
	// Example enum for testing
	enum class PlayerState {
		Idle = 0,
		Walking = 1,
		Running = 2,
		Jumping = 3
	};

	// Example custom struct for testing
	struct PlayerStats {
		int health = 100;
		int maxHealth = 100;
		float stamina = 50.0f;
		int level = 1;

		NE_REFLECT_BEGIN(PlayerStats)
			NE_REFLECT_FIELD(health),
			NE_REFLECT_FIELD(maxHealth),
			NE_REFLECT_FIELD(stamina),
			NE_REFLECT_FIELD(level)
			NE_REFLECT_END()
	};

	//  Example struct with 4 bools
	struct PlayerFlags {
		bool canJump = true;
		bool canDoubleJump = false;
		bool hasKey = false;
		bool questComplete = false;

		NE_REFLECT_BEGIN(PlayerFlags)
			NE_REFLECT_FIELD(canJump),
			NE_REFLECT_FIELD(canDoubleJump),
			NE_REFLECT_FIELD(hasKey),
			NE_REFLECT_FIELD(questComplete)
			NE_REFLECT_END()
	};

	PlayerScript() {
		// Register primitive fields
		REGISTER_FIELD(speed);
		REGISTER_FIELD(color);
		REGISTER_FIELD(lives);
		REGISTER_FIELD(godMode);
		REGISTER_FIELD(label);

		// Register enum field
		REGISTER_ENUM(state, "Idle", "Walking", "Running", "Jumping");

		// Register vector fields
		REGISTER_VECTOR(enemyIDs);
		REGISTER_VECTOR(waypoints);
		REGISTER_VECTOR(flags);

		// Register struct fields
		REGISTER_REFLECTABLE_STRUCT(stats);
		REGISTER_REFLECTABLE_STRUCT(playerFlags);  //  4 bool struct

		//  PRE-FILL TEST DATA  working
		enemyIDs = { 42, 57, 103, 999 };  // 4 enemy IDs to test remove
		waypoints = { 10.5f, 25.0f, 42.3f, 58.7f };  // 4 waypoint positions
		flags = { true, false, true, false, true };// 5 quest flags

		//SPD_DEBUG("PlayerScript created");
	}

	~PlayerScript() override {
		//SPD_DEBUG("PlayerScript destroyed");
	}

	// === IScript Interface ===
	void Awake() override {
		//SPD_DEBUG("PlayerScript::Awake() called for entity {}", GetEntity());

		chandle = Engine_CreateCoroutine();
	}

	void Initialize(NE::ECS::Entity entity) override {
		//SPD_DEBUG("PlayerScript initialized for entity {}", entity);
	}

	void Start() override {
		//SPD_DEBUG("PlayerScript::Start() called for entity {}", GetEntity());
	}

	void OnValidate() override {
		//SPD_DEBUG(" PlayerScript::OnValidate() called!");
		//SPD_DEBUG("  speed={}, lives={}, state={}", speed, lives, static_cast<int>(state));
		//SPD_DEBUG("  flags vector size: {}", flags.size());
		//for (size_t i = 0; i < flags.size(); ++i) {
		//	SPD_DEBUG("    flags[{}] = {}", i, flags[i] ? "true" : "false");
		//}
		//SPD_DEBUG("  playerFlags: canJump={}, canDoubleJump={}, hasKey={}, questComplete={}",
		//	playerFlags.canJump ? "true" : "false",
		//	playerFlags.canDoubleJump ? "true" : "false",
		//	playerFlags.hasKey ? "true" : "false",
		//	playerFlags.questComplete ? "true" : "false");

		// Validate field values when changed in editor
		if (speed < 0) speed = 0;
		if (lives < 0) lives = 0;
	}

	void Update(double deltaTime) override {
		m_timeSinceLastLog += deltaTime;

		if (m_timeSinceLastLog >= LOG_INTERVAL) {
			//SPD_DEBUG("PlayerScript updating - Entity: {}, DeltaTime: {}", GetEntity(), deltaTime);
			//SPD_DEBUG("  State: {}", static_cast<int>(state));
			//SPD_DEBUG("  Health: {}/{}, Stamina: {}", stats.health, stats.maxHealth, stats.stamina);
			//SPD_DEBUG("  Tracking {} enemies, {} waypoints", enemyIDs.size(), waypoints.size());
			m_timeSinceLastLog = 0.0;
		}

		// Unity-style movement with helper functions
		float moveSpeed = speed * (float)deltaTime;

		// Update state based on input
		if (NE::InputManager::IsKeyDown('D')) {
			Translate(moveSpeed, 0, 0);
			state = PlayerState::Walking;
		}
		else if (NE::InputManager::IsKeyDown('A')) {
			Translate(-moveSpeed, 0, 0);
			state = PlayerState::Walking;
		}
		else if (NE::InputManager::IsKeyDown('W')) {
			Translate(0, moveSpeed, 0);
			state = PlayerState::Running;
		}
		else if (NE::InputManager::IsKeyDown('S')) {
			Translate(0, -moveSpeed, 0);
			state = PlayerState::Walking;
		}
		else {
			state = PlayerState::Idle;
		}

		if (NE::InputManager::WasKeyPressed('K')) {
			int dmg = 20;
			NANOEngine::Events::SendScriptEvent("OnPlayerHit", &dmg);
			SPD_DEBUG("Santaclaus is coming to town for my second big mac");
		}
		else if (NE::InputManager::WasKeyPressed('C'))
		{
			// Create a new coroutine
			/*CoroutineHandle h = Engine_CreateCoroutine();*/

			NANOEngine::Events::SendScriptEvent("TimeSwapNow",nullptr);

			//// Wait 3 seconds
			//Engine_AddWaitForSeconds(h, 3.0f);

			//// Action after wait
			//Engine_AddAction(h, DelayedPrintUpdate);

			//// Start the coroutine
			//Engine_StartCoroutine(h);

			SPD_DEBUG("Timer started no way josed!");
		}

		//Courutine Test
		// if (ctimer != 0 && !Engine_IsCoroutineRunning(ctimer)) {
        // SPD_DEBUG("hi 3 seconds over player");
        // std::cout << "santa clause" << std::endl;
        // ctimer = 0;  // Reset so we don't print every frame
    	// }

		//  EXAMPLE: Use the bool flags
		if (NE::InputManager::IsKeyDown(VK_SPACE) && playerFlags.canJump) {
			//SPD_DEBUG("Player jumped!");
			// Jump logic here
		}

		//  EXAMPLE: Patrol through waypoints
		if (!waypoints.empty()) {
			static size_t currentWaypoint = 0;
			float targetX = waypoints[currentWaypoint % waypoints.size()];

			if (abs(GetPosition().x - targetX) < 0.5f) {
				currentWaypoint++;
				//SPD_DEBUG("Reached waypoint {}, moving to next", currentWaypoint - 1);
			}
		}

		//  EXAMPLE: Chase enemies if we have any tracked
		if (!enemyIDs.empty()) {
			// Chase first enemy in list
			// Entity enemyEntity = enemyIDs[0];
			// Vec3 enemyPos = GetEntityPosition(enemyEntity);
			// MoveTowards(enemyPos, speed * deltaTime);
		}
	}

	void OnDestroy() override {
		//SPD_DEBUG("PlayerScript cleanup for entity {}", GetEntity());
	}

	void OnEnable() override {
		//SPD_DEBUG("PlayerScript enabled for entity {}", GetEntity());
	}

	void OnDisable() override {
		//SPD_DEBUG("PlayerScript disabled for entity {}", GetEntity());
	}

	const char* GetTypeName() const override {
		return "PlayerScript";
	}

	// === Event Handlers ===
	void OnCollisionEnter(NE::ECS::Entity other) override {
		//SPD_DEBUG("PlayerScript collision enter with entity {}", other);
	}

	void OnCollisionExit(NE::ECS::Entity other) override {
		//SPD_DEBUG("PlayerScript collision exit with entity {}", other);
	}

	void OnTriggerEnter(NE::ECS::Entity other) override {
		//SPD_DEBUG("PlayerScript trigger enter with entity {}", other);
	}

	void OnTriggerExit(NE::ECS::Entity other) override {
		//SPD_DEBUG("PlayerScript trigger exit with entity {}", other);
	}

	// === Exposed editable fields via registry ===
	std::vector<std::string> GetExposedFieldNames() const override { return m_fields.GetNames(); }
	std::string GetFieldType(const std::string& name) const override { return m_fields.GetType(name); }
	std::string GetFieldValueAsString(const std::string& name) const override { return m_fields.GetValue(name); }
	bool SetFieldValueFromString(const std::string& name, const std::string& value) override { return m_fields.SetValue(name, value); }

	// Enum support
	std::vector<std::string> GetEnumOptions(const std::string& fieldName) const override {
		return m_fields.GetEnumOptions(fieldName);
	}

	// Array/Vector support
	size_t GetArraySize(const std::string& fieldName) const override {
		return m_fields.GetArraySize(fieldName);
	}

	std::string GetArrayElement(const std::string& fieldName, size_t index) const override {
		return m_fields.GetArrayElement(fieldName, index);
	}

	bool SetArrayElement(const std::string& fieldName, size_t index, const std::string& value) override {
		return m_fields.SetArrayElement(fieldName, index, value);
	}

	void AddArrayElement(const std::string& fieldName) override {
		m_fields.AddArrayElement(fieldName);
	}

	void RemoveArrayElement(const std::string& fieldName, size_t index) override {
		m_fields.RemoveArrayElement(fieldName, index);
	}

private:
	double m_timeSinceLastLog = 0.0;
	static constexpr double LOG_INTERVAL = 2.0;

	// Editable fields
	float speed = 5.0f;
	NE::Math::Vec3 color{ 1.0f, 0.5f, 0.25f };
	int lives = 3;
	bool godMode = false;
	std::string label = "Player";
	PlayerState state = PlayerState::Idle;

	// Vector fields
	std::vector<int> enemyIDs;
	std::vector<float> waypoints;
	std::vector<bool> flags;

	// Struct fields
	PlayerStats stats;
	PlayerFlags playerFlags;  //  4 bool struct

	// Coroutine
	CoroutineHandle chandle;

	// Field registry
	ExposedFieldRegistry m_fields;
};

