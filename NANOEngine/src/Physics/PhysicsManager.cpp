#include "PhysicsManager.hpp"

#include <Jolt/RegisterTypes.h>
#include "JoltDebugRenderer.hpp"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BackFaceMode.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterMask.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterMask.h>
#include <Jolt/Math/Math.h>

// Raycasting includes
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Geometry/IndexedTriangle.h>

// debug
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include "Core/SpdLogger.hpp"
#include "Core/LUIDRegistry.hpp"
#include "Scripting/ScriptingEngine.hpp"

#include "Ray.hpp"
#include "RaycastHit.hpp"
#include "ECS/Components/Collider.hpp"
#include "ECS/Components/Rigidbody.hpp"
#include "ECS/Components/CharacterController.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/Renderer.hpp"
#include "ECS/Core/ComponentManager.hpp"
#include "ObjectLayerPairFilterImpl.hpp"
#include "BroadPhaseLayerInterfaceImpl.hpp"
#include "ObjectVsBroadPhaseLayerFilterImpl.hpp"
#include "ObjectLayerFilterImpl.hpp"
#include "StreamInImpl.hpp"
#include "StreamOutImpl.hpp"
#include "Core/LayerRegistry.hpp"
#include "Core/Profiler.hpp"
#include "ContactListenerImpl.hpp"

namespace NE::Physics {
	namespace {
		// here for now to move to JoltMath
		JPH::Vec3 ToJoltVec3(const Math::Vec3& v) {
			return JPH::Vec3(v.x, v.y, v.z);
		}

		JPH::RMat44 ToJoltRMat44(const Math::Vec3& worldPos) {
			return JPH::RMat44::sTranslation(ToJoltVec3(worldPos));
		}

		JPH::EMotionType ToMotionType(const ECS::Component::Rigidbody& rb) {
			return rb.isKinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic;
		}

		JPH::ObjectLayer ToObjectLayer(uint8_t layerId, JPH::EMotionType motionType) {
			uint8_t baseLayer = layerId % 32;

			if (motionType != JPH::EMotionType::Static) {
				return static_cast<JPH::ObjectLayer>(baseLayer + 32);
			}

			return static_cast<JPH::ObjectLayer>(baseLayer);
		}

		NE::Math::Vec3 ToEngineVec3(const JPH::RVec3& v) {
			return NE::Math::Vec3(v.GetX(), v.GetY(), v.GetZ());
		}

		NE::Math::Vec3 JQuatToDegreeEuler(const JPH::Quat& q) {
			JPH::Vec3 angles = q.GetEulerAngles();
			return NE::Math::Vec3(JPH::RadiansToDegrees(angles.GetX()), JPH::RadiansToDegrees(angles.GetY()), JPH::RadiansToDegrees(angles.GetZ()));
		}

		static float Wrap360(float deg) {
			deg = std::fmod(deg, 360.0f);
			if (deg < 0.0f) deg += 360.0f;
			return deg;
		}

		float ExtractYawDegrees(const JPH::Quat& q) {
			// Choose your engine's forward. Common is +Z forward.
			JPH::Vec3 fwd = q * JPH::Vec3(0, 0, 1);

			// Yaw around Y axis (atan2(x, z)) if +Z is forward
			float yawRad = std::atan2(fwd.GetX(), fwd.GetZ());
			return JPH::RadiansToDegrees(yawRad);
		}

		NE::Math::Quat ToEngineQuat(const JPH::Quat& q) {
			return NE::Math::Quat(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
		}

		JPH::Quat ToJPHQuat(const NE::Math::Quat& q) {
			return JPH::Quat(q.x, q.y, q.z, q.w);
		}

		inline uint32_t FloatBits(float f) {
			uint32_t u;
			static_assert(sizeof(u) == sizeof(f));
			std::memcpy(&u, &f, sizeof(u));
			return u;
		}

		inline void HashCombine(uint64_t& h, uint64_t v) {
			// 64-bit hash combine
			h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		}

		inline uint64_t HashBytes(const void* data, size_t size) {
			// Simple FNV-1a 64 (good enough for signatures)
			const uint8_t* p = (const uint8_t*)data;
			uint64_t h = 1469598103934665603ull;
			for (size_t i = 0; i < size; ++i) {
				h ^= p[i];
				h *= 1099511628211ull;
			}
			return h;
		}
	}

	PhysicsManager& PhysicsManager::GetInstance() {
		static PhysicsManager instance;
		return instance;
	}

	void PhysicsManager::Init() {
		JPH::RegisterDefaultAllocator();
		m_factory = std::make_unique<JPH::Factory>();
		JPH::Factory::sInstance = m_factory.get();
		JPH::RegisterTypes();

		m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
		m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
		m_jobSystem = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

		const auto& layerMatrix = Core::LayerRegistry::GetInstance().GetCollisionMatrix();
		for (int a = 0; a < Core::MAX_LAYERS; ++a) {
			m_collisionMatrix[a] = layerMatrix[a];
		}

		m_objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>(m_collisionMatrix);

		m_bpLayerInterface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
		m_objectVsBpFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();

		m_debugRenderer = std::make_unique<JoltDebugRenderer>();
		JPH::DebugRenderer::sInstance = m_debugRenderer.get();
		m_debugRenderer->Init();

		m_contactListener = std::make_unique<ContactListenerImpl>(this);
		m_physicsSystem->SetContactListener(m_contactListener.get());

		const uint32_t maxBodies = 8192;
		const uint32_t numBodyMutexes = 0;
		const uint32_t maxBodyPairs = 65536;
		const uint32_t maxContactConstraints = 10240;

		m_physicsSystem->Init(
			maxBodies,
			numBodyMutexes,
			maxBodyPairs,
			maxContactConstraints,
			*m_bpLayerInterface,
			*m_objectVsBpFilter,
			*m_objectLayerPairFilter
		);

		m_physicsSystem->SetGravity(JPH::Vec3(0.f, -9.81f, 0.f));
	}

	void PhysicsManager::Update(double dt) {
#ifndef PRODUCTION_BUILD
		NE_PROFILE_FUNCTION();
#endif

		if (!m_physicsSystem)
			return;

		if (dt > m_maxFrameTime)
			dt = m_maxFrameTime;

		m_accumulator += dt;
		bool didStep = false;

		while (m_accumulator >= m_fixedDt) {
			const JPH::EPhysicsUpdateError err = m_physicsSystem->Update(
				m_fixedDt,
				m_collisionSteps,
				m_tempAllocator.get(),
				m_jobSystem.get()
			);

			UpdateCharacters(m_fixedDt);

			// (Optional) handle err; in practice you can log it
			(void)err;

			m_accumulator -= m_fixedDt;
			didStep = true;
		}

		if (didStep)
			FlushContactEventsAndDispatch();
	}

	void PhysicsManager::Shutdown() {
		m_physicsSystem.reset();
		m_jobSystem.reset();
		m_tempAllocator.reset();

		JPH::UnregisterTypes();
		JPH::Factory::sInstance = nullptr;
		m_factory.reset();
	}

	uint64_t PhysicsManager::ComputeShapeSignature(uint32_t entity, const ECS::Component::Collider& col) {
		using Collider = ECS::Component::Collider;

		uint64_t h = 0;
		HashCombine(h, (uint64_t)col.type);

		// center offset matters (because you wrap with RotatedTranslated)
		HashCombine(h, FloatBits(col.center.x));
		HashCombine(h, FloatBits(col.center.y));
		HashCombine(h, FloatBits(col.center.z));

		switch (col.type) {
		case Collider::ColliderType::Box:
		{
			auto he = std::get<Collider::BoxColliderData>(col.data).halfExtents;
			HashCombine(h, FloatBits(he.x));
			HashCombine(h, FloatBits(he.y));
			HashCombine(h, FloatBits(he.z));
			break;
		}
		case Collider::ColliderType::Sphere:
		{
			float r = std::get<Collider::SphereColliderData>(col.data).radius;
			HashCombine(h, FloatBits(r));
			break;
		}
		case Collider::ColliderType::Capsule:
		{
			auto& d = std::get<Collider::CapsuleColliderData>(col.data);
			HashCombine(h, FloatBits(d.height));
			HashCombine(h, FloatBits(d.radius));
			break;
		}
		case Collider::ColliderType::Cylinder:
		{
			auto& d = std::get<Collider::CylinderColliderData>(col.data);
			HashCombine(h, FloatBits(d.height));
			HashCombine(h, FloatBits(d.radius));
			break;
		}
		case Collider::ColliderType::Mesh:
		{
			if (!m_componentManager->HasComponent<ECS::Component::Renderer>(entity))
				return 0;

			auto& rend = m_componentManager->GetComponent<ECS::Component::Renderer>(entity);
			if (!rend.model)
				return 0;

			const uint8_t* blobData = nullptr;
			uint32_t blobSize = 0;
			uint8_t blobType = 0;

			if (!rend.model->GetSubmeshColliderBlob(rend.subMeshIndex, blobData, blobSize, blobType))
				return 0;

			HashCombine(h, (uint64_t)rend.subMeshIndex);
			HashCombine(h, (uint64_t)blobType);
			HashCombine(h, (uint64_t)blobSize);

			// Most robust: hash the blob bytes (fast enough; blobs aren't huge)
			HashCombine(h, HashBytes(blobData, blobSize));

			break;
		}
		default:
			return 0;
		}

		return h;
	}

	// TODO split create and update
	void PhysicsManager::CreateOrUpdateShape(uint32_t entity, uint64_t entityLUID, const ECS::Component::Collider& col) {
		uint64_t newSig = ComputeShapeSignature(entity, col);

		if (newSig == 0) {
			RemoveShape(entityLUID);
			return;
		}

		auto it = m_shapes.find(entityLUID);
		if (it != m_shapes.end() && it->second.signature == newSig)
			return;

		using Collider = ECS::Component::Collider;

		JPH::ShapeRefC base;

		auto CreateShape = [&](auto&& settings) -> JPH::ShapeRefC {
			auto result = settings.Create();
			if (result.HasError()) {
				SPD_WARNING("Failed to create shape");
				return nullptr;
			}
			return result.Get();
			};

		switch (col.type) {
		case Collider::ColliderType::Box: {
			Math::Vec3 halfExtents = std::get<Collider::BoxColliderData>(col.data).halfExtents;
			JPH::BoxShapeSettings s(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));

			base = CreateShape(s);
		} break;
		case Collider::ColliderType::Sphere: {
			JPH::SphereShapeSettings s(std::get<Collider::SphereColliderData>(col.data).radius);

			base = CreateShape(s);
		} break;
		case Collider::ColliderType::Capsule: {
			auto& data = std::get<Collider::CapsuleColliderData>(col.data);
			JPH::CapsuleShapeSettings s(data.height * 0.5f, data.radius);

			base = CreateShape(s);
		} break;
		case Collider::ColliderType::Cylinder: {
			auto& data = std::get<Collider::CylinderColliderData>(col.data);
			JPH::CylinderShapeSettings s(data.height * 0.5f, data.radius);

			base = CreateShape(s);
		} break;
		case Collider::ColliderType::Mesh: {
			auto& data = std::get<Collider::MeshColliderData>(col.data);
			if (!m_componentManager->HasComponent<ECS::Component::Renderer>(entity)) {
				SPD_WARNING("CreateOrUpdateShape: Entity " << entity << " has Mesh collider but no Renderer component.");
				RemoveShape(entityLUID);
				return;
			}
			auto& rend = m_componentManager->GetComponent<ECS::Component::Renderer>(entity);
			if (!rend.model) {
				SPD_WARNING("CreateOrUpdateShape: Entity " << entity << " has Mesh collider but Renderer has no model loaded.");
				RemoveShape(entityLUID);
				return;
			}

			const uint8_t* blobData = nullptr;
			uint32_t blobSize = 0;
			uint8_t blobType = 0;

			if (!rend.model->GetSubmeshColliderBlob(rend.subMeshIndex, blobData, blobSize, blobType)) {
				SPD_WARNING("Mesh collider: no cooked blob for submesh " << rend.subMeshIndex);
				RemoveShape(entityLUID);
				return;
			}

			if (blobType != 1) {
				SPD_WARNING("Mesh collider: unsupported blob type " << (int)blobType);
				RemoveShape(entityLUID);
				return;
			}

			StreamInImpl in(blobData, blobSize);
			JPH::ShapeRefC meshShape = JPH::Shape::sRestoreFromBinaryState(in).Get();
			if (!meshShape || in.IsFailed()) {
				SPD_WARNING("Mesh collider: failed to restore from blob");
				RemoveShape(entityLUID);
				return;
			}
			base = meshShape;
		} break;
		default:
			return;
		}

		if (!base) return;

		JPH::ShapeRefC finalShape = base;
		if (!col.center.Zero()) {
			JPH::RotatedTranslatedShapeSettings rt(
				JPH::Vec3(col.center.x, col.center.y, col.center.z),
				JPH::Quat::sIdentity(),
				base
			);
			finalShape = CreateShape(rt);
			if (!finalShape) return;
		}

		m_shapes[entityLUID] = StoredShape{ finalShape, newSig };
	}

	void PhysicsManager::RemoveShape(const uint64_t entityLUID) {
		m_shapes.erase(entityLUID);
	}

	void PhysicsManager::CreateCharacterController(uint32_t entity, uint64_t entityLUID,
		const ECS::Component::Transform& t, const ECS::Component::CharacterController& cc,
		const ECS::Component::Collider& col, uint8_t layerID)
	{
		JPH::RefConst<JPH::Shape> shape;

		auto itShape = m_shapes.find(entityLUID);
		if (itShape != m_shapes.end() && itShape->second.shape) {
			shape = itShape->second.shape.GetPtr();
		}

		if (!shape) {
			SPD_WARNING("CreateCharacterController failed: missing shape for LUID" << entityLUID);
			return;
		}

		const Math::Vec3 pos = t.worldMatrix.GetTranslation();
		const JPH::RVec3 jPos((double)pos.x, (double)pos.y, (double)pos.z);

		const float yawRad = JPH::DegreesToRadians(t.localRotationEuler.y);
		const JPH::Quat jRot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), yawRad);

		JPH::CharacterVirtualSettings settings;
		settings.mShape = shape;

		settings.mMaxSlopeAngle = JPH::DegreesToRadians(cc.maxSlopeAngleDeg);
		settings.mMaxStrength = cc.maxStrength;
		settings.mCharacterPadding = cc.characterPadding;
		settings.mPenetrationRecoverySpeed = cc.penRecoverySpeed;
		settings.mPredictiveContactDistance = cc.predictiveContactDistance;
		settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
		settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cc.supportingVolumeDepth);

		JPH::Ref<JPH::CharacterVirtual> character = new JPH::CharacterVirtual(
			&settings,
			jPos,
			jRot,
			m_physicsSystem.get()
		);

		CharacterRuntime rt;
		rt.controller = character;
		rt.velocity = JPH::Vec3::sZero();
		rt.entity = entity;
		rt.luid = entityLUID;
		rt.layerID = layerID;

		m_characters[entityLUID] = std::move(rt);
	}

	void PhysicsManager::UpdateCharacters(float dt) {
		const JPH::Vec3 gravity = m_physicsSystem->GetGravity();

		for (auto& [luid, rt] : m_characters) {
			JPH::CharacterVirtual& ch = *rt.controller;

			if (rt.hasPendingDelta) {
				rt.velocity = rt.pendingDelta / dt;
				rt.pendingDelta = JPH::Vec3::sZero();
				rt.hasPendingDelta = false;
			} else {
				rt.velocity = JPH::Vec3::sZero();
			}

			ch.SetLinearVelocity(rt.velocity);

			const uint32_t layerMask = m_collisionMatrix[rt.layerID];
			ObjectLayerFilterImpl layerFilter(layerMask);

			ch.Update(
				dt,
				gravity,
				JPH::BroadPhaseLayerFilter(),
				layerFilter,
				JPH::BodyFilter(),
				JPH::ShapeFilter(),
				*m_tempAllocator
			);

			rt.velocity = ch.GetLinearVelocity();

			ECS::Entity e = static_cast<ECS::Entity>(rt.entity);
			if (m_componentManager->HasComponent<ECS::Component::Transform>(e)) {
				auto& tr = m_componentManager->GetComponent<ECS::Component::Transform>(e);

				const JPH::RVec3 p = ch.GetPosition();
				tr.localPosition = ToEngineVec3(p);

				//            float yaw = Wrap360(ExtractYawDegrees(ch.GetRotation()));
							////SPD_WARNING("Yaw: " << yaw);
				//            tr.localRotationEuler = { 0.0f, yaw, 0.0f };
							//SPD_WARNING(tr.localRotationEuler);

				Math::Quat q = ToEngineQuat(ch.GetRotation());
				tr.localRotationQuat = q;

				//Math::Vec3 e = QuatToEulerDegrees(q);                // any consistent order
				//tr.localRotationEuler = StabilizeEulerForUI(e, tr.localRotationEuler);

				tr.isDirty = true;
			}
		}
	}

	bool PhysicsManager::CharacterIsGrounded(uint64_t entityLUID) const {
		auto it = m_characters.find(entityLUID);
		if (it == m_characters.end()) return false;

		const CharacterRuntime& rt = it->second;

		return rt.controller->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
	}

	void PhysicsManager::CharacterMove(uint64_t entityLUID, const Math::Vec3& delta) {
		auto it = m_characters.find(entityLUID);
		if (it == m_characters.end()) return;

		CharacterRuntime& rt = it->second;

		if (rt.hasPendingDelta)
			rt.pendingDelta += JPH::Vec3(delta.x, delta.y, delta.z);
		else {
			rt.pendingDelta = JPH::Vec3(delta.x, delta.y, delta.z);
			rt.hasPendingDelta = true;
		}
	}

	void PhysicsManager::CharacterRotateYaw(uint64_t entityLUID, float yawDegrees) {
		auto it = m_characters.find(entityLUID);
		if (it == m_characters.end()) return;

		it->second.controller->SetRotation(
			JPH::Quat::sRotation(JPH::Vec3::sAxisY(), JPH::DegreesToRadians(yawDegrees))
		);
	}

	Math::Vec3 PhysicsManager::CharacterGetVelocity(uint64_t entityLUID) const {
		auto it = m_characters.find(entityLUID);
		if (it == m_characters.end()) return { 0.f, 0.f, 0.f };

		const JPH::Vec3& v = it->second.velocity;
		return Math::Vec3(v.GetX(), v.GetY(), v.GetZ());
	}

	Math::Vec3 PhysicsManager::CharacterGetGroundNormal(uint64_t entityLUID) const {
		auto it = m_characters.find(entityLUID);
		if (it == m_characters.end() || !it->second.controller)
			return { 0.f, 1.f, 0.f };

		const JPH::CharacterVirtual& ch = *it->second.controller;

		if (ch.GetGroundState() != JPH::CharacterVirtual::EGroundState::OnGround)
			return { 0.f, 1.f, 0.f };

		const JPH::Vec3 n = ch.GetGroundNormal();

		return { n.GetX(), n.GetY(), n.GetZ() };
	}

	void PhysicsManager::CreateBody(uint32_t entity, uint64_t luid, const ECS::Component::Transform& t,
		const ECS::Component::Rigidbody& rb, const ECS::Component::Collider& col, uint8_t layerID) {
		auto itShape = m_shapes.find(luid);
		if (itShape == m_shapes.end() || !itShape->second.shape)
			return;

		const JPH::ShapeRefC& shape = itShape->second.shape;

		const Math::Vec3 pos = t.worldMatrix.GetTranslation();
		const JPH::RVec3 jPos((double)pos.x, (double)pos.y, (double)pos.z);
		//const JPH::Quat jRot = JPH::Quat::sEulerAngles({
		//	JPH::DegreesToRadians(t.localRotationEuler.x),
		//	JPH::DegreesToRadians(t.localRotationEuler.y),
		//	JPH::DegreesToRadians(t.localRotationEuler.z) }
		//	);

		const JPH::Quat jRot = ToJPHQuat(t.localRotationQuat);

		const JPH::EMotionType motion = ToMotionType(rb);
		const JPH::ObjectLayer objLayer = ToObjectLayer(layerID, motion);

		JPH::BodyCreationSettings settings(
			shape,
			jPos,
			jRot,
			motion,
			objLayer
		);

		settings.mGravityFactor = rb.useGravity ? 1.0f : 0.0f;
		settings.mLinearDamping = rb.linearDamping;
		settings.mAngularDamping = rb.angularDamping;
		settings.mIsSensor = col.isTrigger;

		JPH::EAllowedDOFs allowedDOFs = JPH::EAllowedDOFs::All;

		if (rb.freezePosX) allowedDOFs &= ~JPH::EAllowedDOFs::TranslationX;
		if (rb.freezePosY) allowedDOFs &= ~JPH::EAllowedDOFs::TranslationY;
		if (rb.freezePosZ) allowedDOFs &= ~JPH::EAllowedDOFs::TranslationZ;

		if (rb.freezeRotX) allowedDOFs &= ~JPH::EAllowedDOFs::RotationX;
		if (rb.freezeRotY) allowedDOFs &= ~JPH::EAllowedDOFs::RotationY;
		if (rb.freezeRotZ) allowedDOFs &= ~JPH::EAllowedDOFs::RotationZ;

		settings.mAllowedDOFs = allowedDOFs;

		if (motion == JPH::EMotionType::Dynamic) {
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			settings.mMassPropertiesOverride.mMass = rb.mass;
		}

		JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();
		JPH::Body* body = bi.CreateBody(settings);
		if (!body) return;

		const JPH::BodyID id = body->GetID();
		bi.AddBody(id, JPH::EActivation::Activate);
		bi.SetUserData(id, static_cast<JPH::uint64>(entity));

		m_bodies[luid] = id;
		m_bodyToLuid[id] = luid;
	}

	void PhysicsManager::CreateBody(uint32_t entity, uint64_t luid, const ECS::Component::Transform& t,
		const ECS::Component::Collider& col, uint8_t layerID) {
		auto itShape = m_shapes.find(luid);
		if (itShape == m_shapes.end() || !itShape->second.shape)
			return;

		const JPH::ShapeRefC& shape = itShape->second.shape;

		const Math::Vec3 pos = t.worldMatrix.GetTranslation();
		const JPH::RVec3 jPos((double)pos.x, (double)pos.y, (double)pos.z);
		const JPH::Quat jRot = JPH::Quat::sEulerAngles({
			JPH::DegreesToRadians(t.localRotationEuler.x),
			JPH::DegreesToRadians(t.localRotationEuler.y),
			JPH::DegreesToRadians(t.localRotationEuler.z) }
			);

		const JPH::EMotionType motion = JPH::EMotionType::Static;
		const JPH::ObjectLayer objLayer = ToObjectLayer(layerID, motion);

		JPH::BodyCreationSettings settings(
			shape,
			jPos,
			jRot,
			motion,
			objLayer
		);

		settings.mIsSensor = col.isTrigger;

		JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();
		JPH::Body* body = bi.CreateBody(settings);
		if (!body) return;

		const JPH::BodyID id = body->GetID();
		bi.AddBody(id, JPH::EActivation::DontActivate);
		bi.SetUserData(id, static_cast<JPH::uint64>(entity));

		m_bodies[luid] = id;
		m_bodyToLuid[id] = luid;
	}

	void PhysicsManager::DestroyBody(uint64_t luid) {
		auto it = m_bodies.find(luid);
		if (it == m_bodies.end()) return;

		JPH::BodyID id = it->second;

		JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();
		bi.RemoveBody(id);
		bi.DestroyBody(id);

		m_bodies.erase(it);
		m_bodyToLuid.erase(id);

		RemoveContactsInvolving(luid);
	}

	void PhysicsManager::RemoveContactsInvolving(uint64_t luid) {
		for (auto it = m_prevContacts.begin(); it != m_prevContacts.end(); ) {
			if (it->a == luid || it->b == luid)
				it = m_prevContacts.erase(it);
			else
				++it;
		}
	}

	void PhysicsManager::SyncTransformToBodies(uint64_t luid, ECS::Component::Transform& t) const {
		JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();

		auto& bodyID = m_bodies.at(luid);
		t.localPosition = ToEngineVec3(bi.GetPosition(bodyID));
		t.localRotationEuler = JQuatToDegreeEuler(bi.GetRotation(bodyID));
		t.localRotationQuat = ToEngineQuat(bi.GetRotation(bodyID));

		t.isDirty = true;
	}

	void PhysicsManager::SyncTransformToCharacters(uint64_t entityLUID, ECS::Component::Transform& t) const {
		auto& rt = m_characters.at(entityLUID);
		auto& ch = *rt.controller;
		t.localPosition = ToEngineVec3(ch.GetPosition());

		auto euler = JQuatToDegreeEuler(ch.GetRotation());
		euler.x = 0.0f;
		euler.z = 0.0f;
		t.localRotationEuler = euler;
		t.localRotationQuat = ToEngineQuat(ch.GetRotation());

		t.isDirty = true;
	}

	void PhysicsManager::DrawShapeGizmo(const uint64_t entityLUID, const ECS::Component::Transform& t, const ECS::Component::Collider& col) {
		auto it = m_shapes.find(entityLUID);
		if (it == m_shapes.end()) return;

		auto& shapeSettings = it->second;

		const Math::Vec3 pos = t.worldMatrix.GetTranslation();
		//const JPH::Quat rot = ToJPHQuat(t.loc)
		const JPH::Quat rot = JPH::Quat::sEulerAngles({
				JPH::DegreesToRadians(t.localRotationEuler.x),
				JPH::DegreesToRadians(t.localRotationEuler.y),
				JPH::DegreesToRadians(t.localRotationEuler.z)
			}
		);

		JPH::RMat44 world = JPH::RMat44::sRotationTranslation(
			rot,
			JPH::RVec3((double)pos.x, (double)pos.y, (double)pos.z)
		);

		world = world * JPH::RMat44::sTranslation(JPH::Vec3(col.center.x, col.center.y, col.center.z));

		it->second.shape->Draw(m_debugRenderer.get(),
			world,
			JPH::Vec3::sReplicate(1.f),
			JPH::Color::sGreen,
			false,
			true
		);
	}

	bool PhysicsManager::Raycast(Math::Vec3 origin, Math::Vec3 direction, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask) {
		JPH::Vec3 joltDir(direction.x, direction.y, direction.z);
		joltDir = joltDir.Normalized();

		JPH::RRayCast joltRay{ ToJoltVec3(origin), ToJoltVec3(direction * maxDistance) };

		ObjectLayerFilterImpl layerFilter(layerMask);

		JPH::RayCastResult result;
		bool hasHit = m_physicsSystem->GetNarrowPhaseQuery().CastRay(
			joltRay,
			result,
			JPH::BroadPhaseLayerFilter(),
			layerFilter,
			JPH::BodyFilter()
		);

		if (hasHit && !result.mBodyID.IsInvalid()) {
			outHitInfo.distance = result.mFraction * maxDistance;

			JPH::RVec3 hitPoint = joltRay.mOrigin + joltRay.mDirection * result.mFraction;
			outHitInfo.point = Math::Vec3(
				hitPoint.GetX(),
				hitPoint.GetY(),
				hitPoint.GetZ()
			);

			JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), result.mBodyID);
			if (lock.Succeeded()) {
				const JPH::Body& body = lock.GetBody();

				JPH::Vec3 joltNormal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, hitPoint);
				outHitInfo.normal = Math::Vec3(joltNormal.GetX(), joltNormal.GetY(), joltNormal.GetZ());

				// Get entity from physics body
				ECS::Entity hitEntity = static_cast<ECS::Entity>(body.GetUserData());
				outHitInfo.colliderEntityID = hitEntity;

				// Populate component LUIDs using ComponentManager
				if (m_componentManager) {
					if (m_componentManager->HasComponent<ECS::Component::Transform>(hitEntity)) {
						auto& transform = m_componentManager->GetComponent<ECS::Component::Transform>(hitEntity);
						outHitInfo.transformLuid = transform.luid;
					}

					if (m_componentManager->HasComponent<ECS::Component::Rigidbody>(hitEntity)) {
						auto& rigidbody = m_componentManager->GetComponent<ECS::Component::Rigidbody>(hitEntity);
						outHitInfo.rigidbodyLuid = rigidbody.luid;
					}

					if (m_componentManager->HasComponent<ECS::Component::Collider>(hitEntity)) {
						auto& collider = m_componentManager->GetComponent<ECS::Component::Collider>(hitEntity);
						outHitInfo.colliderLuid = collider.luid;
					}
				}
			}

			//SPD_DEBUG("Raycast Hit Body with ID: " << hit.bodyID << " with Entity: " << hit.entity);
		}

		return hasHit;
	}

	bool PhysicsManager::Raycast(Ray ray, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask) {
		return Raycast(ray.origin, ray.direction, outHitInfo, maxDistance, layerMask);
	}

	bool PhysicsManager::SphereCast(Math::Vec3 origin, float radius, Math::Vec3 direction, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask) {
		JPH::Vec3 joltDir(direction.x, direction.y, direction.z);
		joltDir = joltDir.Normalized();

		JPH::SphereShapeSettings sphereSettings(radius);
		JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();

		if (sphereResult.HasError()) {
			SPD_WARNING("Failed to create sphere shape for sphere cast");
			return false;
		}

		JPH::RefConst<JPH::Shape> sphereShape = sphereResult.Get();

		JPH::RMat44 startWorld = JPH::RMat44::sTranslation(JPH::RVec3(origin.x, origin.y, origin.z));

		JPH::Vec3 castDir = joltDir * maxDistance;

		JPH::RShapeCast sphereCast =
			JPH::RShapeCast::sFromWorldTransform(
				sphereShape.GetPtr(),
				JPH::Vec3::sReplicate(1.0f),
				startWorld,
				castDir
			);

		ObjectLayerFilterImpl layerFilter(layerMask);

		using CollectorT = JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>;
		CollectorT collector;

		JPH::ShapeCastSettings settings;

		JPH::RVec3 baseOffset = JPH::RVec3::sZero();

		m_physicsSystem->GetNarrowPhaseQuery().CastShape(
			sphereCast,
			settings,
			baseOffset,
			collector,
			JPH::BroadPhaseLayerFilter(),
			layerFilter,
			JPH::BodyFilter(),
			JPH::ShapeFilter()
		);

		if (!collector.HadHit() || collector.mHit.mBodyID2.IsInvalid())
			return false;

		const JPH::ShapeCastResult& hit = collector.mHit;

		outHitInfo.distance = hit.mFraction * maxDistance;

		const JPH::Vec3 p = hit.mContactPointOn2;
		outHitInfo.point = Math::Vec3(p.GetX(), p.GetY(), p.GetZ());

		JPH::Vec3 n = -hit.mPenetrationAxis.Normalized();
		outHitInfo.normal = Math::Vec3(n.GetX(), n.GetY(), n.GetZ());

		JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), hit.mBodyID2);
		if (lock.Succeeded()) {
			const JPH::Body& body = lock.GetBody();

			ECS::Entity hitEntity = static_cast<ECS::Entity>(body.GetUserData());
			outHitInfo.colliderEntityID = hitEntity;

			if (m_componentManager) {
				if (m_componentManager->HasComponent<ECS::Component::Transform>(hitEntity))
					outHitInfo.transformLuid = m_componentManager->GetComponent<ECS::Component::Transform>(hitEntity).luid;

				if (m_componentManager->HasComponent<ECS::Component::Rigidbody>(hitEntity))
					outHitInfo.rigidbodyLuid = m_componentManager->GetComponent<ECS::Component::Rigidbody>(hitEntity).luid;

				if (m_componentManager->HasComponent<ECS::Component::Collider>(hitEntity))
					outHitInfo.colliderLuid = m_componentManager->GetComponent<ECS::Component::Collider>(hitEntity).luid;
			}
		}

		return true;
	}

	bool PhysicsManager::SphereCast(Ray ray, float radius, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask) {
		return SphereCast(ray.origin, radius, ray.direction, outHitInfo, maxDistance, layerMask);
	}

	void PhysicsManager::AddForce(uint64_t entityLUID, Math::Vec3 force, ForceMode forceMode) {
		auto it = m_bodies.find(entityLUID);
		if (it != m_bodies.end()) {
			switch (forceMode) {
			case ForceMode::Impulse: {
				m_physicsSystem->GetBodyInterface().AddImpulse(it->second, ToJoltVec3(force));
			} break;
								   //case ForceMode::Acceleration: {
								   //} break;
								   //case ForceMode::VelocityChange: {
								   //} break;
			default: {
				m_physicsSystem->GetBodyInterface().AddForce(it->second, ToJoltVec3(force));
			}
			}
		}
	}

	Math::Vec3 PhysicsManager::GetLinearVelocity(uint64_t entityLUID) const {
		auto it = m_bodies.find(entityLUID);
		if (it != m_bodies.end()) {
			JPH::Vec3 joltVel = m_physicsSystem->GetBodyInterface().GetLinearVelocity(it->second);
			return Math::Vec3(joltVel.GetX(), joltVel.GetY(), joltVel.GetZ());
		}
		return Math::Vec3(0.0f, 0.0f, 0.0f);
	}

	void PhysicsManager::SetLinearVelocity(uint64_t entityLUID, const Math::Vec3& velocity) {
		auto it = m_bodies.find(entityLUID);
		if (it != m_bodies.end()) {
			m_physicsSystem->GetBodyInterface().SetLinearVelocity(it->second, ToJoltVec3(velocity));
		}
	}

	Math::Vec3 PhysicsManager::GetAngularVelocity(uint64_t entityLUID) const {
		auto it = m_bodies.find(entityLUID);
		if (it != m_bodies.end()) {
			JPH::Vec3 joltAngVel = m_physicsSystem->GetBodyInterface().GetAngularVelocity(it->second);
			return Math::Vec3(joltAngVel.GetX(), joltAngVel.GetY(), joltAngVel.GetZ());
		}
		return Math::Vec3(0.0f, 0.0f, 0.0f);
	}

	void PhysicsManager::SetAngularVelocity(uint64_t entityLUID, const Math::Vec3& angularVelocity) {
		auto it = m_bodies.find(entityLUID);
		if (it != m_bodies.end()) {
			m_physicsSystem->GetBodyInterface().SetAngularVelocity(it->second, ToJoltVec3(angularVelocity));
		}
	}

	bool PhysicsManager::CookMeshCollider(const std::vector<Math::Vec3>& vertices,
		const std::vector<uint32_t>& indices, std::vector<uint8_t>& outBlob) {
		if (!m_physicsSystem)
			return false;

		if (vertices.empty() || indices.size() < 3 || indices.size() % 3 != 0) {
			SPD_ERROR("PhysicsManager::CookMeshCollider -  invalid mesh data");
			return false;
		}

		JPH::VertexList joltVerts;
		joltVerts.reserve(vertices.size());
		for (const auto& v : vertices) {
			joltVerts.emplace_back(v.x, v.y, v.z);
		}

		JPH::IndexedTriangleList tris;
		tris.reserve(indices.size() / 3);

		// IndexedTriangle (i0, i1, i2, materialIndex (0 for now))
		for (size_t i = 0; i < indices.size(); i += 3) {
			uint32_t i0 = indices[i + 0];
			uint32_t i1 = indices[i + 1];
			uint32_t i2 = indices[i + 2];

			tris.emplace_back(
				i0,
				i1,
				i2,
				0u
			);
		}

		JPH::MeshShapeSettings meshSettings(std::move(joltVerts), std::move(tris));

		meshSettings.mBuildQuality = JPH::MeshShapeSettings::EBuildQuality::FavorRuntimePerformance;
		meshSettings.mPerTriangleUserData = false;

		JPH::ShapeSettings::ShapeResult shapeResult = meshSettings.Create();
		if (shapeResult.HasError()) {
			SPD_ERROR("PhysicsManager::CookMeshCollider - error: " << shapeResult.GetError().c_str());
			return false;
		}

		JPH::RefConst<JPH::Shape> meshShape = shapeResult.Get();
		NE::Physics::StreamOutImpl outBlobStream(outBlob);
		meshShape->SaveBinaryState(outBlobStream);
		meshShape->SaveBinaryState(outBlobStream);
		if (outBlobStream.IsFailed()) {
			SPD_ERROR("CookMeshCollider - failed to serialize shape");
			outBlob.clear();
			return false;
		}

		return true;
	}

	uint64_t PhysicsManager::BodyToLuid(JPH::BodyID bodyID) const {
		auto it = m_bodyToLuid.find(bodyID);
		return it == m_bodyToLuid.end() ? 0 : it->second;
	}

	void PhysicsManager::PushRawContactEvent(const RawContactEvent& e) {
		std::scoped_lock lock(m_contactEventMutex);
		m_contactEventsWrite.push_back(e);
	}

	void PhysicsManager::FlushContactEventsAndDispatch() {
		{
			std::scoped_lock lock(m_contactEventMutex);
			m_contactEventsRead.swap(m_contactEventsWrite);
			m_contactEventsWrite.clear();
		}

		m_currContacts.clear();
		for (const RawContactEvent& e : m_contactEventsRead) {
			if (e.type == ContactEventType::Added || e.type == ContactEventType::Persisted) {
				m_currContacts.insert(ContactKey::Make(e.aLuid, e.bLuid, e.isTrigger));
			}
		}

		for (const auto& k : m_currContacts) {
			if (m_prevContacts.find(k) == m_prevContacts.end()) {
				DispatchEnter(k);
			} else {
				DispatchStay(k);
			}
		}

		for (const auto& k : m_prevContacts) {
			if (m_currContacts.find(k) == m_currContacts.end()) {
				DispatchExit(k);
			}
		}

		m_prevContacts.swap(m_currContacts);
		m_contactEventsRead.clear();
	}

	void PhysicsManager::DispatchEnter(const ContactKey& k) {
		const uint32_t aEnt = m_luidRegistry->Find(k.a)->m_entityOwner;
		const uint32_t bEnt = m_luidRegistry->Find(k.b)->m_entityOwner;

		if (k.isTrigger) {
			Scripting::ScriptingEngine::GetInstance().OnTriggerEnter(aEnt, bEnt);
			Scripting::ScriptingEngine::GetInstance().OnTriggerEnter(bEnt, aEnt);
		} else {
			Scripting::ScriptingEngine::GetInstance().OnCollisionEnter(aEnt, bEnt);
			Scripting::ScriptingEngine::GetInstance().OnCollisionEnter(bEnt, aEnt);
		}
	}

	void PhysicsManager::DispatchExit(const ContactKey& k) {
		const uint32_t aEnt = m_luidRegistry->Find(k.a)->m_entityOwner;
		const uint32_t bEnt = m_luidRegistry->Find(k.b)->m_entityOwner;

		if (k.isTrigger) {
			Scripting::ScriptingEngine::GetInstance().OnTriggerExit(aEnt, bEnt);
			Scripting::ScriptingEngine::GetInstance().OnTriggerExit(bEnt, aEnt);
		} else {
			Scripting::ScriptingEngine::GetInstance().OnCollisionExit(aEnt, bEnt);
			Scripting::ScriptingEngine::GetInstance().OnCollisionExit(bEnt, aEnt);
		}
	}

	void PhysicsManager::DispatchStay(const ContactKey& k) {
		const uint32_t aEnt = m_luidRegistry->Find(k.a)->m_entityOwner;
		const uint32_t bEnt = m_luidRegistry->Find(k.b)->m_entityOwner;

		if (k.isTrigger) {
			Scripting::ScriptingEngine::GetInstance().OnTriggerStay(aEnt, bEnt);
			Scripting::ScriptingEngine::GetInstance().OnTriggerStay(bEnt, aEnt);
		} else {
			Scripting::ScriptingEngine::GetInstance().OnCollisionStay(aEnt, bEnt);
			Scripting::ScriptingEngine::GetInstance().OnCollisionStay(bEnt, aEnt);
		}
	}

	void PhysicsManager::OnPlay() {
	}

	void PhysicsManager::OnStop() {
		JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();

		for (auto& [luid, id] : m_bodies) {
			bi.RemoveBody(id);
			bi.DestroyBody(id);
		}
		m_bodies.clear();
		m_bodyToLuid.clear();
		m_characters.clear();

		m_prevContacts.clear();
		m_currContacts.clear();
		m_contactEventsWrite.clear();
		m_contactEventsRead.clear();
	}

	void PhysicsManager::SetManagers(ECS::ComponentManager* cm, Core::LUIDRegistry* lg) {
		m_componentManager = cm;
		m_luidRegistry = lg;
	}
}