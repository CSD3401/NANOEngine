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
 * 1. Lateral movement with gravity (Arrow keys in all directions)
 * 2. Ground check and jumping
 * 3. Ceiling collision (handled by physics)
 * 4. Slope handling (prevents sliding when standing still)
 */
class PhysicsPlayerController : public IScript {
public:
	PhysicsPlayerController() {
		// Register editable fields
		REGISTER_FIELD(moveSpeed);
		REGISTER_FIELD(jumpForce);
		REGISTER_FIELD(groundCheckThreshold);
		REGISTER_FIELD(gravity);
		REGISTER_FIELD(slopeLimit);
		REGISTER_FIELD(frictionCoefficient);
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
		SetUseGravity(true);
		SetMass(70.0f); // 70kg player mass
		
		SPD_INFO("PhysicsPlayerController started for entity " << GetEntity());
	}

	void Update(double deltaTime) override {
		if (!HasRigidbody()) return;

		// Get current velocity from physics
		NE::Math::Vec3 velocity = GetVelocity();

		// 2. GROUND CHECK - Improved physics-based check  
		bool isGrounded = IsGrounded(velocity);

		// 3. JUMPING - Check if we should jump and get the jump velocity
		float jumpVelocityY = HandleJump(velocity, isGrounded);

		// 1. LATERAL MOVEMENT - Apply jump velocity if jumping
		HandleMovement(velocity, deltaTime, jumpVelocityY, isGrounded);

		// 4. CEILING CHECK - Automatically handled by physics collisions!
		// No code needed - Jolt Physics stops upward movement on collision
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
		NE::Math::Vec3 inputDirection{0, 0, 0};

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
		} else if (isGrounded) {
			// Player is NOT moving AND grounded - apply strong friction to prevent sliding
			// This simulates friction when standing still on slopes
			float horizontalSpeed = std::sqrt(newVelocity.x * newVelocity.x + newVelocity.z * newVelocity.z);
			
			if (horizontalSpeed > 0.01f) {
				// Apply friction force to slow down horizontal movement
				float frictionForce = frictionCoefficient * deltaTime * 100.0f; // Scale factor for responsiveness
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
		// else: in air and not moving - preserve momentum (don't modify X/Z)
		
		// If we're jumping this frame, apply the jump velocity
		if (jumpVelocityY > 0.0f) {
			newVelocity.y = jumpVelocityY;
			SPD_INFO("JUMP! Setting Y velocity to: " << jumpVelocityY);
		}
		// Otherwise, preserve Y velocity from physics (gravity)
		
		// Apply the velocity back
		SetVelocity(newVelocity);
	}

	bool IsGrounded(const NE::Math::Vec3& velocity) const {
		// Improved ground check: 
		// 1. Y velocity must be zero or slightly negative (resting on ground)
		// 2. Not moving upward (which would mean we're jumping/in air)
		bool grounded = velocity.y <= 0.01f && velocity.y >= -groundCheckThreshold;
		
		// Debug output (can be removed later)
		static int frameCount = 0;
		if (++frameCount % 60 == 0) { // Log every 60 frames
			SPD_INFO("Grounded: " << grounded << " | Velocity Y: " << velocity.y);
		}
		
		return grounded;
	}

	float HandleJump(NE::Math::Vec3& velocity, bool isGrounded) {
		// Jump when space pressed and on ground
		if (NE::InputManager::WasKeyPressed(GLFW_KEY_SPACE)) {
			if (isGrounded && !m_hasJumpedThisFrame) {
				m_hasJumpedThisFrame = true;
				// Return the jump velocity (force / mass = acceleration, ~5.7 m/s for 400/70)
				return jumpForce / 70.0f; // Divide by mass to get velocity
			} else if (!isGrounded) {
				SPD_INFO("Can't jump - not grounded (velocity.y = " << velocity.y << ")");
			}
		}
		
		// Reset jump flag when space is released
		if (!NE::InputManager::IsKeyDown(GLFW_KEY_SPACE)) {
			m_hasJumpedThisFrame = false;
		}
		
		return 0.0f; // No jump this frame
	}

	// Editable parameters
	float moveSpeed = 5.0f;            // Horizontal movement speed
	float jumpForce = 400.0f;        // Jump impulse force (mass * velocity, so ~5.7 m/s jump for 70kg)
	float groundCheckThreshold = 1.0f;    // Velocity threshold to detect ground
	float gravity = -9.81f;  // Gravity strength (handled by physics)
	float slopeLimit = 45.0f;     // Maximum slope angle in degrees (not currently used, but available for future)
	float frictionCoefficient = 20.0f;    // Friction when standing still (higher = stops faster on slopes)

	// Internal state
	bool m_hasJumpedThisFrame = false; // Prevent multiple jumps from one press

	// Field registry for editor
	ExposedFieldRegistry m_fields;
};
