#pragma once
#include "Scripting/IScript.hpp"
#include "Input/InputManager.hpp"
#include "ExposedFieldRegistry.hpp"
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
class PhysicsPlayerController : public IScript {
public:
	PhysicsPlayerController() {
		// Register editable fields
		REGISTER_FIELD(moveSpeed);
		REGISTER_FIELD(jumpForce);
		REGISTER_FIELD(manualGravity);
		REGISTER_FIELD(frictionCoefficient);
		REGISTER_FIELD(maxSlopeAngle);
		REGISTER_FIELD(groundCheckThreshold);
		REGISTER_FIELD(useRaycastGroundCheck);

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

		// Hook into collider callbacks for ground detection
		if (m_componentManager && m_componentManager->HasComponent<NE::ECS::Component::Collider>(GetEntity())) {
			auto& collider = m_componentManager->GetComponent<NE::ECS::Component::Collider>(GetEntity());
			m_colliderHalfHeight = collider.halfExtents.y;

			// Register our ground detection callbacks
			// IMPORTANT: Don't access components in these callbacks - just queue the events
			collider.onCollisionEnter = [this](NE::ECS::Entity other) {
				m_pendingCollisionEnters.push_back(other);
				};

			collider.onCollisionExit = [this](NE::ECS::Entity other) {
				m_pendingCollisionExits.push_back(other);
				};

			SPD_INFO("Player collider half height: " << m_colliderHalfHeight);
			SPD_INFO("Ground detection callbacks registered");
		}

		SPD_INFO("PhysicsPlayerController started for entity " << GetEntity());
		SPD_INFO("Ground detection mode: " << (useRaycastGroundCheck ? "RAYCAST" : "COLLISION-BASED"));
		SPD_INFO("Physics gravity disabled - using manual gravity");

		if (enableForwardRaycast) {
			SPD_INFO("Forward raycast detection enabled - Press Z to interact");
		}
	}

	void Update(double deltaTime) override {
		if (!HasRigidbody()) return;

		// CRITICAL: Process pending collision events FIRST, after physics step completes
		// This is safe because we're now outside the physics callback context
		ProcessPendingCollisions();

		// Ensure rotation lock stays active
		LockRotation(true, false, true);

		// 1. GROUND CHECK - Using collision detection or optional raycast
		bool isGrounded = CheckIfGrounded();

		// Debug: Log grounded state changes
		static bool wasGrounded = false;
		if (isGrounded != wasGrounded) {
			SPD_INFO("Grounded state changed: " << (isGrounded ? "TRUE" : "FALSE")
				<< " (contacts: " << m_groundContacts.size() << ")");
			wasGrounded = isGrounded;
		}

		// 2. Get current velocity
		NE::Math::Vec3 velocity = GetVelocity();

		// 3. JUMPING - Check if we should jump
		bool attemptingJump = HandleJump(velocity, isGrounded);

		// 4. MOVEMENT & GRAVITY
		HandleMovementAndGravity(velocity, deltaTime, attemptingJump, isGrounded);

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
	 * Process collision events that were queued during physics callbacks.
	 * MUST be called in Update(), after physics step completes, when it's safe to access components.
	 * If crash at vector xmath its probably due to here
	 */
	void ProcessPendingCollisions() {
		// Process collision enters
		for (NE::ECS::Entity other : m_pendingCollisionEnters) {
			HandleCollisionEnter(other);
		}
		m_pendingCollisionEnters.clear();

		// Process collision exits
		for (NE::ECS::Entity other : m_pendingCollisionExits) {
			HandleCollisionExit(other);
		}
		m_pendingCollisionExits.clear();
	}

	/**
	 * Handle collision enter - called from collider callback
	 */
	void HandleCollisionEnter(NE::ECS::Entity other) {
		// Don't collide with ourselves
		if (other == GetEntity()) {
			return;
		}

		// Check if this collision is with something below us (ground)
		if (IsEntityBelowPlayer(other)) {
			m_groundContacts.insert(other);
			SPD_INFO("Ground contact added with entity " << other << " (total: " << m_groundContacts.size() << ")");
		}
		else {
			SPD_INFO("Collision with entity " << other << " but not below player (not ground)");
		}
	}

	/**
	 * Handle collision exit - called from collider callback
	 */
	void HandleCollisionExit(NE::ECS::Entity other) {
		// Remove from ground contacts
		if (m_groundContacts.erase(other) > 0) {
			SPD_INFO("Ground contact removed with entity " << other << " (remaining: " << m_groundContacts.size() << ")");
		}
	}

	/**
	 * Check if an entity is below the player (potential ground contact)
	 * NOTE: This accesses components, so must only be called OUTSIDE physics callbacks
	 */
	bool IsEntityBelowPlayer(NE::ECS::Entity other) const {
		if (!m_componentManager) return false;

		// Safety: Validate entity IDs
		if (other == GetEntity()) {
			return false; // Can't be our own ground
		}

		// Get our position (safe because we're in Update, not in callback)
		NE::Math::Vec3 ourPos = GetPosition();
		float ourBottom = ourPos.y - m_colliderHalfHeight;

		// Get other entity's position and collider
		if (!m_componentManager->HasComponent<NE::ECS::Component::Transform>(other)) {
			SPD_WARNING("Entity " << other << " has no Transform component!");
			return false;
		}

		auto& otherTransform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(other);
		float otherY = otherTransform.position.y;

		// Get the other entity's collider height if it exists
		float otherHalfHeight = 0.5f; // default
		if (m_componentManager->HasComponent<NE::ECS::Component::Collider>(other)) {
			auto& otherCollider = m_componentManager->GetComponent<NE::ECS::Component::Collider>(other);
			otherHalfHeight = otherCollider.halfExtents.y;
		}

		float otherTop = otherY + otherHalfHeight;

		// Check if the other entity's top surface is near our bottom
		// We're standing on it if our bottom is within threshold of their top
		float heightDiff = ourBottom - otherTop;

		bool isBelow = heightDiff >= -groundCheckThreshold && heightDiff <= groundCheckThreshold;

		// Only log on first few checks or when debug is enabled
		static int logCount = 0;
		if (debugRaycastInfo && logCount < 5) {
			SPD_INFO("IsEntityBelowPlayer check for entity " << other << ":");
			SPD_INFO("  Our bottom: " << ourBottom << ", Other top: " << otherTop);
			SPD_INFO("  Height diff: " << heightDiff << ", Threshold: " << groundCheckThreshold);
			SPD_INFO("  Result: " << (isBelow ? "YES (ground)" : "NO (not ground)"));
			logCount++;
		}

		return isBelow;
	}

	/**
	 * Determine if player is grounded using collision-based or raycast method
	 */
	bool CheckIfGrounded() const {
		// If moving upward significantly, we're not grounded (even if touching ground)
		// This handles the brief moment after jumping where collision exit hasn't fired yet
		NE::Math::Vec3 velocity = GetVelocity();
		if (velocity.y > 1.0f) {  // Moving upward faster than 1 unit/sec = definitely airborne
			return false;
		}

		if (useRaycastGroundCheck) {
			// Optional raycast mode (for compatibility)
			NE::Math::Vec3 origin = GetPosition();
			origin.y -= m_colliderHalfHeight; // Start at feet

			NE::Math::Vec3 downDirection{ 0, -1, 0 };
			uint32_t layerMask = 0xFFFFFFFF;

			// Very short raycast - just checking if ground is directly below
			IScript::RaycastHit hit = Raycast(origin, downDirection, 0.1f, layerMask);
			return hit.hasHit && hit.distance <= 0.1f;
		}
		else {
			// Collision-based detection (default and recommended)
			return !m_groundContacts.empty();
		}
	}

	/**
	 * Perform forward raycast to detect what the player is looking at.
	 */
	void HandleForwardDetection() {
		// Continuous check every frame
		if (continuousForwardCheck) {
			PerformForwardRaycast(false); // Don't spam console
		}

		// Manual check when pressing Z
		if (NE::InputManager::WasKeyPressed(GLFW_KEY_Z)) {
			SPD_INFO("Z pressed - Performing forward raycast...");
			PerformForwardRaycast(true); // Verbose output

			// If looking at something, interact with it
			if (m_lookingAtEntity != 0) {
				OnInteractWithEntity(m_lookingAtEntity);
			}
		}
	}

	/**
	 * Cast forward rays to detect entities in front of player
	 */
	void PerformForwardRaycast(bool verbose) {
		NE::Math::Vec3 forward = GetForward();
		NE::Math::Vec3 playerPos = GetPosition();

		// Start raycast slightly in front and at custom height
		NE::Math::Vec3 origin = playerPos;
		origin.x += forward.x * forwardRaycastStartOffset;
		origin.y += targetHeightOffset;
		origin.z += forward.z * forwardRaycastStartOffset;

		// Try multiple height offsets for better detection
		std::vector<float> heightOffsets = { 0.0f, 0.3f, -0.3f };
		uint32_t layerMask = 0xFFFFFFFF;

		bool foundHit = false;
		IScript::RaycastHit bestHit;
		float closestDistance = forwardRaycastDistance + 1.0f;

		// Try each height offset
		for (float heightOffset : heightOffsets) {
			NE::Math::Vec3 adjustedOrigin = origin;
			adjustedOrigin.y += heightOffset;

			IScript::RaycastHit hit = Raycast(adjustedOrigin, forward, forwardRaycastDistance, layerMask);

			// Skip self-hits
			if (hit.hasHit && hit.entity != GetEntity() && hit.distance < closestDistance) {
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

		// Verbose output when requested
		if (verbose) {
			if (foundHit) {
				SPD_INFO("----------------------------------------");
				SPD_INFO("     FORWARD RAYCAST HIT!            ");
				SPD_INFO("----------------------------------------");
				SPD_INFO("Hit Entity: " << bestHit.entity);
				SPD_INFO("Distance: " << bestHit.distance << " units");
				//SPD_INFO("Hit Point: (" << bestHit.point.x << ", " << bestHit.point.y << ", " << bestHit.point.z << ")");
				SPD_INFO("----------------------------------------");
			}
			else {
				SPD_INFO("----------------------------------------");
				SPD_INFO("Forward raycast: NO HIT");
				SPD_INFO("----------------------------------------");
			}
		}
	}

	void OnStartLookingAt(NE::ECS::Entity entity) {
		if (debugRaycastInfo) {
			SPD_INFO("Started looking at entity " << entity);
		}

		// Store and apply highlight scale
		if (m_componentManager && m_componentManager->HasComponent<NE::ECS::Component::Transform>(entity)) {
			auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(entity);
			m_originalScale = transform.scale;
			transform.scale = m_originalScale * highlightScaleMultiplier;
			transform.isDirty = true;
		}
	}

	void OnStopLookingAt(NE::ECS::Entity entity) {
		if (debugRaycastInfo) {
			SPD_INFO("Stopped looking at entity " << entity);
		}

		// Restore original scale
		if (m_componentManager && m_componentManager->HasComponent<NE::ECS::Component::Transform>(entity)) {
			auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(entity);
			transform.scale = m_originalScale;
			transform.isDirty = true;
		}
	}

	void OnInteractWithEntity(NE::ECS::Entity entity) {
		SPD_INFO("========================================");
		SPD_INFO("    INTERACTING WITH ENTITY " << entity);
		SPD_INFO("========================================");
	}

	void HandleMovementAndGravity(NE::Math::Vec3& velocity, double deltaTime,
		bool attemptingJump, bool isGrounded) {

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
		else if (isGrounded) {
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
			}
			else {
				// Stop downward velocity when grounded
				if (newVelocity.y < 0.0f) {
					newVelocity.y = 0.0f;
				}
			}
		}
		else {
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

				// CRITICAL: Clear ground contacts immediately when jumping
				// This ensures isGrounded becomes false right away
				m_groundContacts.clear();
				SPD_INFO("Ground contacts cleared for jump");

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
	float manualGravity = -9.81f;
	float frictionCoefficient = 20.0f;
	float maxSlopeAngle = 45.0f;

	// Ground detection
	float groundCheckThreshold = 0.2f;  // Distance tolerance for ground detection (in units)
	bool useRaycastGroundCheck = false; // If true, use short raycast instead of collision detection

	// Forward raycast parameters
	bool enableForwardRaycast = true;
	float forwardRaycastDistance = 10.0f;
	float forwardRaycastHeightOffset = 0.5f;
	float forwardRaycastStartOffset = 1.5f;
	float targetHeightOffset = 1.0f;
	bool continuousForwardCheck = true;
	float highlightScaleMultiplier = 1.2f;
	bool debugRaycastInfo = true;

	// Internal state
	bool m_hasJumpedThisFrame = false;
	NE::ECS::Entity m_lookingAtEntity = 0;
	NE::Math::Vec3 m_originalScale = { 1.0f, 1.0f, 1.0f };
	float m_colliderHalfHeight = 0.5f;

	// Collision-based ground detection
	std::unordered_set<NE::ECS::Entity> m_groundContacts;

	// Pending collisions (queued during physics callbacks, processed in Update)
	std::vector<NE::ECS::Entity> m_pendingCollisionEnters;
	std::vector<NE::ECS::Entity> m_pendingCollisionExits;

	// Field registry for editor
	ExposedFieldRegistry m_fields;
};