#pragma once
#include "EngineAPI.hpp"

/**
 * Comprehensive example showcasing ALL GameObject and Script Reference functionality.
 *
 * This script demonstrates:
 * 1. GameObjectRef fields (drag-drop in editor)
 * 2. Accessing scripts on the SAME entity (multiple scripts per entity)
 * 3. Accessing scripts on DIFFERENT entities
 * 4. Finding GameObjects by name
 * 5. Finding all GameObjects with a specific script type
 * 6. Calling other script functions
 * 7. Reading/writing other script members
 * 8. Using GameObject to access entity properties
 *
 * SETUP INSTRUCTIONS:
 * 1. Create an entity named "Player" and add PlayerScript to it
 * 2. Create an entity named "Enemy" and add TestScript to it
 * 3. Create another entity and add BOTH PlayerScript AND this script to it
 * 4. Use the drag-drop in Inspector to set the targetRef to the "Enemy" entity
 * 5. Press keys to test different functionality (see comments below)
 */

class GameObjectExamples : public IScript {
public:
	//=========================================================================
	// FIELD REGISTRATION
	//=========================================================================
	GameObjectExamples() {
		SCRIPT_FIELD(debugEnabled, Bool);
		SCRIPT_FIELD(testValue, Float);
	}

	void Initialize(Entity entity) override {
		// Register fields for editor exposure
		SCRIPT_FIELD(debugEnabled, Bool);
		SCRIPT_FIELD(testValue, Float);

		// Register GameObjectRef field (drag-drop supported in editor)
		SCRIPT_GAMEOBJECT_REF(targetRef);
	}

	//=========================================================================
	// UPDATE - Contains all examples
	//=========================================================================
	void Update(double deltaTime) override {
		PrintWelcomeMessage();

		//=====================================================================
		// EXAMPLE 1: Access script on SAME entity (multiple scripts per entity)
		//=====================================================================
		Example1_SameEntityScript();

		//=====================================================================
		// EXAMPLE 2: Access script via GameObjectRef (drag-drop reference)
		//=====================================================================
		Example2_GameObjectRef();

		//=====================================================================
		// EXAMPLE 3: Find GameObject by name
		//=====================================================================
		Example3_FindByName();

		//=====================================================================
		// EXAMPLE 4: Find all GameObjects with specific script type
		//=====================================================================
		Example4_FindAllByType();

		//=====================================================================
		// EXAMPLE 5: Get script by string name (when type unknown)
		//=====================================================================
		Example5_GetScriptByName();

		//=====================================================================
		// EXAMPLE 6: Check if entity has specific script
		//=====================================================================
		Example6_HasComponent();

		//=====================================================================
		// EXAMPLE 7: Access GameObject properties (name, position, etc)
		//=====================================================================
		Example7_GameObjectProperties();

		//=====================================================================
		// EXAMPLE 8: Using gameObject() property directly
		//=====================================================================
		Example8_GameObjectProperty();
	}

	//=========================================================================
	// EXAMPLE 1: Access script on SAME entity
	//=========================================================================
	void Example1_SameEntityScript() const {
		if (Input::WasKeyPressed('1')) {
			LOG_DEBUG("=== EXAMPLE 1: Same Entity Script Access ===");

			// Method A: Use GetComponent<T>() directly (shortcut)
			auto playerScript = GetComponent<PlayerScript>();
			if (playerScript) {
				LOG_DEBUG("  Found PlayerScript on SAME entity!");

				// READ member variable
				// float speed = playerScript->speed;
				// LOG_DEBUG("  Player speed: {}", speed);

				// WRITE member variable
				// playerScript->lives = 5;
				// LOG_DEBUG("  Set player lives to 5");

				// CALL member function
				// playerScript->SomePublicFunction();
			} else {
				LOG_DEBUG("  No PlayerScript on this entity");
				LOG_DEBUG("  (Add PlayerScript to this entity to test)");
			}

			// Method B: Use gameObject().GetComponent<T>()
			auto playerScriptB = gameObject().GetComponent<PlayerScript>();
			if (playerScriptB) {
				LOG_DEBUG("  Also accessible via gameObject().GetComponent<T>()");
			}
		}
	}

	//=========================================================================
	// EXAMPLE 2: Access script via GameObjectRef (drag-drop)
	//=========================================================================
	void Example2_GameObjectRef() const {
		if (Input::WasKeyPressed('2')) {
			LOG_DEBUG("=== EXAMPLE 2: GameObjectRef (Drag-Drop) ===");

			// Check if the ref is valid
			if (!targetRef.IsValid()) {
				LOG_DEBUG("  targetRef is NOT valid!");
				LOG_DEBUG("  Drag an entity from Hierarchy to the 'targetRef' field in Inspector");
				return;
			}

			LOG_DEBUG("  targetRef is valid! Entity ID: " << targetRef.GetEntity());

			// Method A: Convert GameObjectRef to GameObject
			GameObject targetGO(targetRef);
			if (targetGO) {
				LOG_DEBUG("  GameObject created from GameObjectRef");

				// Get the TestScript on the target entity
				auto testScript = targetGO.GetComponent<TestScript>();
				if (testScript) {
					LOG_DEBUG("  Found TestScript on target entity!");

					// Access TestScript members
					/*float bounceHeight = testScript->bounceHeight;
					LOG_DEBUG("  Target bounceHeight: " << bounceHeight);*/
					testScript->TestPrint();

					// Modify TestScript members
					// testScript->bounceHeight = 2.0f;
				} else {
					LOG_DEBUG("  No TestScript on target entity");
				}
			}

			// Method B: One-liner - direct conversion + GetComponent
			auto testScript = GameObject(targetRef).GetComponent<TestScript>();
			if (testScript) {
				LOG_DEBUG("  Also works in one line!");
			}
		}
	}

	//=========================================================================
	// EXAMPLE 3: Find GameObject by name
	//=========================================================================
	void Example3_FindByName() const {
		if (Input::WasKeyPressed('3')) {
			LOG_DEBUG("=== EXAMPLE 3: Find GameObject by Name ===");

			// Find an entity named "Player"
			GameObject player = GameObject::Find("Player");

			if (player) {
				LOG_DEBUG("  Found 'Player' entity! ID: " << player.GetEntityId());
				LOG_DEBUG("  Entity name: '" << player.GetName() << "'");

				// Get the script on the found entity
				auto playerScript = player.GetComponent<PlayerScript>();
				if (playerScript) {
					LOG_DEBUG("  Has PlayerScript!");
					// Access members/functions
				}
			} else {
				LOG_DEBUG("  Could not find entity named 'Player'");
				LOG_DEBUG("  (Create an entity named 'Player' to test)");
			}

			// Try finding "Enemy"
			GameObject enemy = GameObject::Find("Enemy");
			if (enemy) {
				LOG_DEBUG("  Found 'Enemy' entity!");
				auto testScript = enemy.GetComponent<TestScript>();
				if (testScript) {
					LOG_DEBUG("  Enemy has TestScript!");
				}
			}
		}
	}

	//=========================================================================
	// EXAMPLE 4: Find all GameObjects with specific script type
	//=========================================================================
	void Example4_FindAllByType() const {
		if (Input::WasKeyPressed('4')) {
			LOG_DEBUG("=== EXAMPLE 4: Find All by Script Type ===");

			// Find ALL entities that have TestScript
			auto allTestScripts = GameObject::FindObjectsOfType<TestScript>();
			size_t testCount = allTestScripts.size();
			LOG_DEBUG("  Found " << testCount << " entities with TestScript");

			for (size_t i = 0; i < allTestScripts.size(); ++i) {
				const GameObject& go = allTestScripts[i];
				LOG_DEBUG("  [" << i << "] Entity " << go.GetEntityId() << ": '" << go.GetName() << "'");

				// Access each script
				auto testScript = go.GetComponent<TestScript>();
				if (testScript) {
					// Do something with each TestScript
					// testScript->bounceHeight = 1.0f;
				}
			}

			// Find ALL entities with PlayerScript
			auto allPlayers = GameObject::FindObjectsOfType<PlayerScript>();
			size_t playerCount = allPlayers.size();
			LOG_DEBUG("  Found " << playerCount << " entities with PlayerScript");
		}
	}

	//=========================================================================
	// EXAMPLE 5: Get script by string name
	//=========================================================================
	void Example5_GetScriptByName() const {
		if (Input::WasKeyPressed('5')) {
			LOG_DEBUG("=== EXAMPLE 5: Get Script by Name ===");

			// Get script by string name (useful when type is unknown)
			IScript* script = GetScript("PlayerScript");
			if (script) {
				LOG_DEBUG("  Found PlayerScript by name!");
				LOG_DEBUG("  Type name: '" << script->GetTypeName() << "'");
				// Note: To access specific members, you'd need to cast or use GetComponent<T>
			}

			// Also works with GameObject
			GameObject player = GameObject::Find("Player");
			if (player) {
				IScript* playerScript = player.GetScript("PlayerScript");
				if (playerScript) {
					LOG_DEBUG("  Got PlayerScript from player entity by name!");
				}
			}
		}
	}

	//=========================================================================
	// EXAMPLE 6: Check if entity has specific script
	//=========================================================================
	void Example6_HasComponent() const {
		if (Input::WasKeyPressed('6')) {
			LOG_DEBUG("=== EXAMPLE 6: Has Component Check ===");

			// Check if THIS entity has PlayerScript
			if (HasScript<PlayerScript>()) {
				LOG_DEBUG("  This entity HAS PlayerScript!");
			} else {
				LOG_DEBUG("  This entity does NOT have PlayerScript");
			}

			// Check if target entity has TestScript
			if (targetRef.IsValid()) {
				GameObject target(targetRef);
				if (target.HasScript<TestScript>()) {
					LOG_DEBUG("  Target entity HAS TestScript!");
				} else {
					LOG_DEBUG("  Target entity does NOT have TestScript");
				}
			}

			// Check using GameObject directly
			GameObject player = GameObject::Find("Player");
			if (player && player.HasScript<PlayerScript>()) {
				LOG_DEBUG("  Player entity HAS PlayerScript!");
			}
		}
	}

	//=========================================================================
	// EXAMPLE 7: GameObject properties
	//=========================================================================
	void Example7_GameObjectProperties() const {
		if (Input::WasKeyPressed('7')) {
			LOG_DEBUG("=== EXAMPLE 7: GameObject Properties ===");

			// Get this entity's GameObject
			GameObject myGO = gameObject();

			// Get entity ID
			Entity id = myGO.GetEntityId();
			LOG_DEBUG("  Entity ID: " << id);

			// Get entity name
			std::string name = myGO.GetName();
			LOG_DEBUG("  Entity Name: '" << name << "'");

			// Check if valid
			if (myGO.IsValid()) {
				LOG_DEBUG("  GameObject is valid!");
			}

			// Set entity name
			myGO.SetName("RenamedEntity");
			LOG_DEBUG("  Set name to 'RenamedEntity'");
			myGO.SetName(name);  // Restore original name

			// Get position (using IScript method, not GameObject)
			Vec3 pos = GetPosition();
			LOG_DEBUG("  Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")");
		}
	}

	//=========================================================================
	// EXAMPLE 8: Using gameObject() property
	//=========================================================================
	void Example8_GameObjectProperty() const {
		if (Input::WasKeyPressed('8')) {
			LOG_DEBUG("=== EXAMPLE 8: gameObject() Property ===");

			// gameObject() returns a GameObject for this entity
			// This is the same as "this" in Unity

			// Use it to get other scripts on same entity
			auto player = gameObject().GetComponent<PlayerScript>();
			if (player) {
				LOG_DEBUG("  Got PlayerScript via gameObject()");
			}

			// Chain calls
			if (gameObject().HasScript<PlayerScript>()) {
				LOG_DEBUG("  HasScript via gameObject() works!");
			}

			// Pass to other functions
			ProcessGameObject(gameObject());
		}
	}

	//=========================================================================
	// HELPER: Example of passing GameObject to functions
	//=========================================================================
	void ProcessGameObject(const GameObject& go) const {
		if (Input::WasKeyPressed('9')) {
			LOG_DEBUG("=== EXAMPLE 9: Passing GameObject to Function ===");

			if (go.IsValid()) {
				LOG_DEBUG("  Received valid GameObject: '" << go.GetName() << "'");

				// Get any script from it
				auto testScript = go.GetComponent<TestScript>();
				if (testScript) {
					LOG_DEBUG("  It has TestScript!");
				}
				auto playerScript = go.GetComponent<PlayerScript>();
				if (playerScript) {
					LOG_DEBUG("  It has PlayerScript!");
				}
			}
		}
	}

	//=========================================================================
	// REQUIRED SCRIPT OVERRIDES
	//=========================================================================
	const char* GetTypeName() const override {
		return "GameObjectExamples";
	}

	void OnCollisionEnter(Entity other) override {
		if (debugEnabled) {
			LOG_DEBUG("[GameObjectExamples] Collision with entity " << other);
		}
	}

	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

private:
	//=========================================================================
	// HELPER: Print welcome message with controls
	//=========================================================================
	void PrintWelcomeMessage() const {
		static bool printed = false;
		if (!printed) {
			LOG_DEBUG("========================================");
			LOG_DEBUG("GameObject Examples - PRESS KEYS TO TEST:");
			LOG_DEBUG("========================================");
			LOG_DEBUG("  [1] Same Entity Script Access");
			LOG_DEBUG("  [2] GameObjectRef (Drag-Drop)");
			LOG_DEBUG("  [3] Find GameObject by Name");
			LOG_DEBUG("  [4] Find All by Script Type");
			LOG_DEBUG("  [5] Get Script by Name");
			LOG_DEBUG("  [6] Has Component Check");
			LOG_DEBUG("  [7] GameObject Properties");
			LOG_DEBUG("  [8] gameObject() Property");
			LOG_DEBUG("  [9] Pass GameObject to Function");
			LOG_DEBUG("========================================");
			LOG_DEBUG("SETUP: Create entities named 'Player' and 'Enemy'");
			LOG_DEBUG("Add PlayerScript to 'Player', TestScript to 'Enemy'");
			LOG_DEBUG("Drag 'Enemy' to targetRef field for Example 2");
			LOG_DEBUG("========================================");
			printed = true;
		}
	}

	//=========================================================================
	// EXPOSED FIELDS
	//=========================================================================
	bool debugEnabled = true;
	float testValue = 42.0f;

	// GameObjectRef - can be set via drag-drop in editor
	GameObjectRef targetRef;
};
