#pragma once
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
#include "ExposedFieldRegistry.hpp"
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
	// RAYCAST-BASED GROUND CHECK with Layer Filtering
	bool IsGroundedRaycast() const {
		NE::Math::Vec3 origin = GetPosition();

		SPD_INFO("=== GROUND CHECK DEBUG ===");
		SPD_INFO("Player position: (" << origin.x << ", " << origin.y << ", " << origin.z << ")");

		// Start ray at player's feet (below center)
		origin.y -= raycastOriginOffset;

		SPD_INFO("Ray origin (feet): (" << origin.x << ", " << origin.y << ", " << origin.z << ")");
		SPD_INFO("Ray offset: " << raycastOriginOffset);
		SPD_INFO("Ground check distance: " << groundCheckDistance);

		NE::Math::Vec3 downDirection{ 0, -1, 0 };
		float totalDistance = groundCheckDistance;

		// LAYER FILTERING: Only hit static (ground) objects on layer 0 (NON_MOVING)
		uint32_t layerMask = (1 << 0);  // Only layer 0 = static ground

		SPD_INFO("Layer mask: " << layerMask << " (binary: " << std::bitset<8>(layerMask) << ")");

		// First try WITHOUT layer filter to see if we can hit anything at all
		//IScript::RaycastHit hitAll = Raycast(origin, downDirection, totalDistance, 0xFFFFFFFF);
		//SPD_INFO("Raycast (ALL layers) - HasHit: " << hitAll.hasHit
		//	<< ", HitEntity: " << hitAll.entity
		//	<< ", Distance: " << hitAll.distance);

		// Now try WITH layer filter
		IScript::RaycastHit hit = Raycast(origin, downDirection, totalDistance, layerMask);
		SPD_INFO("Raycast (layer 0 only) - HasHit: " << hit.hasHit
			<< ", HitEntity: " << hit.entity
			<< ", Distance: " << hit.distance);

		if (hit.hasHit && hit.distance <= totalDistance) {
			SPD_INFO("Valid ground hit!");
			return true;
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
