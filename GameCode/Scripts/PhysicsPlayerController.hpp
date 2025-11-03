#pragma once
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
#include "ExposedFieldRegistry.hpp"
#include "Math/Vec3.hpp"
#include "ECS/Components/Transform.hpp"
#include <cmath>
#include <Core/SpdLogger.hpp>
#include <bitset>

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
 * 2. Ground check via RAYCAST (accurate detection)
 * 3. Jumping with physics
 * 4. Slope stability - stays still on slopes when not moving
 * 5. Kinematic positioning when grounded (no velocity sinking)
 * 6. Forward raycast detection for interaction/targeting
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

		// Forward raycast fields
		REGISTER_FIELD(enableForwardRaycast);
		REGISTER_FIELD(forwardRaycastDistance);
		REGISTER_FIELD(forwardRaycastHeightOffset);
		REGISTER_FIELD(forwardRaycastStartOffset);
		REGISTER_FIELD(targetHeightOffset);
		REGISTER_FIELD(continuousForwardCheck);
		REGISTER_FIELD(highlightScaleMultiplier);
		REGISTER_FIELD(debugRaycastInfo);
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

		if (enableForwardRaycast) {
			SPD_INFO("Forward raycast detection enabled - Press E to interact");
			SPD_INFO("Raycast distance: " << forwardRaycastDistance << " units");
		}
	}

	void Update(double deltaTime) override {
		if (!HasRigidbody()) return;

		// Ensure rotation lock stays active
		LockRotation(true, false, true);

		// 1. GROUND CHECK - Using RAYCAST for accurate detection
		GroundCheckResult groundCheck = CheckGroundWithNormal();

		// 2. Get current velocity
		NE::Math::Vec3 velocity = GetVelocity();

		// DEBUG: Check if gravity is actually disabled
		bool gravityEnabled = GetUseGravity();

		// 3. JUMPING - Check if we should jump
		bool attemptingJump = HandleJump(velocity, groundCheck.isGrounded);

		// 4. MOVEMENT & GRAVITY
		HandleMovementAndGravity(velocity, deltaTime, attemptingJump, groundCheck);

		// 5. TOGGLE FORWARD RAYCAST DETECTION
		if (NE::InputManager::WasKeyPressed(GLFW_KEY_X))
		{
			enableForwardRaycast = !enableForwardRaycast;
			SPD_INFO("enableForwardRaycast :" << enableForwardRaycast);
		}

		// 6. FORWARD RAYCAST DETECTION
		if (enableForwardRaycast) {
			HandleForwardDetection();
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

	// ========================================
	// FORWARD RAYCAST DETECTION (FIXED!)
	// ========================================

	/**
	 * Calculate the forward direction vector from the player's rotation.
	 * FIXED: Now properly calculates forward based on rotation
	 */
	NE::Math::Vec3 GetForwardVector() const {
		NE::Math::Vec3 rotation = GetRotation(); // (pitch, yaw, roll)

		// Convert degrees to radians
		float pitch = rotation.x * (3.14159265f / 180.0f);
		float yaw = rotation.y * (3.14159265f / 180.0f);

		// Calculate forward vector from Euler angles
		NE::Math::Vec3 forward;
		forward.x = std::cos(pitch) * std::sin(yaw);
		forward.y = -std::sin(pitch);
		forward.z = -std::cos(pitch) * std::cos(yaw);

		// Normalize to get unit vector
		float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
		if (length > 0.0001f) {
			forward.x /= length;
			forward.y /= length;
			forward.z /= length;
		}

		return forward;
	}

	/**
	 * Perform forward raycast to detect what the player is looking at.
	 * FIXED VERSION with proper offset and debug info
	 */
	void HandleForwardDetection() {
		// Continuous check every frame
		if (continuousForwardCheck) {
			PerformForwardRaycast(false); // Don't spam console
		}

		// Manual check when pressing E
		if (NE::InputManager::WasKeyPressed(GLFW_KEY_Z)) {
			SPD_INFO("E pressed - Performing forward raycast...");
			PerformForwardRaycast(true); // Show detailed info

			// If we hit something, trigger interaction
			if (m_lookingAtEntity != 0) {
				OnInteractWithEntity(m_lookingAtEntity);
			}
			else {
				SPD_WARNING("No entity detected in front!");
			}
		}
	}

	/**
	 * Perform the actual forward raycast.
	 * FIXED: Proper forward offset and adjustable target height
	 */
	void PerformForwardRaycast(bool verbose) {
		// Get player position
		NE::Math::Vec3 playerPos = GetPosition();

		// Get forward direction
		NE::Math::Vec3 forward = GetForwardVector();

		// Create MULTIPLE raycasts at different heights to increase hit chances
		// This ensures we can hit objects even if they're at different Y positions
		std::vector<float> heightOffsets = {
			forwardRaycastHeightOffset,          // Eye level (default)
			forwardRaycastHeightOffset + 0.5f,   // Above
			forwardRaycastHeightOffset - 0.5f,   // Below
			targetHeightOffset                    // Custom target height
		};

		bool foundHit = false;
		IScript::RaycastHit bestHit;
		float closestDistance = forwardRaycastDistance + 1.0f;

		// Try raycasts at different heights
		for (float heightOffset : heightOffsets) {
			NE::Math::Vec3 origin = playerPos;
			origin.y += heightOffset;

			// FIXED: Offset along the FORWARD DIRECTION, not just X axis!
			// This prevents the raycast from starting inside the player collider
			origin = origin + forward * forwardRaycastStartOffset;

			// Debug output for first raycast
			if (verbose && heightOffset == forwardRaycastHeightOffset) {
				SPD_INFO("═══════════════════════════════════════");
				SPD_INFO("RAYCAST DEBUG INFO:");
				SPD_INFO("Player Position: (" << playerPos.x << ", " << playerPos.y << ", " << playerPos.z << ")");
				SPD_INFO("Player Rotation: (" << GetRotation().x << ", " << GetRotation().y << ", " << GetRotation().z << ")");
				SPD_INFO("Forward Vector: (" << forward.x << ", " << forward.y << ", " << forward.z << ")");
				SPD_INFO("Raycast Origin: (" << origin.x << ", " << origin.y << ", " << origin.z << ")");
				SPD_INFO("Max Distance: " << forwardRaycastDistance);
				SPD_INFO("═══════════════════════════════════════");
			}

			// Perform raycast
			uint32_t layerMask = 0xFFFFFFFF;  // Check all layers
			IScript::RaycastHit hit = Raycast(origin, forward, forwardRaycastDistance, layerMask);

			// IMPORTANT: Ignore hits on ourselves!
			if (hit.hasHit && hit.entity == GetEntity()) {
				// We hit ourselves, try a second raycast starting further forward
				NE::Math::Vec3 newOrigin = origin + forward * (forwardRaycastStartOffset + 0.5f);
				hit = Raycast(newOrigin, forward, forwardRaycastDistance - forwardRaycastStartOffset - 0.5f, layerMask);
			}

			// Keep the closest hit
			if (hit.hasHit && hit.distance < closestDistance) {
				foundHit = true;
				bestHit = hit;
				closestDistance = hit.distance;
			}
		}

		// Update what we're looking at
		NE::ECS::Entity previousEntity = m_lookingAtEntity;
		m_lookingAtEntity = foundHit ? bestHit.entity : 0;

		// Entity changed - trigger callbacks
		if (previousEntity != m_lookingAtEntity) {
			if (previousEntity != 0) {
				OnStopLookingAt(previousEntity);
			}
			if (m_lookingAtEntity != 0) {
				OnStartLookingAt(m_lookingAtEntity);
			}
		}

		// Verbose output when requested (e.g., pressing E)
		if (verbose) {
			if (foundHit) {
				SPD_INFO("----------------------------------------");
				SPD_INFO("     FORWARD RAYCAST HIT!            ");
				SPD_INFO("----------------------------------------");
				SPD_INFO("Hit Entity: " << bestHit.entity);
				SPD_INFO("Distance: " << bestHit.distance << " units");
				SPD_INFO("Hit Point: (" << bestHit.point.x << ", " << bestHit.point.y << ", " << bestHit.point.z << ")");
				SPD_INFO("Surface Normal: (" << bestHit.normal.x << ", " << bestHit.normal.y << ", " << bestHit.normal.z << ")");
				SPD_INFO("----------------------------------------");
			}
			else {
				SPD_INFO("----------------------------------------");
				SPD_INFO("Forward raycast: NO HIT");
				SPD_INFO("Checked distance: " << forwardRaycastDistance << " units");
				SPD_INFO("Try:");
				SPD_INFO("  1. Increase 'forwardRaycastDistance' in Inspector");
				SPD_INFO("  2. Adjust 'targetHeightOffset' to match target Y position");
				SPD_INFO("  3. Make sure target has a collider");
				SPD_INFO("----------------------------------------");
			}
		}

		// Extra debug info if enabled
		if (debugRaycastInfo && verbose) {
			SPD_INFO("Debug: Tried " << heightOffsets.size() << " height offsets");
			SPD_INFO("Debug: Forward direction magnitude: " <<
				std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z));
		}
	}

	/**
	 * Called when player starts looking at an entity.
	 */
	void OnStartLookingAt(NE::ECS::Entity entity) {
		if (debugRaycastInfo) {
			SPD_INFO("Started looking at entity " << entity);
		}

		// Store original scale before modifying
		if (m_componentManager && m_componentManager->HasComponent<NE::ECS::Component::Transform>(entity)) {
			auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(entity);
			m_originalScale = transform.scale;  // Remember original size

			// Apply highlight scale
			transform.scale = m_originalScale * highlightScaleMultiplier;
			transform.isDirty = true;

			SPD_INFO("Highlighting entity " << entity << " (scale: x" << highlightScaleMultiplier << ")");
		}
	}

	/**
	 * Called when player stops looking at an entity.
	 */
	void OnStopLookingAt(NE::ECS::Entity entity) {
		if (debugRaycastInfo) {
			SPD_INFO("Stopped looking at entity " << entity);
		}

		// Restore original scale
		if (m_componentManager && m_componentManager->HasComponent<NE::ECS::Component::Transform>(entity)) {
			auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(entity);
			transform.scale = m_originalScale;  // Restore original size
			transform.isDirty = true;

			SPD_INFO("↩️  Restored entity " << entity << " to original scale");
		}
	}

	/**
	 * Called when player presses E while looking at an entity.
	 */
	void OnInteractWithEntity(NE::ECS::Entity entity) {
		SPD_INFO("----------------------------------------");
		SPD_INFO("|    INTERACTING WITH ENTITY!          |");
		SPD_INFO("----------------------------------------");
		SPD_INFO("Entity: " << entity);
		SPD_INFO("----------------------------------------");

		// Add your interaction code here
	}

	// ========================================
	// END FORWARD RAYCAST DETECTION
	// ========================================

	void HandleMovementAndGravity(NE::Math::Vec3& velocity, double deltaTime,
		bool attemptingJump, const GroundCheckResult& groundCheck) {

		// Get input for all 4 directions
		NE::Math::Vec3 inputDirection{ 0, 0, 0 };

		if (NE::InputManager::IsKeyDown(GLFW_KEY_UP)) {
			inputDirection.z -= 1.0f;
		}
		if (NE::InputManager::IsKeyDown(GLFW_KEY_DOWN)) {
			inputDirection.z += 1.0f;
		}
		if (NE::InputManager::IsKeyDown(GLFW_KEY_LEFT)) {
			inputDirection.x -= 1.0f;
		}
		if (NE::InputManager::IsKeyDown(GLFW_KEY_RIGHT)) {
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
			newVelocity.x = inputDirection.x * moveSpeed;
			newVelocity.z = inputDirection.z * moveSpeed;
		}
		else if (groundCheck.isGrounded) {
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
		if (groundCheck.isGrounded) {
			if (attemptingJump) {
				newVelocity.y = jumpForce / 70.0f;
			}
			else {
				newVelocity.y = 0.0f;

				if (groundCheck.distance > 0.01f && groundCheck.distance < groundSnapDistance) {
					NE::Math::Vec3 currentPos = GetPosition();
					float snapAmount = groundCheck.distance - 0.01f;
					currentPos.y -= snapAmount;
					SetPosition(currentPos);
				}
			}
		}
		else {
			newVelocity.y += manualGravity * static_cast<float>(deltaTime);
		}

		SetVelocity(newVelocity);
	}

	bool HandleJump(NE::Math::Vec3& velocity, bool isGrounded) {
		if (NE::InputManager::WasKeyPressed(GLFW_KEY_SPACE)) {
			if (isGrounded && !m_hasJumpedThisFrame && !IsCeilingAbove()) {
				SPD_INFO("Jump input registered!");
				m_hasJumpedThisFrame = true;
				return true;
			}
			else if (isGrounded && IsCeilingAbove()) {
				SPD_WARNING("Cannot jump - ceiling too low!");
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
	float groundCheckDistance = 0.5f;     // INCREASED for better detection
	float ceilingCheckDistance = 0.5f;
	float manualGravity = -9.81f;
	float frictionCoefficient = 20.0f;
	float raycastOriginOffset = 1.0f;
	float maxSlopeAngle = 45.0f;
	float groundSnapDistance = 0.1f;

	// Forward raycast parameters (IMPROVED!)
	bool enableForwardRaycast = true;
	float forwardRaycastDistance = 10.0f;      // INCREASED from 5.0 to 10.0
	float forwardRaycastHeightOffset = 0.5f;   // Eye level
	float forwardRaycastStartOffset = 1.5f;    // NEW: How far forward to start raycast (prevents self-hit)
	float targetHeightOffset = 1.0f;           // NEW: Custom height for hitting objects
	bool continuousForwardCheck = true;
	float highlightScaleMultiplier = 1.2f;     // NEW: How much to scale up when looking at entity (1.2 = 20% bigger)
	bool debugRaycastInfo = true;              // NEW: Enable debug output

	// Internal state
	bool m_hasJumpedThisFrame = false;
	NE::ECS::Entity m_lookingAtEntity = 0;
	NE::Math::Vec3 m_originalScale = { 1.0f, 1.0f, 1.0f };  // NEW: Store original scale for restoration

	// Field registry for editor
	ExposedFieldRegistry m_fields;
};