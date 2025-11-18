#pragma once
#include "EngineAPI.hpp"
#include <cmath>
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
class PhysicsPlayerController : public NE::Scripting::IScript {
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

	void Initialize(NE::Scripting::Entity entity) override {
		// Entity is automatically set by the base class
	}

	void Start() override {
		// Ensure rigidbody exists
		if (!HasRigidbody()) {
			LOG_ERROR("PhysicsPlayerController requires a Rigidbody component!");
			return;
		}

		// Configure rigidbody for character
		SetUseGravity(false);  // CRITICAL: Disable physics engine gravity
		SetMass(70.0f);        // 70kg player mass

		// Lock rotations to prevent tipping
		LockRotation(true, false, true); // Lock X and Z, allow Y for turning

		// Hook into collider callbacks for ground detection
		if (NE::ECS::Command::HasComponent<NE::ECS::Component::Collider>(GetEntity())) {
			auto& collider = NE::ECS::Command::GetComponent<NE::ECS::Component::Collider>(GetEntity());
			m_colliderHalfHeight = collider.halfExtents.y;

			// Register our ground detection callbacks
			// IMPORTANT: Don't access components in these callbacks - just queue the events
			collider.onCollisionEnter = [this](NE::Scripting::Entity other) {
				m_pendingCollisionEnters.push_back(other);
				};

			collider.onCollisionExit = [this](NE::Scripting::Entity other) {
				m_pendingCollisionExits.push_back(other);
				};

			LOG_INFO("Player collider half height: " << m_colliderHalfHeight);
			LOG_INFO("Ground detection callbacks registered");
		}

		LOG_INFO("PhysicsPlayerController started for entity " << GetEntity());
		LOG_INFO("Ground detection mode: " << (useRaycastGroundCheck ? "RAYCAST" : "COLLISION-BASED"));
		LOG_INFO("Physics gravity disabled - using manual gravity");

		if (enableForwardRaycast) {
			LOG_INFO("Forward raycast detection enabled - Press Z to interact");
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
			LOG_INFO("Grounded state changed: " << (isGrounded ? "TRUE" : "FALSE")
				<< " (contacts: " << m_groundContacts.size() << ")");
			wasGrounded = isGrounded;
		}

		// 2. Get current velocity
		NE::Scripting::Vec3 velocity = GetVelocity();

		// 3. JUMPING - Check if we should jump
		bool attemptingJump = HandleJump(velocity, isGrounded);

		// 4. MOVEMENT & GRAVITY
		HandleMovementAndGravity(velocity, deltaTime, attemptingJump, isGrounded);

		// 5. TOGGLE FORWARD RAYCAST DETECTION
		if (Input::WasKeyPressed(GLFW_KEY_X))
		{
			enableForwardRaycast = !enableForwardRaycast;
			LOG_INFO("enableForwardRaycast :" << enableForwardRaycast);
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

	void OnCollisionEnter(NE::Scripting::Entity other) override {
		// Not used - we hook into collider callbacks directly
	}

	void OnCollisionExit(NE::Scripting::Entity other) override {
		// Not used - we hook into collider callbacks directly
	}

	void OnTriggerEnter(NE::Scripting::Entity other) override {}
	void OnTriggerExit(NE::Scripting::Entity other) override {}

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
		for (NE::Scripting::Entity other : m_pendingCollisionEnters) {
			HandleCollisionEnter(other);
		}
		m_pendingCollisionEnters.clear();

		// Process collision exits
		for (NE::Scripting::Entity other : m_pendingCollisionExits) {
			HandleCollisionExit(other);
		}
		m_pendingCollisionExits.clear();
	}

	/**
	 * Handle collision enter - called from collider callback
	 */
	void HandleCollisionEnter(NE::Scripting::Entity other) {
		// Don't collide with ourselves
		if (other == GetEntity()) {
			return;
		}

		// Check if this collision is with something below us (ground)
		if (IsEntityBelowPlayer(other)) {
			m_groundContacts.insert(other);
			LOG_INFO("Ground contact added with entity " << other << " (total: " << m_groundContacts.size() << ")");
		}
		else {
			LOG_INFO("Collision with entity " << other << " but not below player (not ground)");
		}
	}

	/**
	 * Handle collision exit - called from collider callback
	 */
	void HandleCollisionExit(NE::Scripting::Entity other) {
		// Remove from ground contacts
		if (m_groundContacts.erase(other) > 0) {
			LOG_INFO("Ground contact removed with entity " << other << " (remaining: " << m_groundContacts.size() << ")");
		}
	}

	/**
	 * Check if an entity is below the player (potential ground contact)
	 * NOTE: This accesses components, so must only be called OUTSIDE physics callbacks
	 */
	bool IsEntityBelowPlayer(NE::Scripting::Entity other) const {
		// Safety: Validate entity IDs
		if (other == GetEntity()) {
			return false; // Can't be our own ground
		}

		// Get our position (safe because we're in Update, not in callback)
		NE::Scripting::Vec3 ourPos = GetPosition();
		float ourBottom = ourPos.y - m_colliderHalfHeight;

		// Get other entity's position and collider
		if (!NE::ECS::Command::HasComponent<NE::ECS::Component::Transform>(other)) {
			LOG_WARNING("Entity " << other << " has no Transform component!");
			return false;
		}

		auto& otherTransform = NE::ECS::Command::GetComponent<NE::ECS::Component::Transform>(other);
		float otherY = otherTransform.position.y;

		// Get the other entity's collider height if it exists
		float otherHalfHeight = 0.5f; // default
		if (NE::ECS::Command::HasComponent<NE::ECS::Component::Collider>(other)) {
			auto& otherCollider = NE::ECS::Command::GetComponent<NE::ECS::Component::Collider>(other);
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
			LOG_INFO("IsEntityBelowPlayer check for entity " << other << ":");
			LOG_INFO("  Our bottom: " << ourBottom << ", Other top: " << otherTop);
			LOG_INFO("  Height diff: " << heightDiff << ", Threshold: " << groundCheckThreshold);
			LOG_INFO("  Result: " << (isBelow ? "YES (ground)" : "NO (not ground)"));
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
		NE::Scripting::Vec3 velocity = GetVelocity();
		if (velocity.y > 1.0f) {  // Moving upward faster than 1 unit/sec = definitely airborne
			return false;
		}

		if (useRaycastGroundCheck) {
			// Optional raycast mode (for compatibility)
			NE::Scripting::Vec3 origin = GetPosition();
			origin.y -= m_colliderHalfHeight; // Start at feet

			NE::Scripting::Vec3 downDirection{ 0, -1, 0 };
			uint32_t layerMask = 0xFFFFFFFF;

			// Very short raycast - just checking if ground is directly below
			NE::Scripting::RaycastHit hit = Raycast(origin, downDirection, 0.1f, layerMask);
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
		if (Input::WasKeyPressed(GLFW_KEY_Z)) {
			LOG_INFO("Z pressed - Performing forward raycast...");
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
		NE::Scripting::Vec3 forward = GetForward();
		NE::Scripting::Vec3 playerPos = GetPosition();

		// Start raycast slightly in front and at custom height
		NE::Scripting::Vec3 origin = playerPos;
		origin.x += forward.x * forwardRaycastStartOffset;
		origin.y += targetHeightOffset;
		origin.z += forward.z * forwardRaycastStartOffset;

		// Try multiple height offsets for better detection
		std::vector<float> heightOffsets = { 0.0f, 0.3f, -0.3f };
		uint32_t layerMask = 0xFFFFFFFF;

		bool foundHit = false;
		NE::Scripting::RaycastHit bestHit;
		float closestDistance = forwardRaycastDistance + 1.0f;

		// Try each height offset
		for (float heightOffset : heightOffsets) {
			NE::Scripting::Vec3 adjustedOrigin = origin;
			adjustedOrigin.y += heightOffset;

			NE::Scripting::RaycastHit hit = Raycast(adjustedOrigin, forward, forwardRaycastDistance, layerMask);

			// Skip self-hits
			if (hit.hasHit && hit.entity != GetEntity() && hit.distance < closestDistance) {
				foundHit = true;
				bestHit = hit;
				closestDistance = hit.distance;
			}
		}

		// Update what we're looking at
		NE::Scripting::Entity previousEntity = m_lookingAtEntity;
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
				LOG_INFO("----------------------------------------");
				LOG_INFO("     FORWARD RAYCAST HIT!            ");
				LOG_INFO("----------------------------------------");
				LOG_INFO("Hit Entity: " << bestHit.entity);
				LOG_INFO("Distance: " << bestHit.distance << " units");
				//LOG_INFO("Hit Point: (" << bestHit.point.x << ", " << bestHit.point.y << ", " << bestHit.point.z << ")");
				LOG_INFO("----------------------------------------");
			}
			else {
				LOG_INFO("----------------------------------------");
				LOG_INFO("Forward raycast: NO HIT");
				LOG_INFO("----------------------------------------");
			}
		}
	}

	void OnStartLookingAt(NE::Scripting::Entity entity) {
		if (debugRaycastInfo) {
			LOG_INFO("Started looking at entity " << entity);
		}

		// Store and apply highlight scale
		if (NE::ECS::Command::HasComponent<NE::ECS::Component::Transform>(entity)) {
			auto& transform = NE::ECS::Command::GetComponent<NE::ECS::Component::Transform>(entity);
			m_originalScale = transform.scale;
			transform.scale = m_originalScale * highlightScaleMultiplier;
			transform.isDirty = true;
		}
	}

	void OnStopLookingAt(NE::Scripting::Entity entity) {
		if (debugRaycastInfo) {
			LOG_INFO("Stopped looking at entity " << entity);
		}

		// Restore original scale
		if (NE::ECS::Command::HasComponent<NE::ECS::Component::Transform>(entity)) {
			auto& transform = NE::ECS::Command::GetComponent<NE::ECS::Component::Transform>(entity);
			transform.scale = m_originalScale;
			transform.isDirty = true;
		}
	}

	void OnInteractWithEntity(NE::Scripting::Entity entity) {
		LOG_INFO("========================================");
		LOG_INFO("    INTERACTING WITH ENTITY " << entity);
		LOG_INFO("========================================");
	}

	void HandleMovementAndGravity(NE::Scripting::Vec3& velocity, double deltaTime,
		bool attemptingJump, bool isGrounded) {
		
		//auto camTransform = NE::ECS::Command::GetComponent<NE::ECS::Component::Transform>(7);
		//NE::Scripting::Vec3 camForward = camTransform.GetForward();  // or Normalize manually
		//NE::Scripting::Vec3 camRight = camTransform.GetRight();
		//float yaw = camTransform.rotation.y * 0.017453292519943295f; // deg→rad

		/*NE::Scripting::Vec3 camForward{
			sinf(yaw),
			0.0f,
			-cosf(yaw)
		};
		camForward = camForward.Normalized();

		NE::Scripting::Vec3 camRight{
			camForward.z,
			0.0f,
			-camForward.x
		};*/

		// Get input for all 4 directions
		NE::Scripting::Vec3 inputDirection{ 0, 0, 0 };

		if (Input::IsKeyDown('W')) {
			inputDirection.z -= 1.0f;
		}
		if (Input::IsKeyDown('S')) {
			inputDirection.z += 1.0f;
		}
		if (Input::IsKeyDown('A')) {
			inputDirection.x -= 1.0f;
		}
		if (Input::IsKeyDown('D')) {
			inputDirection.x += 1.0f;
		}
		//if (Input::IsKeyDown('W')) inputDirection += camForward;
		//if (Input::IsKeyDown('S')) inputDirection -= camForward;
		//if (Input::IsKeyDown('A')) inputDirection += camRight;
		//if (Input::IsKeyDown('D')) inputDirection -= camRight;

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
		NE::Scripting::Vec3 newVelocity = GetVelocity();

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
				LOG_INFO("JUMP! velocity.y = " << newVelocity.y);
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
				LOG_INFO("Airborne: velocity.y = " << newVelocity.y << ", deltaTime = " << deltaTime);
			}
		}

		SetVelocity(newVelocity);
	}

	bool HandleJump(NE::Scripting::Vec3& velocity, bool isGrounded) {
		if (Input::WasKeyPressed(GLFW_KEY_SPACE)) {
			if (isGrounded && !m_hasJumpedThisFrame) {
				LOG_INFO("Jump input registered!");
				m_hasJumpedThisFrame = true;

				// CRITICAL: Clear ground contacts immediately when jumping
				// This ensures isGrounded becomes false right away
				m_groundContacts.clear();
				LOG_INFO("Ground contacts cleared for jump");

				return true;
			}
		}

		if (!Input::IsKeyDown(GLFW_KEY_SPACE)) {
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
	NE::Scripting::Entity m_lookingAtEntity = 0;
	NE::Scripting::Vec3 m_originalScale = { 1.0f, 1.0f, 1.0f };
	float m_colliderHalfHeight = 0.5f;

	// Collision-based ground detection
	std::unordered_set<NE::Scripting::Entity> m_groundContacts;

	// Pending collisions (queued during physics callbacks, processed in Update)
	std::vector<NE::Scripting::Entity> m_pendingCollisionEnters;
	std::vector<NE::Scripting::Entity> m_pendingCollisionExits;

	// Field registry for editor
	ExposedFieldRegistry m_fields;
};