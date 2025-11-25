#pragma once

#include "EngineAPI.hpp"

/**
 * @file BulletShooterScript.hpp
 * @brief Example script demonstrating Unity-style bullet spawning with prefabs
 *
 * This script shows a common game scenario:
 * - Player/Enemy shoots bullets using a prefab
 * - Bullets spawn at the entity's position
 * - Bullets are given forward velocity
 * - Fire rate control
 *
 * Usage:
 * 1. Create a bullet prefab with Renderer, Rigidbody, and Collider
 * 2. Attach this script to a player/turret entity
 * 3. Drag the bullet prefab into the "bulletPrefab" field in the editor
 * 4. Press SPACE to shoot
 */


    class BulletShooterScript : public IScript {
    public:
        BulletShooterScript() {
            // Register the bullet prefab reference
            SCRIPT_PREFAB_REF(bulletPrefab);

            // Register shooter settings
            SCRIPT_FIELD(fireRate, Float);
            SCRIPT_FIELD(bulletSpeed, Float);
            SCRIPT_FIELD(spawnOffset, Vec3);
        }

        void Initialize(Entity entity) override {
            m_fireCooldown = 0.0f;

            LOG_INFO("BulletShooterScript initialized on entity " << entity);
        }

        void Update(double deltaTime) override {
            // Update fire cooldown
            if (m_fireCooldown > 0.0f) {
                m_fireCooldown -= static_cast<float>(deltaTime);
            }

            // Shoot when spacebar is pressed and not on cooldown
            if (Input::WasKeyPressed(' ') && CanFire()) {
                Fire();
            }
        }

        const char* GetTypeName() const override {
            return "BulletShooterScript";
        }

        // Collision events (required overrides)
        void OnCollisionEnter(Entity other) override {}
        void OnCollisionExit(Entity other) override {}
        void OnTriggerEnter(Entity other) override {}
        void OnTriggerExit(Entity other) override {}

    private:
        bool CanFire() const {
            return bulletPrefab.IsValid() && m_fireCooldown <= 0.0f;
        }

        void Fire() {
            // Calculate spawn position (forward of the shooter)
            Vec3 shooterPos = GetPosition();
            Vec3 shooterForward = GetForward();
            Vec3 spawnPosition = shooterPos + shooterForward * spawnOffset.z + Vec3(0, spawnOffset.y, 0);

            // Get shooter rotation for bullet
            Vec3 shooterRotation = GetRotation();

            // Spawn the bullet prefab
            Entity bullet = InstantiatePrefab(bulletPrefab, spawnPosition, shooterRotation);

            if (bullet != NE::Scripting::INVALID_ENTITY) {
                // Apply velocity to the bullet in the forward direction
                Vec3 bulletVelocity = shooterForward * bulletSpeed;

                if (HasRigidbody(bullet)) {
                    SetVelocity(bulletVelocity, bullet);
                }

                // Set fire cooldown based on fire rate
                m_fireCooldown = 1.0f / fireRate;

                // Play sound effect (if audio source is attached)
                if (HasAudioSource()) {
                    PlayAudio();
                }
            } else {
                LOG_ERROR("Failed to spawn bullet prefab!");
            }
        }

    private:
        // Unity-style prefab reference
        PrefabRef bulletPrefab;

        // Shooter settings
        float fireRate = 5.0f;          // Shots per second
        float bulletSpeed = 20.0f;      // Bullet speed in units/second
        Vec3 spawnOffset = Vec3(0, 0, 1); // Offset from shooter position
        bool autoFire = false;          // Enable auto-fire when mouse held

        // Internal state
        float m_fireCooldown = 0.0f;
    };


