#pragma once

#include <vector>
#include "Math/Vec3.hpp"
#include "Math/Vec4.hpp"
#include "Graphics/Core/DrawCommand.hpp"
#include "Graphics/Core/InstanceData.hpp"
#include "Graphics/Core/Material.hpp"
#include "Graphics/Core/Model.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

	struct ParticleEmitter
	{
		// =====================================================
		// Basic Emitter Settings
		// =====================================================
		bool enabled = true;
		bool looping = true;

		int maxParticles = 1000;
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
		//bool billboard = true;
		//bool stretchWithVelocity = false;
		//float stretchFactor = 0.0f;
		std::string materialUUID;      // particle material (uses texture(s) inside)
		std::string modelUUID;         // OPTIONAL: if you want a custom quad model; can be empty and use engine quad

		// =====================================================
		// Runtime State (not serialized)
		// =====================================================

		std::shared_ptr<Graphics::Material> material;
		std::shared_ptr<Graphics::Model> model; // if null, use engine’s built-in quad

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

		bool isDirty = true;

		NE_REFLECT_BEGIN(ParticleEmitter)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(looping),
			NE_REFLECT_FIELD(maxParticles),
			NE_REFLECT_FIELD(spawnRate),
			NE_REFLECT_FIELD(duration),
			NE_REFLECT_FIELD(playOnStart),
			NE_REFLECT_FIELD(localSpace),

			NE_REFLECT_FIELD(lifetimeMin),
			NE_REFLECT_FIELD(lifetimeMax),

			NE_REFLECT_FIELD(speedMin),
			NE_REFLECT_FIELD(speedMax),

			NE_REFLECT_FIELD(sizeMin),
			NE_REFLECT_FIELD(sizeMax),

			NE_REFLECT_FIELD(startColor),
			NE_REFLECT_FIELD(endColor),

			NE_REFLECT_FIELD(enableGravity),
			NE_REFLECT_FIELD(gravity),

			NE_REFLECT_FIELD(enableDrag),
			NE_REFLECT_FIELD(drag),

			NE_REFLECT_FIELD(enableCollision),
			NE_REFLECT_FIELD(collisionRadius),
			NE_REFLECT_FIELD(bounceFactor),
			NE_REFLECT_FIELD(killOnCollision),

			NE_REFLECT_FIELD(shape),
			NE_REFLECT_FIELD(sphereRadius),
			NE_REFLECT_FIELD(coneAngle),
			NE_REFLECT_FIELD(boxExtents),

			NE_REFLECT_FIELD(materialUUID),
			NE_REFLECT_FIELD(modelUUID),
			//NE_REFLECT_FIELD(billboard),
			//NE_REFLECT_FIELD(stretchWithVelocity),
			//NE_REFLECT_FIELD(stretchFactor),


			NE_REFLECT_FIELD_HIDDEN(isDirty)
		NE_REFLECT_END()
	};

}