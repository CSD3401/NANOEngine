#pragma once
#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"

namespace NE::ECS::Component {
	struct ParticleEmitter;
}

namespace NE::Math {
	struct Vec3;
	struct Vec4;
}

namespace NE::ECS::Systems {
	using NE::ECS::Component::ParticleEmitter;
	using NE::Math::Vec3;
	using NE::Math::Vec4;

	class ParticleSystem final : public System {
	public:
		explicit ParticleSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void OnEntityActive(Entity entity) override;
		void OnEntityInactive(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void Exit() override;

	private:
		// Runtime init for new/loaded emitters
		static void InitEmitterRuntime(NE::ECS::Component::ParticleEmitter& e, Entity entity);

		// RNG helpers
		static uint32_t XorShift32(uint32_t& state);
		static float Rand01(uint32_t& state);
		static float RandRange(uint32_t& state, float a, float b);

		// Helpers
		static float Clamp01(float v);
		static Vec4 Lerp(const Vec4& a, const Vec4& b, float t);

		// SoA operations
		static void KillSwapRemove(NE::ECS::Component::ParticleEmitter& e, uint32_t i);

		// Spawn sampling
		static Vec3 NormalizeSafe(const Vec3& v);
		static Vec3 SampleSpawnOffset(NE::ECS::Component::ParticleEmitter& e);
		static Vec3 SampleInitialVelocity(NE::ECS::Component::ParticleEmitter& e);

		// Collision hook (replace with physics query)
		static bool ResolveCollision(
			NE::ECS::Component::ParticleEmitter& e,
			NE::Math::Vec3& nextPos,
			NE::Math::Vec3& vel,
			float dt
		);

	private:
		ComponentManager* m_componentManager;

	};
}