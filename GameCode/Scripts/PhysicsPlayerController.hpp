#pragma once
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
#include "ExposedFieldRegistry.hpp"
#include "Math/Vec3.hpp"
#include <cmath>
#include <Core/SpdLogger.hpp>

// GLFW key codes for arrow keys
#define GLFW_KEY_UP 265
#define GLFW_KEY_DOWN 264
#define GLFW_KEY_LEFT 263
#define GLFW_KEY_RIGHT 262
#define GLFW_KEY_SPACE 32

/**
 * Physics-based 3D player controller with:
 * 1. Lateral movement with manual gravity (Arrow keys in all directions)
 * 2. Ground check via RAYCAST (accurate detection)
 * 3. Jumping with physics
 * 4. Rotation locking ONLY when falling (prevents tipping while in air)
 */
class PhysicsPlayerController : public IScript {
public:
	PhysicsPlayerController() {
		// Register editable fields
		REGISTER_FIELD(moveSpeed);
		REGISTER_FIELD(jumpForce);
		REGISTER_FIELD(groundCheckDistance);
		REGISTER_FIELD(manualGravity);
		REGISTER_FIELD(frictionCoefficient);
		REGISTER_FIELD(raycastOriginOffset);
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
		SetUseGravity(false);  // Use manual gravity for better control
		SetMass(70.0f); // 70kg player mass

		SPD_INFO("PhysicsPlayerController started for entity " << GetEntity());
	}

	void Update(double deltaTime) override {
		if (!HasRigidbody()) return;

		// Get current velocity from physics
		NE::Math::Vec3 velocity = GetVelocity();

		// 1. GROUND CHECK - Using RAYCAST for accurate detection
		bool isGrounded = IsGroundedRaycast();
		SPD_INFO("IsGrounded: " << isGrounded);

		// 2. ROTATION LOCKING - Only lock when falling to prevent tipping
		if (!isGrounded) {
			// Player is in air - lock X and Z rotation to prevent tipping over
			LockRotation(true, false, true); // Lock X, unlock Y (turning), lock Z
		}
		else {
			// Player is grounded - allow all rotation (unlock everything)
			LockRotation(false, false, false); // Unlock X, Y, Z
		}

		// 3. JUMPING - Check if we should jump and get the jump velocity
		float jumpVelocityY = HandleJump(velocity, isGrounded);

		// 4. LATERAL MOVEMENT - Apply jump velocity if jumping
		HandleMovement(velocity, deltaTime, jumpVelocityY, isGrounded);
	}

	void OnDestroy() override {}
	void OnEnable() override {}
	void OnDisable() override {}

	const char* GetTypeName() const override {
		return "PhysicsPlayerController";
	}

	// Collision events
	void OnCollisionEnter(NE::ECS::Entity other) override {
		SPD_INFO("Player collided with entity " << other);
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
	void HandleMovement(NE::Math::Vec3& velocity, double deltaTime, float jumpVelocityY, bool isGrounded) {
		// Get input for all 4 directions (Arrow keys)
		NE::Math::Vec3 inputDirection{ 0, 0, 0 };

		// Forward/Backward (UP/DOWN arrows)
		if (NE::InputManager::IsKeyDown(GLFW_KEY_UP)) {
			inputDirection.z -= 1.0f; // Forward in -Z
		}
		if (NE::InputManager::IsKeyDown(GLFW_KEY_DOWN)) {
			inputDirection.z += 1.0f; // Backward in +Z
		}

		// Left/Right (LEFT/RIGHT arrows)
		if (NE::InputManager::IsKeyDown(GLFW_KEY_LEFT)) {
			inputDirection.x -= 1.0f; // Left in -X
		}
		if (NE::InputManager::IsKeyDown(GLFW_KEY_RIGHT)) {
			inputDirection.x += 1.0f; // Right in +X
		}

		// Normalize diagonal movement (so moving diagonally isn't faster)
		float inputMagnitude = std::sqrt(
			inputDirection.x * inputDirection.x +
			inputDirection.z * inputDirection.z
		);

		if (inputMagnitude > 0.01f) {
			inputDirection.x /= inputMagnitude;
			inputDirection.z /= inputMagnitude;
		}

		// Get current velocity
		NE::Math::Vec3 newVelocity = GetVelocity();

		// Determine if player is trying to move
		bool isMoving = inputMagnitude > 0.01f;

		if (isMoving) {
			// Player is trying to move - apply input velocity
			newVelocity.x = inputDirection.x * moveSpeed;
			newVelocity.z = inputDirection.z * moveSpeed;
		}
		else if (isGrounded) {
			// Player is NOT moving AND grounded - apply strong friction
			float horizontalSpeed = std::sqrt(newVelocity.x * newVelocity.x + newVelocity.z * newVelocity.z);

			if (horizontalSpeed > 0.01f) {
				// Apply friction force to slow down horizontal movement
				float frictionForce = frictionCoefficient * static_cast<float>(deltaTime) * 100.0f;
				float speedReduction = std::min(frictionForce, horizontalSpeed);

				// Reduce velocity proportionally
				float factor = (horizontalSpeed - speedReduction) / horizontalSpeed;
				if (factor < 0.0f) factor = 0.0f;

				newVelocity.x *= factor;
				newVelocity.z *= factor;

				// If velocity is very small, set to zero (full stop)
				if (std::abs(newVelocity.x) < 0.01f) newVelocity.x = 0.0f;
				if (std::abs(newVelocity.z) < 0.01f) newVelocity.z = 0.0f;
			}
		}

		// === MANUAL GRAVITY APPLICATION ===
		if (!isGrounded) {
			// In air - apply gravity
			newVelocity.y += manualGravity * static_cast<float>(deltaTime);
		}
		else {
			// On ground - stop falling
			if (newVelocity.y < 0) {
				newVelocity.y = 0;
			}
		}

		// If we're jumping this frame, apply the jump velocity
		if (jumpVelocityY > 0.0f) {
			newVelocity.y = jumpVelocityY;
		}

		// Apply the velocity back
		SetVelocity(newVelocity);
	}

	// RAYCAST-BASED GROUND CHECK
	bool IsGroundedRaycast() const {
		// CRITICAL: Start the ray OUTSIDE the player's collider!
		NE::Math::Vec3 origin = GetPosition();
		
		// Move origin UP by the collider's half-height + offset
		origin.y -= raycastOriginOffset;
		
		NE::Math::Vec3 downDirection{ 0, -1, 0 };
		
		// Total distance = offset to get back to player center + ground check distance
		float totalDistance = raycastOriginOffset + groundCheckDistance;

		// Cast ray downward using base class Raycast method
		IScript::RaycastHit hit = Raycast(origin, downDirection, totalDistance);

		// DEBUG: Log raycast info
		SPD_INFO("Raycast - HasHit: " << hit.hasHit 
			<< ", HitEntity: " << hit.entity 
			<< ", SelfEntity: " << GetEntity() 
			<< ", Distance: " << hit.distance 
			<< ", TotalDistance: " << totalDistance);

		if (hit.hasHit) {
			// Check if we hit ourselves - THIS IS CRITICAL!
			if (hit.entity == GetEntity()) {
				SPD_INFO("Hit self! Ignoring.");
				return false; // Ignore self-hits
			}

			// Only consider grounded if hit distance is reasonable
			// (not too far away from the player's feet)
			if (hit.distance <= totalDistance) {
				SPD_INFO("Valid ground hit!");
				return true;
			}
		}

		return false;
	}

	float HandleJump(NE::Math::Vec3& velocity, bool isGrounded) {
		// Jump when space pressed and on ground
		if (NE::InputManager::WasKeyPressed(GLFW_KEY_SPACE)) {
			if (isGrounded && !m_hasJumpedThisFrame) {
				m_hasJumpedThisFrame = true;
				return jumpForce / 70.0f; // Divide by mass to get velocity
			}
		}

		// Reset jump flag when space is released
		if (!NE::InputManager::IsKeyDown(GLFW_KEY_SPACE)) {
			m_hasJumpedThisFrame = false;
		}

		return 0.0f;
	}

	// Editable parameters
	float moveSpeed = 5.0f;           // Horizontal movement speed
	float jumpForce = 400.0f;         // Jump impulse force
	float groundCheckDistance = 0.2f; // How far below player center to check for ground
	float manualGravity = -9.81f;     // Manual gravity strength
	float frictionCoefficient = 20.0f;// Friction when standing still
	float raycastOriginOffset = 1.0f; // How high above player center to start ray (= collider half-height)

	// Internal state
	bool m_hasJumpedThisFrame = false;

	// Field registry for editor
	ExposedFieldRegistry m_fields;
};
