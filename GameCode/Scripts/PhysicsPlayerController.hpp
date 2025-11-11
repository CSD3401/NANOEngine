#pragma once
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
#include "../ExposedFieldRegistry.hpp"
#include "Math/Vec3.hpp"
#include <cmath>
#include <Core/SpdLogger.hpp>
#include <bitset>

// GLFW key codes for arrow keys
#define GLFW_KEY_UP 265
#define GLFW_KEY_DOWN 264
#define GLFW_KEY_LEFT 263
#define GLFW_KEY_RIGHT 262
#define GLFW_KEY_SPACE 32


/**
 * Physics-based 3D player controller with:
 * 1. Lateral movement with manual gravity
 * 2. Ground check via RAYCAST (accurate detection)
 * 3. Jumping with physics
 * 4. Slope stability - stays still on slopes when not moving
 * 5. Kinematic positioning when grounded (no velocity sinking)
 */
class PhysicsPlayerController : public IScript {
public:
	PhysicsPlayerController() {
		// Register editable fields
		REGISTER_FIELD(moveSpeed);
		REGISTER_FIELD(jumpForce);
		REGISTER_FIELD(groundCheckDistance);
		REGISTER_FIELD(ceilingCheckDistance);
		REGISTER_FIELD(manualGravity);
		REGISTER_FIELD(frictionCoefficient);
		REGISTER_FIELD(raycastOriginOffset);
		REGISTER_FIELD(maxSlopeAngle);
		REGISTER_FIELD(groundSnapDistance);
	}

	~PhysicsPlayerController() override = default;

	void Awake() override {}

	void Initialize(NE::ECS::Entity entity) override {
		SetEntity(entity);
	}

	void Start() override {
		// Ensure rigidbody exists
		if (!HasRigidbody()) {
			SPD_ERROR("PhysicsPlayerController requires a Rigidbody component!");
			return;
		}

		// Configure rigidbody for character
		SetUseGravity(false);  // CRITICAL: Disable physics engine gravity
		SetMass(70.0f);        // 70kg player mass

		// Lock rotations to prevent tipping
		LockRotation(true, false, true); // Lock X and Z, allow Y for turning

		SPD_INFO("PhysicsPlayerController started for entity " << GetEntity());
		SPD_INFO("Physics gravity disabled - using manual gravity + kinematic grounding");
	}

	void Update(double deltaTime) override {
		if (!HasRigidbody()) return;

		// Ensure rotation lock stays active
		LockRotation(true, false, true);

		// 1. GROUND CHECK - Using RAYCAST for accurate detection
		GroundCheckResult groundCheck = CheckGroundWithNormal();

		//SPD_INFO("=== FRAME START ===");
		//SPD_INFO("IsGrounded: " << groundCheck.isGrounded
		//	<< " | SlopeAngle: " << groundCheck.slopeAngle
		//	<< " | OnSlope: " << groundCheck.isOnSlope);

		// 2. Get current velocity
		NE::Math::Vec3 velocity = GetVelocity();
		//SPD_INFO("Current velocity: (" << velocity.x << ", " << velocity.y << ", " << velocity.z << ")");

		// DEBUG: Check if gravity is actually disabled
		bool gravityEnabled = GetUseGravity();
		//SPD_INFO("Rigidbody useGravity flag: " << gravityEnabled);

		// 3. JUMPING - Check if we should jump
		bool attemptingJump = HandleJump(velocity, groundCheck.isGrounded);

		// 4. MOVEMENT & GRAVITY
		HandleMovementAndGravity(velocity, deltaTime, attemptingJump, groundCheck);

		if (NE::InputManager::WasKeyPressed('C')) {
			NANOEngine::Events::SendScriptEvent("TimeSwapNow", nullptr);

			SPD_DEBUG("Timer started for Texture switching 5 seconds!");
		}
	
	}

	void OnDestroy() override {}
	void OnEnable() override {}
	void OnDisable() override {}

	const char* GetTypeName() const override {
		return "PhysicsPlayerController";
	}

	// Collision events
	void OnCollisionEnter(NE::ECS::Entity other) override {
		//SPD_INFO("Player collided with entity " << other);
	}

	void OnCollisionExit(NE::ECS::Entity other) override {}
	void OnTriggerEnter(NE::ECS::Entity other) override {}
	void OnTriggerExit(NE::ECS::Entity other) override {}

	// Exposed fields
	std::vector<std::string> GetExposedFieldNames() const override { return m_fields.GetNames(); }
	std::string GetFieldType(const std::string& name) const override { return m_fields.GetType(name); }
	std::string GetFieldValueAsString(const std::string& name) const override { return m_fields.GetValue(name); }
	bool SetFieldValueFromString(const std::string& name, const std::string& value) override {
		return m_fields.SetValue(name, value);
	}

private:
	struct GroundCheckResult {
		bool isGrounded = false;
		bool isOnSlope = false;
		float slopeAngle = 0.0f;
		NE::Math::Vec3 groundNormal = { 0, 1, 0 };
		float distance = 0.0f;
		NE::Math::Vec3 hitPoint = { 0, 0, 0 };
	};

	// Enhanced ground check that also returns surface normal and slope angle
	GroundCheckResult CheckGroundWithNormal() const {
		GroundCheckResult result;

		NE::Math::Vec3 origin = GetPosition();
		origin.y -= raycastOriginOffset; // Start at player's feet

		NE::Math::Vec3 downDirection{ 0, -1, 0 };
		uint32_t layerMask = (1 << 0);  // Only layer 0 = static ground

		// Perform raycast
		IScript::RaycastHit hit = Raycast(origin, downDirection, groundCheckDistance, layerMask);

		if (hit.hasHit && hit.distance <= groundCheckDistance) {
			result.isGrounded = true;
			result.distance = hit.distance;
			result.groundNormal = hit.normal;
			result.hitPoint = hit.point;

			// Calculate slope angle
			float dotProduct = result.groundNormal.y;
			result.slopeAngle = std::acos(dotProduct) * (180.0f / 3.14159f);

			// Check if on a slope
			result.isOnSlope = result.slopeAngle > 0.1f && result.slopeAngle <= maxSlopeAngle;
		}

		return result;
	}

	// RAYCAST-BASED CEILING CHECK
	bool IsCeilingAbove() const {
		NE::Math::Vec3 origin = GetPosition();
		origin.y += raycastOriginOffset; // Start at player's head

		NE::Math::Vec3 upDirection{ 0, 1, 0 };
		uint32_t layerMask = (1 << 0);  // Only layer 0 = static geometry

		IScript::RaycastHit hit = Raycast(origin, upDirection, ceilingCheckDistance, layerMask);

		return (hit.hasHit && hit.distance <= ceilingCheckDistance);
	}

	void HandleMovementAndGravity(NE::Math::Vec3& velocity, double deltaTime,
		bool attemptingJump, const GroundCheckResult& groundCheck) {

		// Get input for all 4 directions
		NE::Math::Vec3 inputDirection{ 0, 0, 0 };

		if (NE::InputManager::IsKeyDown('W')) {
			inputDirection.z -= 1.0f;
		}
		if (NE::InputManager::IsKeyDown('S')) {
			inputDirection.z += 1.0f;
		}
		if (NE::InputManager::IsKeyDown('A')) {
			inputDirection.x -= 1.0f;
		}
		if (NE::InputManager::IsKeyDown('D')) {
			inputDirection.x += 1.0f;
		}

		// Normalize diagonal movement
		float inputMagnitude = std::sqrt(
			inputDirection.x * inputDirection.x +
			inputDirection.z * inputDirection.z
		);

		if (inputMagnitude > 0.01f) {
			inputDirection.x /= inputMagnitude;
			inputDirection.z /= inputMagnitude;
		}

		bool isMoving = inputMagnitude > 0.01f;

		// Get current velocity
		NE::Math::Vec3 newVelocity = GetVelocity();

		// === HORIZONTAL MOVEMENT ===
		if (isMoving) {
			// Player is trying to move - apply input velocity
			newVelocity.x = inputDirection.x * moveSpeed;
			newVelocity.z = inputDirection.z * moveSpeed;
			//SPD_INFO("Moving: velocity XZ = (" << newVelocity.x << ", " << newVelocity.z << ")");
		}
		else if (groundCheck.isGrounded) {
			// Player is NOT moving AND grounded - apply friction
			float horizontalSpeed = std::sqrt(
				newVelocity.x * newVelocity.x +
				newVelocity.z * newVelocity.z
			);

			if (horizontalSpeed > 0.01f) {
				// Apply friction force
				float frictionForce = frictionCoefficient * static_cast<float>(deltaTime) * 100.0f;
				float speedReduction = std::min(frictionForce, horizontalSpeed);

				float factor = (horizontalSpeed - speedReduction) / horizontalSpeed;
				if (factor < 0.0f) factor = 0.0f;

				newVelocity.x *= factor;
				newVelocity.z *= factor;

				// Full stop if very slow
				if (std::abs(newVelocity.x) < 0.01f) newVelocity.x = 0.0f;
				if (std::abs(newVelocity.z) < 0.01f) newVelocity.z = 0.0f;
			}
		}

		// === VERTICAL MOVEMENT (GRAVITY & JUMPING) ===
		if (groundCheck.isGrounded) {
			// === GROUNDED STATE ===
			if (attemptingJump) {
				// JUMPING - Apply upward velocity
				newVelocity.y = jumpForce / 70.0f; // Divide by mass
				//SPD_INFO("JUMPING! Applied Y velocity: " << newVelocity.y);
			}
			else {
				// NOT JUMPING - This is the critical fix for slope stability
				// OPTION 1: Zero velocity (let collision keep us on ground)
				newVelocity.y = 0.0f;

				// OPTION 2: Snap to ground position (kinematic approach)
				// If player is slightly above ground, teleport them down
				if (groundCheck.distance > 0.01f && groundCheck.distance < groundSnapDistance) {
					NE::Math::Vec3 currentPos = GetPosition();
					// Snap to ground surface
					float snapAmount = groundCheck.distance - 0.01f; // Leave tiny gap
					currentPos.y -= snapAmount;
					SetPosition(currentPos);
					//SPD_INFO("Snapped to ground by " << snapAmount << " units");
				}

				//SPD_INFO("Grounded & not jumping: Y velocity = 0");
			}
		}
		else {
			// === IN AIR STATE ===
			// Apply manual gravity
			newVelocity.y += manualGravity * static_cast<float>(deltaTime);
			//SPD_INFO("In air: Applying gravity | New Y velocity: " << newVelocity.y);
		}

		// Apply the new velocity
		SetVelocity(newVelocity);

		//SPD_INFO("Final velocity: (" << newVelocity.x << ", "
		//	<< newVelocity.y << ", " << newVelocity.z << ")");
	}

	bool HandleJump(NE::Math::Vec3& velocity, bool isGrounded) {
		// Jump when space pressed and on ground
		if (NE::InputManager::WasKeyPressed(GLFW_KEY_SPACE)) {
			if (isGrounded && !m_hasJumpedThisFrame && !IsCeilingAbove()) {
				//SPD_INFO("Jump input registered!");
				m_hasJumpedThisFrame = true;
				return true; // Signal to apply jump velocity
			}
			else if (isGrounded && IsCeilingAbove()) {
				SPD_WARNING("Cannot jump - ceiling too low!");
			}
		}

		// Reset jump flag when space is released
		if (!NE::InputManager::IsKeyDown(GLFW_KEY_SPACE)) {
			m_hasJumpedThisFrame = false;
		}

		return false;
	}

	// === EDITABLE PARAMETERS ===
	float moveSpeed = 5.0f;              // Horizontal movement speed
	float jumpForce = 400.0f;            // Jump impulse force
	float groundCheckDistance = 0.1f;    // How far to check for ground (small = more accurate)
	float ceilingCheckDistance = 0.5f;   // How far to check for ceiling
	float manualGravity = -9.81f;        // Manual gravity (negative = downward)
	float frictionCoefficient = 20.0f;   // Friction when standing still
	float raycastOriginOffset = 1.0f;    // Collider half-height
	float maxSlopeAngle = 45.0f;         // Maximum walkable slope angle
	float groundSnapDistance = 0.1f;     // Max distance to snap to ground

	// Internal state
	bool m_hasJumpedThisFrame = false;

	// Field registry for editor
	ExposedFieldRegistry m_fields;
};