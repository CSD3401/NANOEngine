#include "pch.h"
#include "ParticleSystem.hpp"
#include "../Components/ParticleEmitter.hpp"
#include "../Components/Transform.hpp"
#include "../Components/PERenderer.h"
#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/Core/InstanceData.hpp"

namespace NE::ECS::Systems {
	ParticleSystem::ParticleSystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void ParticleSystem::OnEntityAdded(Entity entity)
	{
		auto& e = m_componentManager->GetComponent<ParticleEmitter>(entity);
		InitEmitterRuntime(e, entity);
	}

	void ParticleSystem::OnEntityRemoved(Entity)
	{
	}

	void ParticleSystem::OnEntityActive(Entity)
	{
	}

	void ParticleSystem::OnEntityInactive(Entity)
	{
	}

	void ParticleSystem::Init()
	{
		const auto& entities = GetEntities();
		for (Entity entity : entities)
		{
			auto& e = m_componentManager->GetComponent<ParticleEmitter>(entity);
			InitEmitterRuntime(e, entity);
		}
	}

	void ParticleSystem::Update(double deltaTime)
	{
		if (!m_componentManager) return;

		const float dt = static_cast<float>(deltaTime);
		if (dt <= 0.0f) return;

		const auto& entities = GetEntities();
		for (Entity entity : entities)
		{
			auto& e = m_componentManager->GetComponent<NE::ECS::Component::ParticleEmitter>(entity);
			auto& tr = m_componentManager->GetComponent<NE::ECS::Component::Transform>(entity);

			if (!e.enabled) continue;

			if (e.positions.size() != e.maxParticles)
				e.EnsureCapacity();

			const NE::Math::Mat4& W = tr.worldMatrix;
			const NE::Math::Vec3 emitterWorldPos = W.GetTranslation();

			// Normalize basis to get rotation-only directions
			NE::Math::Vec3 rightWS = W.Right();   rightWS.Normalize();
			NE::Math::Vec3 upWS = W.Up();      upWS.Normalize();
			NE::Math::Vec3 forwardWS = W.Forward(); forwardWS.Normalize();

			// local -> world direction (rotation only, no translation, no scale)
			auto LocalDirToWorld = [&](const NE::Math::Vec3& vLS) -> NE::Math::Vec3
				{
					return {
						rightWS.x * vLS.x + upWS.x * vLS.y + forwardWS.x * vLS.z,
						rightWS.y * vLS.x + upWS.y * vLS.y + forwardWS.y * vLS.z,
						rightWS.z * vLS.x + upWS.z * vLS.y + forwardWS.z * vLS.z
					};
				};

			// world -> local direction (inverse rotation = transpose, using dot products)
			auto WorldDirToLocal = [&](const NE::Math::Vec3& vWS) -> NE::Math::Vec3
				{
					return {
						vWS.Dot(rightWS),
						vWS.Dot(upWS),
						vWS.Dot(forwardWS)
					};
				};

			// -----------------------
			// Duration controls spawning only
			// -----------------------
			bool canSpawn = e.playing;

			if (canSpawn && e.duration > 0.0f)
			{
				e.emitterTime += dt;
				if (e.emitterTime >= e.duration)
				{
					if (e.looping) e.emitterTime = 0.0f;
					else { e.playing = false; canSpawn = false; }
				}
			}

			// -----------------------
			// Spawn
			// -----------------------
			if (canSpawn)
			{
				e.spawnAccumulator += e.spawnRate * dt;

				const uint32_t spawnCount = static_cast<uint32_t>(std::floor(e.spawnAccumulator));
				if (spawnCount > 0)
					e.spawnAccumulator -= static_cast<float>(spawnCount);

				for (uint32_t s = 0; s < spawnCount && e.aliveCount < e.maxParticles; s++)
				{
					const uint32_t i = e.aliveCount++;

					const NE::Math::Vec3 offsetLS = SampleSpawnOffset(e);
					const NE::Math::Vec3 velLS = SampleInitialVelocity(e);

					if (e.localSpace)
					{
						e.positions[i] = offsetLS;
						e.velocities[i] = velLS;
					}
					else
					{
						const NE::Math::Vec3 offsetWS = LocalDirToWorld(offsetLS);
						const NE::Math::Vec3 velWS = LocalDirToWorld(velLS);

						e.positions[i] = emitterWorldPos + offsetWS;
						e.velocities[i] = velWS;
					}

					e.ages[i] = 0.0f;
					e.lifetimes[i] = std::max(0.01f, RandRange(e.rngState, e.lifetimeMin, e.lifetimeMax));
					e.sizes[i] = std::max(0.0001f, RandRange(e.rngState, e.sizeMin, e.sizeMax));
					e.colors[i] = e.startColor;
				}
			}

			// -----------------------
			// Simulate
			// -----------------------
			// Gravity is defined in WORLD space.
			// If simulating in local space, convert gravity to local via inverse rotation.
			NE::Math::Vec3 gravityToApply = e.gravity;
			if (e.enableGravity && e.localSpace)
				gravityToApply = WorldDirToLocal(e.gravity);

			uint32_t i = 0;
			while (i < e.aliveCount)
			{
				e.ages[i] += dt;

				const float life = e.lifetimes[i];
				if (e.ages[i] >= life)
				{
					KillSwapRemove(e, i);
					continue;
				}

				NE::Math::Vec3 vel = e.velocities[i];
				NE::Math::Vec3 pos = e.positions[i];

				if (e.enableGravity)
					vel += gravityToApply * dt;

				if (e.enableDrag && e.drag > 0.0f)
				{
					const float k = std::max(0.0f, 1.0f - e.drag * dt);
					vel *= k;
				}

				NE::Math::Vec3 nextPos = pos + vel * dt;

				// Collision: keep it world-space only for the simple version
				if (e.enableCollision && !e.localSpace)
				{
					const bool hit = ResolveCollision(e, nextPos, vel, dt);
					if (hit && e.killOnCollision)
					{
						KillSwapRemove(e, i);
						continue;
					}
				}

				e.positions[i] = nextPos;
				e.velocities[i] = vel;

				const float t = Clamp01(e.ages[i] / life);
				e.colors[i] = Lerp(e.startColor, e.endColor, t);

				i++;
			}

			// Renderer module
			if (m_componentManager->HasComponent<NE::ECS::Component::PERenderer>(entity))
			{
				auto& per = m_componentManager->GetComponent<NE::ECS::Component::PERenderer>(entity);

				if (!per.material)
					continue;

				// Choice B requires localSpace
				if (!e.localSpace)
					continue;

				// Build instances (stored in emitter so memory stays valid)
				e.renderInstances.clear();
				e.renderInstances.reserve(e.aliveCount);

				for (uint32_t i = 0; i < e.aliveCount; ++i)
				{
					NE::Graphics::ParticleInstanceData inst{};
					inst.posLS = e.positions[i];
					inst.size = e.sizes[i];
					inst.color = e.colors[i];
					e.renderInstances.push_back(inst);
				}

				NE::Graphics::ParticleDrawCommand cmd{};
				cmd.emitterModel = tr.worldMatrix;
				cmd.mesh = NE::Graphics::GraphicsManager::GetGlobalParticleQuadMesh();
				cmd.material = per.material;
				cmd.instances = e.renderInstances.data();
				cmd.instanceCount = (uint32_t)e.renderInstances.size();

				cmd.boundsCenterWS = tr.worldMatrix.GetTranslation();
				cmd.boundsRadiusWS = e.sphereRadius + e.sizeMax; // conservative

				NE::Graphics::GraphicsManager::Submit(cmd);
			}
		}
	}

	void ParticleSystem::Exit()
	{
	}

	void ParticleSystem::InitEmitterRuntime(ParticleEmitter& e, Entity entity)
	{
		e.EnsureCapacity();

		// Seed RNG only once (0 means "uninitialized" in this design)
		if (e.rngState == 0u)
		{
			// Simple deterministic seed based on entity id.
			uint32_t seed = static_cast<uint32_t>(entity) * 747796405u + 2891336453u;
			if (seed == 0u) seed = 0x6C8E9CF5u;
			e.rngState = seed;
		}

		// If this is first-time init, respect playOnStart
		// (Don't override if already explicitly set by gameplay code.)
		// Heuristic: if emitterTime==0 and spawnAccumulator==0 and aliveCount==0 -> likely fresh.
		const bool looksFresh =
			(e.emitterTime == 0.0f) &&
			(e.spawnAccumulator == 0.0f) &&
			(e.aliveCount == 0);

		if (looksFresh)
			e.playing = e.playOnStart;
	}

	// =========================================================
	// RNG helpers
	// =========================================================
	uint32_t ParticleSystem::XorShift32(uint32_t& state)
	{
		if (state == 0u) state = 0x6C8E9CF5u;
		uint32_t x = state;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		state = x;
		return x;
	}

	float ParticleSystem::Rand01(uint32_t& state)
	{
		const uint32_t r = XorShift32(state);
		return (r & 0x00FFFFFFu) / 16777216.0f; // 2^24 in [0,1)
	}

	float ParticleSystem::RandRange(uint32_t& state, float a, float b)
	{
		return a + (b - a) * Rand01(state);
	}

	// =========================================================
	// Helpers
	// =========================================================
	float ParticleSystem::Clamp01(float v)
	{
		return std::max(0.0f, std::min(1.0f, v));
	}

	NE::Math::Vec4 ParticleSystem::Lerp(const NE::Math::Vec4& a, const NE::Math::Vec4& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t
		};
	}

	// =========================================================
	// SoA swap-remove kill
	// =========================================================
	void ParticleSystem::KillSwapRemove(ParticleEmitter& e, uint32_t i)
	{
		const uint32_t last = e.aliveCount - 1;
		if (i != last)
		{
			e.positions[i] = e.positions[last];
			e.velocities[i] = e.velocities[last];
			e.ages[i] = e.ages[last];
			e.lifetimes[i] = e.lifetimes[last];
			e.sizes[i] = e.sizes[last];
			e.colors[i] = e.colors[last];
		}
		e.aliveCount--;
	}

	// =========================================================
	// Spawn sampling
	// =========================================================
	NE::Math::Vec3 ParticleSystem::NormalizeSafe(const NE::Math::Vec3& v)
	{
		const float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
		if (len2 <= 1e-8f) return { 0.f, 1.f, 0.f };
		const float invLen = 1.0f / std::sqrt(len2);
		return { v.x * invLen, v.y * invLen, v.z * invLen };
	}

	NE::Math::Vec3 ParticleSystem::SampleSpawnOffset(ParticleEmitter& e)
	{
		switch (e.shape)
		{
		case ParticleEmitter::ShapeType::Point:
			return { 0.f, 0.f, 0.f };

		case ParticleEmitter::ShapeType::Sphere:
		{
			NE::Math::Vec3 dir = {
				RandRange(e.rngState, -1.f, 1.f),
				RandRange(e.rngState, -1.f, 1.f),
				RandRange(e.rngState, -1.f, 1.f)
			};
			dir = NormalizeSafe(dir);

			const float u = Rand01(e.rngState);
			const float r = e.sphereRadius * std::cbrt(u); // uniform volume
			return { dir.x * r, dir.y * r, dir.z * r };
		}

		case ParticleEmitter::ShapeType::Box:
			return {
				RandRange(e.rngState, -e.boxExtents.x, e.boxExtents.x),
				RandRange(e.rngState, -e.boxExtents.y, e.boxExtents.y),
				RandRange(e.rngState, -e.boxExtents.z, e.boxExtents.z)
			};

		case ParticleEmitter::ShapeType::Cone:
		{
			// Simple: spawn at origin (or slight disk if you extend later)
			return { 0.f, 0.f, 0.f };
		}

		default:
			return { 0.f, 0.f, 0.f };
		}
	}

	NE::Math::Vec3 ParticleSystem::SampleInitialVelocity(ParticleEmitter& e)
	{
		NE::Math::Vec3 dir = {
			RandRange(e.rngState, -1.f, 1.f),
			RandRange(e.rngState, -1.f, 1.f),
			RandRange(e.rngState, -1.f, 1.f)
		};
		dir = NormalizeSafe(dir);

		// Cone bias: bias direction towards +Y based on coneAngle
		if (e.shape == ParticleEmitter::ShapeType::Cone)
		{
			const float bias = Clamp01(e.coneAngle / 90.0f);
			const NE::Math::Vec3 up{ 0.f, 1.f, 0.f };
			dir = NormalizeSafe({
				up.x * (1.0f - bias) + dir.x * bias,
				up.y * (1.0f - bias) + dir.y * bias,
				up.z * (1.0f - bias) + dir.z * bias
				});
		}

		const float speed = RandRange(e.rngState, e.speedMin, e.speedMax);
		return { dir.x * speed, dir.y * speed, dir.z * speed };
	}

	// =========================================================
	// Collision hook 
	// =========================================================
	bool ParticleSystem::ResolveCollision(ParticleEmitter& e, NE::Math::Vec3& nextPos, NE::Math::Vec3& vel, float dt)
	{
		// Note: This function is currently a placeholder to show where and how collision resolution would be integrated.
		
		// Replace with: swept sphere query from current pos -> nextPos with radius e.collisionRadius
		// On hit:
		//   nextPos = hitPoint + hitNormal * e.collisionRadius
		//   vel = reflect(vel, hitNormal) * e.bounceFactor
		(void)e; (void)nextPos; (void)vel; (void)dt;
		return false;
	}

} // namespace NE::ECS::Systems