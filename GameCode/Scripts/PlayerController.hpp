#pragma once
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
#include "../ExposedFieldRegistry.hpp"
#include "Math/Vec3.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/Collider.hpp"
#include <cmath>
#include <Core/SpdLogger.hpp>
#include <bitset>
#include <unordered_set>

// GLFW key codes for arrow keys
#define GLFW_KEY_UP 265
#define GLFW_KEY_DOWN 264
#define GLFW_KEY_LEFT 263
#define GLFW_KEY_RIGHT 262
#define GLFW_KEY_SPACE 32
#define GLFW_KEY_X 88  // Toggle forward raycast detection
#define GLFW_KEY_Z 90  // Interaction key


/**
 * Physics-based 3D player controller with:
 * 1. Lateral movement with manual gravity
 * 2. COLLISION-BASED ground detection (hooks into Collider callbacks)
 * 3. Jumping with physics
 * 4. Slope stability - stays still on slopes when not moving
 *
 * IMPORTANT SETUP NOTES:
 * - This script registers callbacks on the Collider component in Start()
 * - Ground detection works by tracking collision contacts with entities below the player
 * - Collision events are QUEUED during physics callbacks and processed in Update()
 *   to avoid accessing components during the physics step (which causes crashes)
 * - Set groundCheckThreshold to control how close entities need to be to count as ground
 *
 * ARCHITECTURE:
 * - Collision callbacks (during physics step): Just queue entity IDs, NO component access
 * - Update() (after physics step): Process queued collisions, safe to access components
 */
class PlayerController : public IScript {
public:
	PlayerController() {
		REGISTER_FIELD(moveSpeed);
		REGISTER_FIELD(jumpForce);
		REGISTER_FIELD(manualGravity);
		REGISTER_FIELD(frictionCoefficient);
		REGISTER_FIELD(maxSlopeAngle);
		REGISTER_FIELD(groundRaycastDistance);
	}

	~PlayerController() override = default;

	void Awake() override {}

	void Initialize(NE::ECS::Entity entity) override {
		SetEntity(entity);
	}

	void Start() override {
		// Configure rigidbody for character
		//SetUseGravity(false);  // CRITICAL: Disable physics engine gravity
		SetMass(70.0f);        // 70kg player mass

		// Lock rotations to prevent tipping
		LockRotation(true, false, true); // Lock X and Z, allow Y for turning

	}

	void Update(double deltaTime) override {
		// Ensure rotation lock stays active
		LockRotation(true, false, true);

		// 1. GROUND CHECK - Using collision detection or optional raycast
		bool isGrounded = CheckIfGrounded();

		// Debug: Log grounded state changes
		static bool wasGrounded = false;
		if (isGrounded != wasGrounded) {
			wasGrounded = isGrounded;
		}

		// 2. Get current velocity
		NE::Math::Vec3 velocity = GetVelocity();

		// 3. JUMPING - Check if we should jump
		bool attemptingJump = HandleJump(velocity, isGrounded);

		// 4. MOVEMENT & GRAVITY
		HandleMovementAndGravity(velocity, deltaTime, attemptingJump, isGrounded);
	}

	void OnDestroy() override {}
	void OnEnable() override {}
	void OnDisable() override {}

	const char* GetTypeName() const override {
		return "PhysicsPlayerController";
	}

	// ========================================
	// COLLISION-BASED GROUND DETECTION
	// ========================================

	void OnCollisionEnter(NE::ECS::Entity other) override {
		// Not used - we hook into collider callbacks directly
	}

	void OnCollisionExit(NE::ECS::Entity other) override {
		// Not used - we hook into collider callbacks directly
	}

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

	/**
	 * Determine if player is grounded using collision-based or raycast method
	 */
	bool CheckIfGrounded() const {
		NE::Math::Vec3 velocity = GetVelocity();
		if (velocity.y > 1.0f) {
			return false; // going up
		}

		NE::Math::Vec3 origin = GetPosition();
		origin.y -= m_colliderHalfHeight;

		NE::Math::Vec3 downDirection{ 0, -1, 0 };
		uint32_t layerMask = 0xFFFFFFFF;

		IScript::RaycastHit hit = Raycast(origin, downDirection, groundRaycastDistance, layerMask);

		// Debug
		//SPD_DEBUG("Ground ray: hasHit=" << hit.hasHit
		//	<< " dist=" << hit.distance
		//	<< " entity=" << hit.entity
		//	<< " self=" << GetEntity());

		// 1) No hit at all
		if (!hit.hasHit) {
			return false;
		}

		// 2) Hit self? ignore it.
		if (hit.entity == GetEntity()) {
			SPD_DEBUG("Ground ray hit self -> treating as not grounded");
			return false;
		}

		// 3) Actual ground hit
		return hit.distance <= groundRaycastDistance;
	}

	void HandleMovementAndGravity(NE::Math::Vec3& velocity, double deltaTime,
		bool attemptingJump, bool isGrounded) {

		//auto camTransform = GetTransform(7);
		////NE::Math::Vec3 camForward = camTransform.GetForward();  // or Normalize manually
		////NE::Math::Vec3 camRight = camTransform.GetRight();
		//float yaw = camTransform.rotation.y * 0.017453292519943295f; // deg?rad

		//NE::Math::Vec3 camForward{
		//	sinf(yaw),
		//	0.0f,
		//	-cosf(yaw)
		//};
		//camForward = camForward.Normalized();

		//NE::Math::Vec3 camRight{
		//	camForward.z,
		//	0.0f,
		//	-camForward.x
		//};

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
		//if (NE::InputManager::IsKeyDown('W')) inputDirection += camForward;
		//if (NE::InputManager::IsKeyDown('S')) inputDirection -= camForward;
		//if (NE::InputManager::IsKeyDown('A')) inputDirection += camRight;
		//if (NE::InputManager::IsKeyDown('D')) inputDirection -= camRight;

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
			newVelocity.x = inputDirection.x * moveSpeed;
			newVelocity.z = inputDirection.z * moveSpeed;
		} else if (isGrounded) {
			// Apply friction
			float horizontalSpeed = std::sqrt(
				newVelocity.x * newVelocity.x +
				newVelocity.z * newVelocity.z
			);

			if (horizontalSpeed > 0.01f) {
				float frictionForce = frictionCoefficient * static_cast<float>(deltaTime) * 100.0f;
				float speedReduction = std::min(frictionForce, horizontalSpeed);

				float factor = (horizontalSpeed - speedReduction) / horizontalSpeed;
				if (factor < 0.0f) factor = 0.0f;

				newVelocity.x *= factor;
				newVelocity.z *= factor;

				if (std::abs(newVelocity.x) < 0.01f) newVelocity.x = 0.0f;
				if (std::abs(newVelocity.z) < 0.01f) newVelocity.z = 0.0f;
			}
		}

		// === VERTICAL MOVEMENT ===
		if (isGrounded) {
			if (attemptingJump) {
				// Apply jump impulse
				newVelocity.y = jumpForce / 70.0f;
				SPD_INFO("JUMP! velocity.y = " << newVelocity.y);
			} else {
				// Stop downward velocity when grounded
				if (newVelocity.y < 0.0f) {
					newVelocity.y = 0.0f;
				}
			}
		} else {
			// Apply gravity when airborne
			newVelocity.y += manualGravity * static_cast<float>(deltaTime);

			// Debug: Log velocity during fall
			static int fallLogCounter = 0;
			if (fallLogCounter++ % 30 == 0) {  // Log every 30 frames
				SPD_INFO("Airborne: velocity.y = " << newVelocity.y << ", deltaTime = " << deltaTime);
			}
		}

		SetVelocity(newVelocity);
	}

	bool HandleJump(NE::Math::Vec3& velocity, bool isGrounded) {
		if (NE::InputManager::WasKeyPressed(GLFW_KEY_SPACE)) {
			if (isGrounded && !m_hasJumpedThisFrame) {
				SPD_INFO("Jump input registered!");
				m_hasJumpedThisFrame = true;

				return true;
			}
		}

		if (!NE::InputManager::IsKeyDown(GLFW_KEY_SPACE)) {
			m_hasJumpedThisFrame = false;
		}

		return false;
	}

	// === EDITABLE PARAMETERS ===

	// Movement parameters
	float moveSpeed = 5.0f;
	float jumpForce = 400.0f;
	float manualGravity = -18.81f;
	float frictionCoefficient = 20.0f;
	float maxSlopeAngle = 45.0f;

	// Ground detection
	float groundRaycastDistance = 0.2f;  // Distance tolerance for ground detection (in units)

	// Internal state
	bool m_hasJumpedThisFrame = false;
	NE::ECS::Entity m_lookingAtEntity = 0;
	NE::Math::Vec3 m_originalScale = { 1.0f, 1.0f, 1.0f };
	float m_colliderHalfHeight = 0.501f;

	// Field registry for editor
	ExposedFieldRegistry m_fields;
};