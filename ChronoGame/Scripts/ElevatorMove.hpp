#pragma once
#include "EngineAPI.hpp"

/**
 * ElevatorMove - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class ElevatorMove : public IScript {
public:
	ElevatorMove() {
		// Register any editable fields here
		// Example: REGISTER_FIELD(speed);
		// Example: REGISTER_VECTOR(enemies);
		SCRIPT_FIELD(startPos, Vec3);
		SCRIPT_FIELD(targetPos, Vec3);
		SCRIPT_FIELD_VECTOR(entityToMove, Entity); // why is there no singular inspector for entity xd
		SCRIPT_FIELD(moveDuration, Float);
		SCRIPT_FIELD(listenToMessage, String); 
	}

	~ElevatorMove() override = default;

	// === Lifecycle Methods ===

	void Awake() override {
		// Called when the script component is first created
	}

	void Initialize(Entity entity) override {
		// Called to initialize the script with its entity
	}

	void Start() override {
		// Called when the script is enabled and play mode starts
		if (entityToMove.size() == 0){
			entityToMove.resize(1);
			entityToMove[0] = GetEntity();
			SetPosition(GetTransformRef(entityToMove[0]), startPos);
		}

		if (entityToMove[0]){
			SetPosition(GetTransformRef(entityToMove[0]), startPos);

			Events::Listen(listenToMessage.c_str(), [this](void* data) {
				this->MoveToTargetPos();
				});
		}


		isMoving = false;
		moveTimer = moveDuration;
	}

	void Update(double deltaTime) override {
		// Called every frame while the script is enabled
		if (isMoving)
		{
			moveTimer -= (float)deltaTime;
			if (moveTimer <= 0.0f)
			{
				isMoving = false;
				std::swap(startPos, targetPos);
				moveTimer = 2.0f;
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
		return "ElevatorMove";
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

	Vec3 startPos;	// Starting position 
	Vec3 targetPos;	// Ending position
	std::vector<Entity> entityToMove;
	std::string listenToMessage = "Lever0";
	float moveDuration = 2.0f;
	float moveTimer = 2.0f;
	bool isMoving = false;

	void MoveToTargetPos()
	{
		if (isMoving)
			return;

		isMoving = true;

		Tweener::StartVec3([this](const Vec3& pos) { SetPosition(pos); },
			startPos, targetPos, moveDuration, NE::Scripting::TweenType::EASE_BOTH, entityToMove[0]);
	}
};
