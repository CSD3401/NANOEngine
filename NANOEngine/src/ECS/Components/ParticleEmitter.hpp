#pragma once

#include <vector>
#include "Math/Vec3.hpp"
#include "Math/Vec4.hpp"
#include "Graphics/Core/DrawCommand.hpp"
#include "Graphics/Core/InstanceData.hpp"

namespace NE::ECS::Component {

	struct ParticleEmitter
	{
		// =====================================================
		// Basic Emitter Settings
		// =====================================================
		bool enabled = true;
		bool looping = true;

		uint32_t maxParticles = 1000;
		float spawnRate = 50.0f;       // particles per second
		float duration = 0.0f;         // 0 = infinite

		bool playOnStart = true;
		bool localSpace = true;        // particles move relative to emitter

		// =====================================================
		// Lifetime
		// =====================================================
		float lifetimeMin = 1.0f;
		float lifetimeMax = 2.0f;

		// =====================================================
		// Initial Velocity
		// =====================================================
		float speedMin = 1.0f;
		float speedMax = 3.0f;

		// =====================================================
		// Initial Size
		// =====================================================
		float sizeMin = 0.1f;
		float sizeMax = 0.5f;

		// =====================================================
		// Color
		// =====================================================
		NE::Math::Vec4 startColor = { 1.f, 1.f, 1.f, 1.f };
		NE::Math::Vec4 endColor = { 1.f, 1.f, 1.f, 0.f };

		// =====================================================
		// Gravity
		// =====================================================
		bool enableGravity = false;
		NE::Math::Vec3 gravity = { 0.0f, -9.81f, 0.0f };

		// =====================================================
		// Drag
		// =====================================================
		bool enableDrag = false;
		float drag = 0.0f;

		// =====================================================
		// Collision (simple world collision)
		// =====================================================
		bool enableCollision = false;
		float collisionRadius = 0.02f;
		float bounceFactor = 0.5f;
		bool killOnCollision = false;

		// =====================================================
		// Shape (spawn volume)
		// =====================================================
		enum class ShapeType
		{
			Point,
			Sphere,
			Cone,
			Box
		};

		ShapeType shape = ShapeType::Point;

		float sphereRadius = 1.0f;
		float coneAngle = 30.0f;
		NE::Math::Vec3 boxExtents = { 1.f,1.f,1.f };

		// =====================================================
		// Rendering
		// =====================================================
		bool billboard = true;
		bool stretchWithVelocity = false;
		float stretchFactor = 0.0f;

		// =====================================================
		// Runtime State (not serialized)
		// =====================================================
		
		bool playing = false;
		uint32_t rngState = 0;

		// Alive particles are always in [0 .. aliveCount)
		uint32_t aliveCount = 0;

		// Particle attributes
		std::vector<NE::Math::Vec3> positions;
		std::vector<NE::Math::Vec3> velocities;

		std::vector<float> ages;
		std::vector<float> lifetimes;

		std::vector<float> sizes;
		std::vector<NE::Math::Vec4> colors;

		float spawnAccumulator = 0.0f;
		float emitterTime = 0.0f;

		std::vector<NE::Graphics::ParticleInstanceData> renderInstances;

		void EnsureCapacity()
		{
			positions.resize(maxParticles);
			velocities.resize(maxParticles);
			ages.resize(maxParticles);
			lifetimes.resize(maxParticles);
			sizes.resize(maxParticles);
			colors.resize(maxParticles);

			if (aliveCount > maxParticles) aliveCount = maxParticles;
		}
		void Play()
		{
			if (!enabled) return;
			playing = true;
		}
		void Stop()
		{
			playing = false;
		}
		void Reset()
		{
			aliveCount = 0;
			spawnAccumulator = 0.0f;
			emitterTime = 0.0f;
		}
	};

}