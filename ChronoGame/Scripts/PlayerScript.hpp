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
 * Example player script demonstrating standardized field registration.
 * Now uses ONLY IScript's built-in FieldRegistry - no ExposedFieldRegistry needed!
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
		//  PRE-FILL TEST DATA
		enemyIDs = { 42, 57, 103, 999 };  // 4 enemy IDs to test remove
		waypoints = { 10.5f, 25.0f, 42.3f, 58.7f };  // 4 waypoint positions
		flags = { true, false, true, false, true };// 5 quest flags

		SCRIPT_FIELD_VECTOR(blingstring, String);
		SCRIPT_FIELD_VECTOR(eDDDD,Entity);
	}

	~PlayerScript() override {
		//LOG_DEBUG("PlayerScript destroyed");
	}

	// === IScript Interface ===
	void Awake() override {
		//LOG_DEBUG("PlayerScript::Awake() called for entity {}", GetEntity());
		Coroutines::Create();
	}

	void Initialize(Entity entity) override {
		// Register primitive fields
		SCRIPT_FIELD(speed, Float);
		SCRIPT_FIELD(color, Vec3);
		SCRIPT_FIELD(lives, Int);
		SCRIPT_FIELD(godMode, Bool);
		SCRIPT_FIELD(label, String);

		// Register enum field - provide enum option names
		RegisterEnumField("state", &state, {
			"Idle",
			"Walking",
			"Running",
			"Jumping"
		});

		// Register vector fields
		SCRIPT_FIELD_VECTOR(enemyIDs, Int);
		SCRIPT_FIELD_VECTOR(waypoints, Float);
		SCRIPT_FIELD_VECTOR(flags, Bool);

		// Register struct fields using reflection
		SCRIPT_FIELD_STRUCT(stats);
		SCRIPT_FIELD_STRUCT(playerFlags);

		SCRIPT_COMPONENT_REF(tref0, TransformRef);

		//LOG_DEBUG("PlayerScript initialized for entity {}", entity);
	}

	void Start() override {
		//LOG_DEBUG("PlayerScript::Start() called for entity {}", GetEntity());

		//tref0 = GetTransformRef(eDDDD[0]);
	}

	void OnValidate() override {
		//LOG_DEBUG(" PlayerScript::OnValidate() called!");
		// Validate field values when changed in editor
		if (speed < 0) speed = 0;
		if (lives < 0) lives = 0;
	}

	void Update(double deltaTime) override {
		m_timeSinceLastLog += deltaTime;

		if (m_timeSinceLastLog >= LOG_INTERVAL) {
			//LOG_DEBUG("PlayerScript updating - Entity: {}, DeltaTime: {}", GetEntity(), deltaTime);
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

		SetPosition(tref0, GetPosition() + Vec3(1.0,1.0,0));

		if (Input::WasKeyPressed('K')) {
			/*int dmg = 20;
			Events::Send("OnPlayerHit", &dmg);
			LOG_DEBUG("Santaclaus is coming to town for my damn choice");*/

			NE::Scripting::SwitchScene("Assets/NewScene.scene");
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

			LOG_DEBUG("Timer start macdonaldo!");
		}

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
	void OnCollisionEnter(Entity other) override {
		//LOG_DEBUG("PlayerScript collision enter with entity {}", other);
	}

	void OnCollisionExit(Entity other) override {
		//LOG_DEBUG("PlayerScript collision exit with entity {}", other);
	}

	void OnTriggerEnter(Entity other) override {
		//LOG_DEBUG("PlayerScript trigger enter with entity {}", other);
	}

	void OnTriggerExit(Entity other) override {
		//LOG_DEBUG("PlayerScript trigger exit with entity {}", other);
	}

private:
	double m_timeSinceLastLog = 0.0;
	static constexpr double LOG_INTERVAL = 2.0;

	// Editable fields
	float speed = 5.0f;
	Vec3 color{ 1.0f, 0.5f, 0.25f };
	int lives = 3;
	bool godMode = false;
	std::string label = "Player";
	PlayerState state = PlayerState::Idle;

	// Vector fields
	std::vector<int> enemyIDs;
	std::vector<float> waypoints;
	std::vector<bool> flags;
	std::vector<std::string> blingstring;
	std::vector<Entity> eDDDD;

	TransformRef tref0;

	// Struct fields
	PlayerStats stats;
	PlayerFlags playerFlags;

	// Coroutine
	CoroutineHandle chandle;
};
