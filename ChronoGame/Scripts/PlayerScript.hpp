#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "EngineAPI.hpp"

void DelayedPrintUpdate() {
	LOG_DEBUG("hi 3 seconds over player");

}

/**
 * Example player script demonstrating how to implement IScript.
 * Now uses the built-in field system from IScript base class.
 */
class PlayerScript : public NE::Scripting::IScript {
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

		//LOG_DEBUG("PlayerScript created");
	}

	~PlayerScript() override {
		//LOG_DEBUG("PlayerScript destroyed");
	}

	// === IScript Interface ===
	void Awake() override {
		//LOG_DEBUG("PlayerScript::Awake() called for entity {}", GetEntity());

		Coroutines::Create();
	}

	void Initialize(NE::Scripting::Entity entity) override {
		//LOG_DEBUG("PlayerScript initialized for entity {}", entity);
	}

	void Start() override {
		//LOG_DEBUG("PlayerScript::Start() called for entity {}", GetEntity());
	}

	void OnValidate() override {
		//LOG_DEBUG(" PlayerScript::OnValidate() called!");
		//LOG_DEBUG("  speed={}, lives={}, state={}", speed, lives, static_cast<int>(state));
		//LOG_DEBUG("  flags vector size: {}", flags.size());
		//for (size_t i = 0; i < flags.size(); ++i) {
		//	LOG_DEBUG("    flags[{}] = {}", i, flags[i] ? "true" : "false");
		//}
		//LOG_DEBUG("  playerFlags: canJump={}, canDoubleJump={}, hasKey={}, questComplete={}",
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
			//LOG_DEBUG("PlayerScript updating - Entity: {}, DeltaTime: {}", GetEntity(), deltaTime);
			//LOG_DEBUG("  State: {}", static_cast<int>(state));
			//LOG_DEBUG("  Health: {}/{}, Stamina: {}", stats.health, stats.maxHealth, stats.stamina);
			//LOG_DEBUG("  Tracking {} enemies, {} waypoints", enemyIDs.size(), waypoints.size());
			m_timeSinceLastLog = 0.0;
		}

		// Unity-style movement with helper functions
		float moveSpeed = speed * (float)deltaTime;

		// Update state based on input
		if (Input::IsKeyDown('D')) {
			Translate(moveSpeed, 0, 0);
			state = PlayerState::Walking;
		}
		else if (Input::IsKeyDown('A')) {
			Translate(-moveSpeed, 0, 0);
			state = PlayerState::Walking;
		}
		else if (Input::IsKeyDown('W')) {
			Translate(0, moveSpeed, 0);
			state = PlayerState::Running;
		}
		else if (Input::IsKeyDown('S')) {
			Translate(0, -moveSpeed, 0);
			state = PlayerState::Walking;
		}
		else {
			state = PlayerState::Idle;
		}

		if (Input::WasKeyPressed('K')) {
			int dmg = 20;
			Events::Send("OnPlayerHit", &dmg);
			LOG_DEBUG("Santaclaus is coming to town for my damn choice");
		}
		else if (Input::WasKeyPressed('C'))
		{
			// Create a new coroutine
			Coroutines::Handle h = Coroutines::Create();

			Events::Send("TimeSwapNow", nullptr);

			// Wait 3 seconds
			Coroutines::AddWait(h, 3.0f);

			// Action after wait
			Coroutines::AddAction(h, DelayedPrintUpdate);

			// Start the coroutine
			Coroutines::Start(h);

			LOG_DEBUG("Timer started no way josed!");
		}

		//Courutine Test
		// if (ctimer != 0 && !Engine_IsCoroutineRunning(ctimer)) {
        // LOG_DEBUG("hi 3 seconds over player");
        // std::cout << "santa clause" << std::endl;
        // ctimer = 0;  // Reset so we don't print every frame
    	// }

		//  EXAMPLE: Use the bool flags
		if (Input::IsKeyDown(VK_SPACE) && playerFlags.canJump) {
			//LOG_DEBUG("Player jumped!");
			// Jump logic here
		}

		//  EXAMPLE: Patrol through waypoints
		if (!waypoints.empty()) {
			static size_t currentWaypoint = 0;
			float targetX = waypoints[currentWaypoint % waypoints.size()];

			if (abs(GetPosition().x - targetX) < 0.5f) {
				currentWaypoint++;
				//LOG_DEBUG("Reached waypoint {}, moving to next", currentWaypoint - 1);
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
		//LOG_DEBUG("PlayerScript cleanup for entity {}", GetEntity());
	}

	void OnEnable() override {
		//LOG_DEBUG("PlayerScript enabled for entity {}", GetEntity());
	}

	void OnDisable() override {
		//LOG_DEBUG("PlayerScript disabled for entity {}", GetEntity());
	}

	const char* GetTypeName() const override {
		return "PlayerScript";
	}

	// === Event Handlers ===
	void OnCollisionEnter(NE::Scripting::Entity other) override {
		//LOG_DEBUG("PlayerScript collision enter with entity {}", other);
	}

	void OnCollisionExit(NE::Scripting::Entity other) override {
		//LOG_DEBUG("PlayerScript collision exit with entity {}", other);
	}

	void OnTriggerEnter(NE::Scripting::Entity other) override {
		//LOG_DEBUG("PlayerScript trigger enter with entity {}", other);
	}

	void OnTriggerExit(NE::Scripting::Entity other) override {
		//LOG_DEBUG("PlayerScript trigger exit with entity {}", other);
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
	NE::Scripting::Vec3 color{ 1.0f, 0.5f, 0.25f };
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
	NE::Scripting::CoroutineHandle chandle;

	// Field registry
	ExposedFieldRegistry m_fields;
};

