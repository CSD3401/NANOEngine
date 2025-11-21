#pragma once
#include "EngineAPI.hpp"
#include <array>

// GLFW key codes
#define GLFW_KEY_UP 265
#define GLFW_KEY_DOWN 264
#define GLFW_KEY_LEFT 263
#define GLFW_KEY_RIGHT 262
#define GLFW_KEY_SPACE 32
#define GLFW_KEY_X 88
#define GLFW_KEY_Z 90

class MatchingPuzzle : public IScript {
public:
	MatchingPuzzle() {
		// Use the new macro! Much cleaner
		SCRIPT_COMPONENT_REF(targetTransform, Transform);
		SCRIPT_COMPONENT_REF(endTransform, Transform);
		
		SCRIPT_COMPONENT_REF(transform0, Transform);
		SCRIPT_COMPONENT_REF(transform1, Transform);
		SCRIPT_COMPONENT_REF(transform2, Transform);
		SCRIPT_COMPONENT_REF(transform3, Transform);
		SCRIPT_COMPONENT_REF(transform4, Transform);
		SCRIPT_COMPONENT_REF(transform5, Transform);
		SCRIPT_COMPONENT_REF(transform6, Transform);
		SCRIPT_COMPONENT_REF(transform7, Transform);
		SCRIPT_COMPONENT_REF(transform8, Transform);
		SCRIPT_COMPONENT_REF(transform9, Transform);
		SCRIPT_COMPONENT_REF(transform10, Transform);
		SCRIPT_COMPONENT_REF(transform11, Transform);

		// You can also register regular fields if needed
		// SCRIPT_FIELD(speed, Float);
		// SCRIPT_FIELD(distance, Float);
	}

	~MatchingPuzzle() override = default;

	void Awake() override {}

	void Initialize(Entity entity) override {}

	void Start() override {
		LOG_INFO("MatchingPuzzle started!");

		// Check if we have a valid target assigned
		if (targetTransform.IsValid()) {
			Entity targetEntity = targetTransform.GetEntity();
			LOG_INFO("Target entity assigned: " << targetEntity);
			Vec3 targetPos = GetPosition(targetTransform);
			LOG_INFO("Target Pos: " << targetPos.x << ", " << targetPos.y << ", " << targetPos.z);
		}
		else {
			LOG_WARNING("No target entity assigned!");
		}

		// Set each tile to ALL direction by default
		for (auto& row : grid) 
		{
			row.fill(ALL);
		}
	}

	void Update(double deltaTime) override {
		// Always check if reference is valid (entity exists and has component)
		if (!targetTransform.IsValid()) {
			return; // No target assigned or entity was destroyed
		}

		// Get target's position using the ScriptAPI method
		Vec3 targetPos = GetPosition(targetTransform);
		Vec3 myPos = GetPosition();

		// Calculate distance
		float dx = targetPos.x - myPos.x;
		float dy = targetPos.y - myPos.y;
		float dz = targetPos.z - myPos.z;
		float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

		// Example: Move towards target
		if (distance > 0.1f) {
			// Normalize direction
			float invDist = 1.0f / distance;
			Vec3 direction = {
				dx * invDist,
				dy * invDist,
				dz * invDist
			};

			// Move towards target at 2 units per second
			float speed = 2.0f;
			Translate(
				direction.x * speed * static_cast<float>(deltaTime),
				direction.y * speed * static_cast<float>(deltaTime),
				direction.z * speed * static_cast<float>(deltaTime)
			);
		}

		if (Input::WasKeyPressed('W'))
		{
			Vec3 currentPos = GetPosition(targetTransform);
			currentPos.y += 1;
			SetPosition(targetTransform, currentPos);
			LOG_INFO("MOVE UP");
		}
		if (Input::WasKeyPressed('S'))
		{
			Vec3 currentPos = GetPosition(targetTransform);
			currentPos.y -= 1;
			SetPosition(targetTransform, currentPos);
			LOG_INFO("MOVE DOWN");
		}
		if (Input::WasKeyPressed('A'))
		{
			Vec3 currentPos = GetPosition(targetTransform);
			currentPos.x += -1;
			SetPosition(targetTransform, currentPos);
			LOG_INFO("MOVE LEFT");
		}
		if (Input::WasKeyPressed('D'))
		{
			Vec3 currentPos = GetPosition(targetTransform);
			currentPos.x += 1;
			SetPosition(targetTransform, currentPos);
			LOG_INFO("MOVE RIGHT");
		}
	}

	void OnDestroy() override {}
	void OnEnable() override {}
	void OnDisable() override {}
	void OnValidate() override {}

	const char* GetTypeName() const override {
		return "MatchingPuzzle";
	}

	// Collision callbacks
	void OnCollisionEnter(Entity other) override {}
	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

private:
	// Exposed fields
	// Component reference - will show as "0" until you assign an entity
	TransformRef targetTransform; // this is the start
	TransformRef endTransform; // this is the end

	TransformRef transform0;
	TransformRef transform1;
	TransformRef transform2;
	TransformRef transform3;
	TransformRef transform4;
	TransformRef transform5;
	TransformRef transform6;
	TransformRef transform7;
	TransformRef transform8;
	TransformRef transform9;
	TransformRef transform10;
	TransformRef transform11;

	// Not Exposed fields
	enum Direction : uint8_t 
	{
		NONE = 0b0000,  // 0
		UP = 0b0001,    // 1
		RIGHT = 0b0010, // 2
		DOWN = 0b0100,  // 4
		LEFT = 0b1000,   // 8
		ALL = 0b1111    // 15
	};

	// 4 columns × 3 rows grid
	/*
	grid[0][0]  grid[0][1]  grid[0][2]  grid[0][3]   <- Row 0 (top)
	grid[1][0]  grid[1][1]  grid[1][2]  grid[1][3]   <- Row 1  
	grid[2][0]  grid[2][1]  grid[2][2]  grid[2][3]   <- Row 2 (bottom)
	*/
	std::array<std::array<Direction, 4>, 3> grid;
};