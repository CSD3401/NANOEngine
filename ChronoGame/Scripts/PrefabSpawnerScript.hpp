#pragma once

#include "EngineAPI.hpp"

/**
 * @file PrefabSpawnerScript.hpp
 * @brief Example script demonstrating Unity-style prefab spawning
 *
 * This script shows how to:
 * - Hold references to prefabs in script fields
 * - Spawn prefabs at runtime at specific positions
 * - Use prefabs for enemy spawners, bullet systems, etc.
 *
 * Usage:
 * 1. Attach this script to an entity
 * 2. In the editor, drag a prefab into the "enemyPrefab" field
 *    - The editor should assign the prefab's UUID or path to this field
 *    - For now, you can also manually set the prefab path as a string
 * 3. Run the game and press 'P' to manually spawn, or enable autoSpawn
 *
 * Note: Currently, the system accepts prefab file paths directly.
 * Full UUID-to-path resolution via AssetManager will be added in a future update.
 */


    class PrefabSpawnerScript : public IScript {
    public:
        PrefabSpawnerScript() {
            // Register the prefab reference field for editor exposure
            // This allows the editor to show a prefab picker
            SCRIPT_PREFAB_REF(enemyPrefab);

            // Register other fields
            SCRIPT_FIELD(spawnInterval, Float);
            SCRIPT_FIELD(maxSpawns, Int);
            SCRIPT_FIELD(spawnRadius, Float);
        }

        void Initialize(Entity entity) override {
            m_spawnTimer = 0.0f;
            m_spawnCount = 0;

            LOG_INFO("PrefabSpawnerScript initialized");
        }

        void Update(double deltaTime) override {
            // Manual spawning with keyboard input
            if (Input::WasKeyPressed('P')) {
                SpawnPrefabAtPosition(GetPosition());
                LOG_INFO("Manually spawned prefab at spawner position");
            }

            // Automatic spawning at intervals
            //if (autoSpawn && enemyPrefab.IsValid()) {
            //    m_spawnTimer += static_cast<float>(deltaTime);

            //    if (m_spawnTimer >= spawnInterval && m_spawnCount < maxSpawns) {
            //        // Calculate random spawn position around this entity
            //        Vec3 spawnPos = GetRandomSpawnPosition();
            //        SpawnPrefabAtPosition(spawnPos);

            //        m_spawnTimer = 0.0f;
            //        m_spawnCount++;
            //    }
            //}

            // Reset spawn count with 'R' key
            if (Input::WasKeyPressed('R')) {
                m_spawnCount = 0;
                LOG_INFO("Spawn count reset");
            }
        }

        void OnValidate() override {
            // Called when values change in the editor
            if (spawnInterval < 0.1f) {
                spawnInterval = 0.1f;
                LOG_WARNING("Spawn interval clamped to minimum 0.1 seconds");
            }
            if (maxSpawns < 1) {
                maxSpawns = 1;
            }
        }

        const char* GetTypeName() const override {
            return "PrefabSpawnerScript";
        }

        // Collision events (required overrides)
        void OnCollisionEnter(Entity other) override {}
        void OnCollisionExit(Entity other) override {}
        void OnTriggerEnter(Entity other) override {}
        void OnTriggerExit(Entity other) override {}

    private:
        void SpawnPrefabAtPosition(const Vec3& position) {
            if (!enemyPrefab.IsValid()) {
                LOG_ERROR("Cannot spawn: No prefab assigned to enemyPrefab field!");
                return;
            }

            // Instantiate the prefab at the specified position with default rotation
            Entity spawnedEntity = InstantiatePrefab(enemyPrefab, position, Vec3::Zero());

            if (spawnedEntity != NE::Scripting::INVALID_ENTITY) {
                LOG_INFO("Successfully spawned prefab, entity ID: {}", spawnedEntity);

                // Optionally, you can modify the spawned entity here
                // For example, add velocity, change scale, etc.
                if (HasRigidbody(spawnedEntity)) {
                    SetVelocity(0.0f, 5.0f, 0.0f, spawnedEntity);
                }
            } else {
                LOG_ERROR("Failed to spawn prefab!");
            }
        }

        Vec3 GetRandomSpawnPosition() {
            // Generate random position within spawn radius
            float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f; // 0 to 2*PI
            float distance = static_cast<float>(rand()) / RAND_MAX * spawnRadius;

            Vec3 offset(
                cos(angle) * distance,
                0.0f,
                sin(angle) * distance
            );

            return GetPosition() + offset;
        }

    private:
        // Prefab reference - this will be exposed in the editor
        // The editor will show a prefab picker for this field
        PrefabRef enemyPrefab;

        // Spawn settings
        float spawnInterval = 2.0f;     // Time between spawns (seconds)
        int maxSpawns = 10;             // Maximum number of prefabs to spawn
        float spawnRadius = 5.0f;       // Radius around spawner to spawn within
        bool autoSpawn = true;          // Enable/disable automatic spawning

        // Internal state
        float m_spawnTimer = 0.0f;
        int m_spawnCount = 0;
    };


