/**
 * @file ScriptAPI.cpp
 * @brief Implementation of the clean scripting SDK API
 *
 * This file acts as an adapter/bridge between the public SDK API
 * (ScriptSDK headers) and the internal engine implementation (IScript).
 */

#include "../../include/ScriptSDK/ScriptAPI.h"
#include "../../include/ScriptSDK/ScriptMacros.h"
#include "ScriptContext.hpp"
#include "ScriptContextFactory.hpp"
#include "ScriptingEngine.hpp"

 // Internal engine headers (NOT exposed to scripts)
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/AudioSource.hpp"
#include "../ECS/Components/EntityMeta.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/Camera.hpp"
#include "../ECS/Components/NativeScript.hpp"
#include "../ECS/Components/Hierarchy.hpp"
#include "../Physics/PhysicsManager.hpp"
#include "../Physics/ForceMode.hpp"
#include "../Physics/RaycastHit.hpp"
#include "../Core/LUIDRegistry.hpp"
#include <Math/Vec3.hpp>
#include "../Core/SpdLogger.hpp"
#include "../Core/Couroutine.hpp"
#include "../Input/InputManager.hpp"
#include "../Events/EventBus.hpp"
#include "../Tween/TweenManager.hpp"  // Include TweenManager for tween API
#include "SceneManagement/SceneManager.hpp"
#include "../EditorInterface/RendererExports.hpp"  // For RenderSettings access
#include "../Graphics/Core/RenderSettings.hpp"  // For RenderSettings struct

#include <sstream>
#include <unordered_map>
#include <functional>
#include <cmath>

namespace NE {
	SceneManagement::Scene& GetScene();
	extern SceneManagement::SceneManager gSceneManager;
}

namespace NE {
	namespace Scripting {
		//=========================================================================
		// TYPE CONVERSION UTILITIES (SDK ↔ Engine)
		//=========================================================================

		inline Math::Vec3 ToEngineVec3(const Vec3& v) {
			return Math::Vec3(v.x, v.y, v.z);
		}

		inline Vec3 ToSDKVec3(const Math::Vec3& v) {
			return Vec3(v.x, v.y, v.z);
		}

		//=========================================================================
		// LUID ↔ ENTITY CONVERSION UTILITIES
		//=========================================================================

		/**
		 * Get LUID from Entity ID.
		 * Returns 0 if entity doesn't exist or has no EntityMeta component.
		 */
		inline uint64_t GetLUIDFromEntity(Entity entity, ECS::ComponentManager* componentManager) {
			if (!componentManager || entity == INVALID_ENTITY) {
				return 0;
			}

			if (!componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
				return 0;
			}

			return componentManager->GetComponent<ECS::Component::EntityMeta>(entity).luid;
		}

		/**
		 * Get Entity ID from LUID.
		 * Returns INVALID_ENTITY if LUID not found.
		 * This iterates through all entities, so it's not super fast - use sparingly.
		 */
		inline Entity GetEntityFromLUID(uint64_t luid, ECS::ComponentManager* componentManager, ECS::EntityManager* entityManager) {
			if (!componentManager || !entityManager || luid == 0) {
				return INVALID_ENTITY;
			}

			// Iterate through all active entities to find matching LUID
			const auto& usedEntities = entityManager->GetUsedEntities();
			for (Entity entity : usedEntities) {
				if (componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
					const auto& meta = componentManager->GetComponent<ECS::Component::EntityMeta>(entity);
					if (meta.luid == luid) {
						return entity;
					}
				}
			}

			return INVALID_ENTITY;
		}

		//=========================================================================
		// LUID COMPONENT RESOLUTION HELPER
		//=========================================================================

		/**
		 * Resolve a component by LUID using the LUID registry.
		 * Returns nullptr if LUID is invalid or component not found.
		 */
		template<typename T>
		inline T* ResolveComponentByLuid(uint64_t luid, Core::LUIDRegistry* registry) {
			if (luid == 0 || !registry) return nullptr;

			auto* record = registry->Find(luid);
			if (!record) return nullptr;

			return static_cast<T*>(record->m_ptr);
		}

		/**
		 * Get the Entity that owns a component identified by LUID.
		 * Returns INVALID_ENTITY if LUID not found.
		 */
		inline Entity GetEntityFromComponentLuid(uint64_t luid, Core::LUIDRegistry* registry) {
			if (luid == 0 || !registry) return INVALID_ENTITY;

			auto* record = registry->Find(luid);
			if (!record) return INVALID_ENTITY;

			return static_cast<Entity>(record->m_entityOwner);
		}

		// Vector normalization helper
		inline Vec3 Normalize(const Vec3& v) {
			float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
			if (length > 0.0001f) {
				return Vec3(v.x / length, v.y / length, v.z / length);
			}
			return v;
		}

		// Context validation macro for consistent null-checking
#define CHECK_CONTEXT_OR_RETURN(returnValue) \
        if (!m_context || !m_context->componentManager) return returnValue

	//=========================================================================
	// FIELD REGISTRY (PIMPL - Hide STL containers from DLL interface)
	//=========================================================================

		class IScript::FieldRegistry {
		public:
			struct FieldEntry {
				std::string typeToken;
				void* memberPtr;
				std::function<std::string()> getValue;
				std::function<bool(const std::string&)> setValue;

				// Array operation callbacks for vector fields
				std::function<size_t()> getSize;
				std::function<std::string(size_t)> getElement;
				std::function<bool(size_t, const std::string&)> setElement;
				std::function<void()> addElement;
				std::function<void(size_t)> removeElement;

				// Enum support
				std::vector<std::string> enumOptions;
				std::function<int()> getEnumValue;
				std::function<void(int)> setEnumValue;

				// LayerMask support
				std::function<uint32_t()> getLayerMaskValue;
				std::function<void(uint32_t)> setLayerMaskValue;
			};

			std::unordered_map<std::string, FieldEntry> fields;
		};

		//=========================================================================
		// IScript Implementation
		//=========================================================================

		IScript::~IScript() {
			delete m_fieldRegistry;
			// Properly clean up ScriptContext to avoid memory leak
			if (m_context) {
				DestroyScriptContext(m_context);
				m_context = nullptr;
			}
		}

		void IScript::_LinkToEngine(ScriptContext* context) {
			m_context = context;

			// Initialize field registry if not already done
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}
		}

		void IScript::_RefreshComponentReferences() {
			// This would be implemented by the engine to update component pointers
			// after hot reload or scene changes
		}

		void IScript::SetEnabled(bool enabled) {
			if (m_enabled != enabled) {
				m_enabled = enabled;
				if (enabled) {
					OnEnable();
				} else {
					OnDisable();
				}
			}
		}

		//=========================================================================
		// Transform Operations
		//=========================================================================

		Vec3 IScript::TF_GetPosition(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

			// Use m_entity if entity is DEFAULT_ENTITY_PARAM
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
				return Vec3::Zero();

			auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
			return ToSDKVec3(transform.localPosition);
		}

		Vec3 IScript::TF_GetWorldPosition(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
				return Vec3::Zero();

			auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
			Math::Mat4 m = transform.worldMatrix;
			Math::Vec3 worldPos = m.GetTranslation();
			return ToSDKVec3(worldPos);
		}

		void IScript::TF_SetPosition(const Vec3& pos, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity)) {
				auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
				transform.localPosition = ToEngineVec3(pos);
				transform.isDirty = true;
			}
		}

		void IScript::TF_SetPosition(float x, float y, float z, Entity entity) {
			TF_SetPosition(Vec3(x, y, z), entity);
		}

		Vec3 IScript::TF_GetRotation(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
				return Vec3::Zero();

			auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
			return ToSDKVec3(transform.localRotationEuler);
		}

		void IScript::TF_SetRotation(const Vec3& rot, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity)) {
				auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
				transform.localRotationEuler = ToEngineVec3(rot);
				transform.isDirty = true;
			}
		}

		void IScript::TF_SetRotation(float x, float y, float z, Entity entity) {
			TF_SetRotation(Vec3(x, y, z), entity);
		}

		Vec3 IScript::TF_GetScale(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(Vec3::One());

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity))
				return Vec3::One();

			auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
			return ToSDKVec3(transform.localScale);
		}

		void IScript::TF_SetScale(const Vec3& scale, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (m_context->componentManager->HasComponent<ECS::Component::Transform>(targetEntity)) {
				auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(targetEntity);
				transform.localScale = ToEngineVec3(scale);
				transform.isDirty = true;
			}
		}

		void IScript::TF_SetScale(float x, float y, float z, Entity entity) {
			TF_SetScale(Vec3(x, y, z), entity);
		}

		void IScript::TF_SetScale(float uniformScale, Entity entity) {
			TF_SetScale(Vec3(uniformScale, uniformScale, uniformScale), entity);
		}

		void IScript::TF_Translate(const Vec3& translation, Entity entity) {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			TF_SetPosition(TF_GetPosition(targetEntity) + translation, targetEntity);
		}

		void IScript::TF_Translate(float x, float y, float z, Entity entity) {
			TF_Translate(Vec3(x, y, z), entity);
		}

		void IScript::TF_Rotate(const Vec3& rotation, Entity entity) {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			TF_SetRotation(TF_GetRotation(targetEntity) + rotation, targetEntity);
		}

		void IScript::TF_Rotate(float x, float y, float z, Entity entity) {
			TF_Rotate(Vec3(x, y, z), entity);
		}

		Vec3 IScript::TF_GetForward(Entity entity) const {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			Vec3 rotation = TF_GetRotation(targetEntity); // (pitch, yaw, roll) in degrees

			float pitch = rotation.x * Math::DEG_TO_RAD;
			float yaw = rotation.y * Math::DEG_TO_RAD;

			Vec3 forward;
			forward.x = std::cos(pitch) * std::cos(yaw);
			forward.y = std::sin(pitch);
			forward.z = std::cos(pitch) * std::sin(yaw);

			return forward.Normalized();
		}

		Vec3 IScript::TF_GetRight(Entity entity) const {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			Vec3 rotation = TF_GetRotation(targetEntity); // (pitch, yaw, roll) in degrees

			// Convert degrees to radians
			float yaw = rotation.y * Math::DEG_TO_RAD;

			// Right vector is perpendicular to forward in XZ plane
			Vec3 right;
			right.x = std::cos(yaw);
			right.y = 0.0f;
			right.z = std::sin(yaw);

			return Normalize(right);
		}

		Vec3 IScript::TF_GetUp(Entity entity) const {
			// Up is always world up in this simple implementation
			// For more complex scenarios, you might want to calculate it from forward and right
			return Vec3(0.0f, 1.0f, 0.0f);
		}

		//=========================================================================
		// Hierarchy Operations
		//=========================================================================

		Entity IScript::GetParent(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(INVALID_ENTITY);

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Hierarchy>(targetEntity))
				return INVALID_ENTITY;

			auto& hierarchy = m_context->componentManager->GetComponent<ECS::Component::Hierarchy>(targetEntity);
			return (hierarchy.parent == ECS::Component::INVALID_PARENT) ? INVALID_ENTITY : hierarchy.parent;
		}

		size_t IScript::GetChildCount(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(0);

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Hierarchy>(targetEntity))
				return 0;

			auto& hierarchy = m_context->componentManager->GetComponent<ECS::Component::Hierarchy>(targetEntity);
			return hierarchy.children.size();
		}

		Entity IScript::GetChild(size_t index, Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(INVALID_ENTITY);

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Hierarchy>(targetEntity))
				return INVALID_ENTITY;

			auto& hierarchy = m_context->componentManager->GetComponent<ECS::Component::Hierarchy>(targetEntity);

			if (index >= hierarchy.children.size())
				return INVALID_ENTITY;

			return hierarchy.children[index];
		}

		std::vector<Entity> IScript::GetChildren(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(std::vector<Entity>());

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Hierarchy>(targetEntity))
				return std::vector<Entity>();

			auto& hierarchy = m_context->componentManager->GetComponent<ECS::Component::Hierarchy>(targetEntity);
			return hierarchy.children;
		}

		//=========================================================================
		// Rigidbody Physics
		//=========================================================================

		bool IScript::RB_HasRigidbody(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(false);

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			return m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity);
		}

		float IScript::RB_GetMass(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(0.0f);

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity))
				return 0.0f;

			return m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity).mass;
		}

		void IScript::RB_SetMass(float mass, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity)) {
				auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity);
				rigidbody.mass = mass;
			}
		}

		bool IScript::RB_GetUseGravity(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(false);

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity))
				return false;

			return m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity).useGravity;
		}

		void IScript::RB_SetUseGravity(bool use, Entity entity) {
			//Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			//if (!Physics::PhysicsManager::EntityHasPhysicsBody(targetEntity)) return;

			//uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(targetEntity);
			//Physics::PhysicsManager::SetGravityEnabled(bodyID, use);

			//// Also update Rigidbody component if it exists
			//if (m_context && m_context->componentManager &&
			//	m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity)) {
			//	auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity);
			//	rigidbody.useGravity = use;
			//}
		}

		bool IScript::RB_IsStatic(Entity entity) const {
			/*if (!m_context || !m_context->componentManager) return false;

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity))
				return false;

			return m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity).isStatic;*/
			return false;
		}

		void IScript::RB_SetStatic(bool isStatic, Entity entity) {
			/*if (!m_context || !m_context->componentManager) return;

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(targetEntity)) {
				auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(targetEntity);
				rigidbody.isStatic = isStatic;
			}*/
		}

		void IScript::RB_LockRotation(bool lockX, bool lockY, bool lockZ, Entity entity) {
			/*Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!Physics::PhysicsManager::EntityHasPhysicsBody(targetEntity)) return;

			uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(targetEntity);
			Physics::PhysicsManager::LockRotation(bodyID, lockX, lockY, lockZ);*/
		}

		Vec3 IScript::RB_GetVelocity(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			// Get entity LUID (needed for PhysicsManager which operates on entity LUIDs)
			if (!m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(targetEntity)) {
				return Vec3::Zero();
			}

			auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(targetEntity);
			return ToSDKVec3(Physics::PhysicsManager::GetInstance().GetLinearVelocity(meta.luid));
		}

		void IScript::RB_SetVelocity(const Vec3& velocity, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			// Get entity LUID (needed for PhysicsManager which operates on entity LUIDs)
			if (!m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(targetEntity)) {
				return;
			}

			auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(targetEntity);
			Physics::PhysicsManager::GetInstance().SetLinearVelocity(meta.luid, ToEngineVec3(velocity));
		}

		void IScript::RB_SetVelocity(float x, float y, float z, Entity entity) {
			RB_SetVelocity(Vec3(x, y, z), entity);
		}

		Vec3 IScript::RB_GetAngularVelocity(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(Vec3::Zero());

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			// Get entity LUID (needed for PhysicsManager which operates on entity LUIDs)
			if (!m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(targetEntity)) {
				return Vec3::Zero();
			}

			auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(targetEntity);
			return ToSDKVec3(Physics::PhysicsManager::GetInstance().GetAngularVelocity(meta.luid));
		}

		void IScript::RB_SetAngularVelocity(const Vec3& angularVelocity, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			// Get entity LUID (needed for PhysicsManager which operates on entity LUIDs)
			if (!m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(targetEntity)) {
				return;
			}

			auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(targetEntity);
			Physics::PhysicsManager::GetInstance().SetAngularVelocity(meta.luid, ToEngineVec3(angularVelocity));
		}

		void IScript::RB_SetAngularVelocity(float x, float y, float z, Entity entity) {
			RB_SetAngularVelocity(Vec3(x, y, z), entity);
		}

		void IScript::RB_AddForce(const Vec3& force, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			// Get entity LUID (needed for PhysicsManager which operates on entity LUIDs)
			if (!m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(targetEntity)) {
				return;
			}

			auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(targetEntity);
			Physics::PhysicsManager::GetInstance().AddForce(meta.luid, ToEngineVec3(force));
		}

		void IScript::RB_AddForce(float x, float y, float z, Entity entity) {
			RB_AddForce(Vec3(x, y, z), entity);
		}

		void IScript::RB_AddImpulse(const Vec3& impulse, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			// Get entity LUID (needed for PhysicsManager which operates on entity LUIDs)
			if (!m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(targetEntity)) {
				return;
			}

			auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(targetEntity);
			Physics::PhysicsManager::GetInstance().AddForce(meta.luid, ToEngineVec3(impulse), Physics::ForceMode::Impulse);
		}

		void IScript::RB_AddImpulse(float x, float y, float z, Entity entity) {
			RB_AddImpulse(Vec3(x, y, z), entity);
		}

		void IScript::CC_Move(const Vec3& displacement, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(targetEntity);
			Physics::PhysicsManager::GetInstance().CharacterMove(meta.luid, ToEngineVec3(displacement));
		}

		void IScript::CC_Rotate(float yawDegrees, Entity entity) {
			CHECK_CONTEXT_OR_RETURN();

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(targetEntity);
			Physics::PhysicsManager::GetInstance().CharacterRotateYaw(meta.luid, yawDegrees);
		}

		bool IScript::CC_IsGrounded(Entity entity) const {
			CHECK_CONTEXT_OR_RETURN(false);

			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(targetEntity);
			return Physics::PhysicsManager::GetInstance().CharacterIsGrounded(meta.luid);
		}

		//=========================================================================
		// Physics Raycasting
		//=========================================================================

		RaycastHit IScript::Raycast(const Vec3& origin, const Vec3& direction, float maxDistance, uint32_t layerMask) const {
			RaycastHit sdkHit;
			sdkHit.hasHit = false;

			if (!m_context || !m_context->componentManager) {
				return sdkHit;
			}

			// Call PhysicsManager raycast with engine types
			Physics::RaycastHit engineHit;
			bool hasHit = Physics::PhysicsManager::GetInstance().Raycast(
				ToEngineVec3(origin),
				ToEngineVec3(direction),
				engineHit,
				maxDistance,
				layerMask
			);

			// Convert to SDK RaycastHit (scripts only see Entity, not LUIDs)
			sdkHit.hasHit = hasHit;
			sdkHit.point = ToSDKVec3(engineHit.point);
			sdkHit.normal = ToSDKVec3(engineHit.normal);
			sdkHit.distance = engineHit.distance;
			sdkHit.entity = engineHit.colliderEntityID;

			return sdkHit;
		}

		RaycastHit IScript::Raycast(float originX, float originY, float originZ,
			float dirX, float dirY, float dirZ,
			float maxDistance, uint32_t layerMask) const {
			return Raycast(Vec3(originX, originY, originZ),
				Vec3(dirX, dirY, dirZ),
				maxDistance,
				layerMask);
		}

		std::vector<RaycastHit> IScript::RaycastAll(const Vec3& origin, const Vec3& direction,
			float maxDistance, uint32_t layerMask) const {
			//std::vector<RaycastHit> results;

			//if (!m_context || !m_context->componentManager) {
			//	return results;
			//}

			//// Call PhysicsManager raycast with layer mask (static method)
			//auto hits = Physics::PhysicsManager::RaycastAll(
			//	ToEngineVec3(origin),
			//	ToEngineVec3(direction),
			//	maxDistance,
			//	layerMask
			//);

			//// Convert PhysicsManager::RaycastHit to SDK RaycastHit
			//results.reserve(hits.size());
			//for (const auto& hit : hits) {
			//	RaycastHit result;
			//	result.hasHit = hit.hasHit;
			//	result.point = ToSDKVec3(hit.point);
			//	result.normal = ToSDKVec3(hit.normal);
			//	result.distance = hit.distance;
			//	result.entity = hit.entity;
			//	results.push_back(result);
			//}

			//return results;
			return {};
		}

		//=========================================================================
		// Physics Sphere Casting
		//=========================================================================

		RaycastHit IScript::SphereCast(const Vec3& origin, float radius, const Vec3& direction, float maxDistance, uint32_t layerMask) const {
			RaycastHit sdkHit;
			sdkHit.hasHit = false;

			if (!m_context || !m_context->componentManager) {
				return sdkHit;
			}

			// Call PhysicsManager sphere cast with engine types
			Physics::RaycastHit engineHit;
			bool hasHit = Physics::PhysicsManager::GetInstance().SphereCast(
				ToEngineVec3(origin),
				radius,
				ToEngineVec3(direction),
				engineHit,
				maxDistance,
				layerMask
			);

			// Convert to SDK RaycastHit (scripts only see Entity, not LUIDs)
			sdkHit.hasHit = hasHit;
			sdkHit.point = ToSDKVec3(engineHit.point);
			sdkHit.normal = ToSDKVec3(engineHit.normal);
			sdkHit.distance = engineHit.distance;
			sdkHit.entity = engineHit.colliderEntityID;

			return sdkHit;
		}

		RaycastHit IScript::SphereCast(float originX, float originY, float originZ,
			float radius,
			float dirX, float dirY, float dirZ,
			float maxDistance, uint32_t layerMask) const {
			return SphereCast(Vec3(originX, originY, originZ),
				radius,
				Vec3(dirX, dirY, dirZ),
				maxDistance,
				layerMask);
		}

		//=========================================================================
		// Audio Source
		//=========================================================================

		bool IScript::HasAudioSource(Entity entity) const {
			if (!m_context || !m_context->componentManager) return false;
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			return m_context->componentManager->HasComponent<ECS::Component::AudioSource>(targetEntity);
		}

		void IScript::PlayAudio(Entity entity) {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return;
			auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);

			// If already playing and not paused, stop first
			if (audioSource.m_channel && audioSource.isPlaying && !audioSource.isPaused) {
				audioSource.m_channel->stop();
			}

			// Reset state - AudioSystem will handle actual playback
			audioSource.isPlaying = true;
			audioSource.isPaused = false;
			audioSource.m_hasPlayed = false; // Trigger playback in AudioSystem
		}

		void IScript::StopAudio(Entity entity) {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return;
			auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);

			if (audioSource.m_channel) {
				audioSource.m_channel->stop();
			}

			audioSource.isPlaying = false;
			audioSource.isPaused = false;
		}

		void IScript::PauseAudio(Entity entity) {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return;
			auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);

			if (audioSource.m_channel && audioSource.isPlaying) {
				audioSource.m_channel->setPaused(true);
				audioSource.isPaused = true;
			}
		}

		void IScript::ResumeAudio(Entity entity) {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return;
			auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);

			if (audioSource.m_channel && audioSource.isPaused) {
				audioSource.m_channel->setPaused(false);
				audioSource.isPaused = false;
			}
		}

		bool IScript::IsAudioPlaying(Entity entity) const {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return false;
			const auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
			return audioSource.isPlaying && !audioSource.isPaused;
		}

		float IScript::GetVolume(Entity entity) const {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return 0.0f;
			auto& audio = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
			return audio.volume;
		}

		void IScript::SetVolume(float volume, Entity entity) {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return;
			auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
			audioSource.volume = volume;

			// Apply immediately if playing
			if (audioSource.m_channel) {
				audioSource.m_channel->setVolume(volume);
			}
		}

		float IScript::GetPitch(Entity entity) const {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return 1.0f;
			auto& audio = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
			return audio.pitch;
		}

		void IScript::SetPitch(float pitch, Entity entity) {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return;
			auto& audioSource = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
			audioSource.pitch = pitch;

			// Apply immediately if playing
			if (audioSource.m_channel) {
				audioSource.m_channel->setPitch(pitch);
			}
		}

		void IScript::SetAudioLoop(bool loop, Entity entity) {
			Entity targetEntity = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;
			if (!HasAudioSource(targetEntity)) return;
			auto& audio = m_context->componentManager->GetComponent<ECS::Component::AudioSource>(targetEntity);
			audio.loop = loop;
		}

		//=========================================================================
		// CAMERA OPERATIONS
		//=========================================================================

		bool IScript::HasCamera() const {
			if (!m_context || !m_context->componentManager) return false;
			return m_context->componentManager->HasComponent<ECS::Component::Camera>(m_entity);
		}

		float IScript::GetCameraFOV() const {
			if (!HasCamera()) return 45.0f;
			const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			return camera.fovY;
		}

		void IScript::SetCameraFOV(float fov) {
			if (!HasCamera()) return;
			auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			camera.fovY = fov;
			camera.isDirty = true; // Mark camera projection as needing rebuild
		}

		float IScript::GetCameraAspectRatio() const {
			if (!HasCamera()) return 16.0f / 9.0f;
			const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			return camera.aspectRatio;
		}

		void IScript::SetCameraAspectRatio(float aspectRatio) {
			if (!HasCamera()) return;
			auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			camera.aspectRatio = aspectRatio;
			camera.isDirty = true;
		}

		float IScript::GetCameraNearPlane() const {
			if (!HasCamera()) return 0.1f;
			const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			return camera.nearPlane;
		}

		void IScript::SetCameraNearPlane(float nearPlane) {
			if (!HasCamera()) return;
			auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			camera.nearPlane = nearPlane;
			camera.isDirty = true;
		}

		float IScript::GetCameraFarPlane() const {
			if (!HasCamera()) return 1000.0f;
			const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			return camera.farPlane;
		}

		void IScript::SetCameraFarPlane(float farPlane) {
			if (!HasCamera()) return;
			auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			camera.farPlane = farPlane;
			camera.isDirty = true;
		}

		bool IScript::IsCameraMain() const {
			if (!HasCamera()) return false;
			const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			return camera.isMain;
		}

		void IScript::SetCameraMain(bool isMain) {
			if (!HasCamera()) return;
			auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			camera.isMain = isMain;
		}

		bool IScript::IsCameraActive() const {
			if (!HasCamera()) return false;
			const auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			return camera.isActive;
		}

		void IScript::SetCameraActive(bool isActive) {
			if (!HasCamera()) return;
			auto& camera = m_context->componentManager->GetComponent<ECS::Component::Camera>(m_entity);
			camera.isActive = isActive;
		}

		//=========================================================================
		// Component References
		//=========================================================================

		TransformRef IScript::GetTransformRef(Entity entity) const {
			if (!m_context || !m_context->componentManager) return TransformRef();

			if (m_context->componentManager->HasComponent<ECS::Component::Transform>(entity)) {
				// Get component and extract LUID
				auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(entity);
				return TransformRef(entity, transform.luid);  // Pass both entity and LUID
			}
			return TransformRef();
		}

		RigidbodyRef IScript::GetRigidbodyRef(Entity entity) const {
			if (!m_context || !m_context->componentManager) return RigidbodyRef();

			if (m_context->componentManager->HasComponent<ECS::Component::Rigidbody>(entity)) {
				// Get component and extract LUID
				auto& rigidbody = m_context->componentManager->GetComponent<ECS::Component::Rigidbody>(entity);
				return RigidbodyRef(entity, rigidbody.luid);  // Pass both entity and LUID
			}
			return RigidbodyRef();
		}

		RendererRef IScript::GetRendererRef(Entity entity) const {
			if (!m_context || !m_context->componentManager) return RendererRef();

			if (m_context->componentManager->HasComponent<ECS::Component::Renderer>(entity)) {
				// Get component and extract LUID
				auto& renderer = m_context->componentManager->GetComponent<ECS::Component::Renderer>(entity);
				return RendererRef(entity, renderer.luid);  // Pass both entity and LUID
			}
			return RendererRef();
		}

		AudioSourceRef IScript::GetAudioSourceRef(Entity entity) const {
			if (!m_context || !m_context->componentManager) return AudioSourceRef();

			if (m_context->componentManager->HasComponent<ECS::Component::AudioSource>(entity)) {
				return AudioSourceRef(entity);
			}
			return AudioSourceRef();
		}

		//=========================================================================
		// Material UUID Registry (Maps material IDs to UUIDs)
		//=========================================================================

		namespace {
			struct MaterialRegistry {
				std::unordered_map<uint32_t, std::string> idToUUID;
				std::unordered_map<std::string, uint32_t> uuidToID;
				uint32_t nextID = 1; // Start from 1, 0 is reserved for invalid

				uint32_t GetOrCreateID(const std::string& uuid) {
					if (uuid.empty()) return 0;

					auto it = uuidToID.find(uuid);
					if (it != uuidToID.end()) {
						return it->second;
					}

					// Create new ID
					uint32_t id = nextID++;
					idToUUID[id] = uuid;
					uuidToID[uuid] = id;
					return id;
				}

				std::string GetUUID(uint32_t id) const {
					if (id == 0) return "";
					auto it = idToUUID.find(id);
					return (it != idToUUID.end()) ? it->second : "";
				}
			};

			MaterialRegistry& GetMaterialRegistry() {
				static MaterialRegistry registry;
				return registry;
			}
		}

		MaterialRef IScript::GetMaterialRef(const std::string& materialUUID) const {
			if (materialUUID.empty() || materialUUID == "empty uuid") return MaterialRef();

			// Get or create an ID for this material UUID
			uint32_t materialID = GetMaterialRegistry().GetOrCreateID(materialUUID);
			return MaterialRef(materialID);
		}

		MaterialRef IScript::GetEntityMaterial(Entity entity) const {
			// Get the material UUID from the entity's renderer component
			std::string materialUUID = NE::Renderer::Query::GetMaterial(entity);

			// Check if it's the empty UUID (no material assigned)
			if (materialUUID.empty() || materialUUID == "empty uuid") {
				return MaterialRef();
			}

			// Convert UUID to MaterialRef
			return GetMaterialRef(materialUUID);
		}

		//=========================================================================
		// Global helper function for material UUID conversion (exported for SDK)
		//=========================================================================

		/// Get material UUID from MaterialRef (accessible from other modules)
		SCRIPT_API std::string GetMaterialUUIDFromRef(const MaterialRef& materialRef) {
			if (!materialRef.IsValid()) return "";
			return GetMaterialRegistry().GetUUID(materialRef.GetEntity());
		}

		//=========================================================================
		// Prefab UUID Registry (Maps prefab IDs to UUIDs)
		//=========================================================================

		namespace {
			struct PrefabRegistry {
				std::unordered_map<uint32_t, std::string> idToPath;
				std::unordered_map<std::string, uint32_t> PathToID;
				uint32_t nextID = 1; // Start from 1, 0 is reserved for invalid

				uint32_t GetOrCreateID(const std::string& path) {
					if (path.empty()) return 0;

					auto it = PathToID.find(path);
					if (it != PathToID.end()) {
						return it->second;
					}

					// Create new ID
					uint32_t id = nextID++;
					idToPath[id] = path;
					PathToID[path] = id;
					return id;
				}

				std::string GetPath(uint32_t id) const {
					if (id == 0) return "";
					auto it = idToPath.find(id);
					return (it != idToPath.end()) ? it->second : "";
				}
			};

			PrefabRegistry& GetPrefabRegistry() {
				static PrefabRegistry registry;
				return registry;
			}
		}

		PrefabRef IScript::GetPrefabRef(const std::string& prefabPath) const {
			if (prefabPath.empty()) return PrefabRef();

			// Get or create an ID for this prefab Path
			uint32_t prefabID = GetPrefabRegistry().GetOrCreateID(prefabPath);
			return PrefabRef(prefabID);
		}

		/// Get prefab Path from PrefabRef (accessible from other modules)
		SCRIPT_API std::string GetPrefabPathFromRef(const PrefabRef& prefabRef) {
			if (!prefabRef.IsValid()) return "";
			return GetPrefabRegistry().GetPath(prefabRef.GetEntity());
		}

		//=========================================================================
		// Prefab Instantiation
		//=========================================================================

		Entity IScript::InstantiatePrefab(const PrefabRef& prefabRef, const Vec3& position, const Vec3& rotation) {
			if (!prefabRef.IsValid()) {
				SPD_ERROR("[PrefabRef] Cannot instantiate: Invalid prefab reference");
				return INVALID_ENTITY;
			}

			std::string prefabPath = GetPrefabRegistry().GetPath(prefabRef.GetEntity());
			return InstantiatePrefab(prefabPath, position, rotation);
		}

		Entity IScript::InstantiatePrefab(const std::string& prefabPath, const Vec3& position, const Vec3& rotation) {
			if (prefabPath.empty()) {
				SPD_ERROR("[PrefabRef] Cannot instantiate: Empty prefab path");
				return INVALID_ENTITY;
			}

			if (!m_context || !m_context->componentManager) {
				SPD_ERROR("[PrefabRef] Cannot instantiate: Invalid script context");
				return INVALID_ENTITY;
			}

			try {
				// Check if this is a UUID or a file path
				std::vector<uint32_t> newEntities;
				//newEntities = DeserializePrefab(prefabPath);

				if (newEntities.empty()) {
					SPD_ERROR("[PrefabRef] Failed to instantiate prefab: " << prefabPath);
					return INVALID_ENTITY;
				}

				// The root entity is the first entity in the list
				Entity rootEntity = newEntities[0];
				if (m_context->componentManager->HasComponent<ECS::Component::Transform>(rootEntity)) {
					auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(rootEntity);
					transform.localPosition = ToEngineVec3(position);
					transform.localRotationEuler = ToEngineVec3(rotation);
					transform.isDirty = true;
				}

				SPD_INFO("[PrefabRef] Successfully instantiated prefab {} at position ({}, {}, {})",
					prefabPath, position.x, position.y, position.z);

				return rootEntity;
			} catch (const std::exception& e) {
				SPD_ERROR("[PrefabRef] Exception during instantiation: {}", e.what());
				return INVALID_ENTITY;
			} catch (...) {
				SPD_ERROR("[PrefabRef] Unknown exception during prefab instantiation");
				return INVALID_ENTITY;
			}
		}

		// Component ref operations (for stored references)
		Vec3 IScript::GetPosition(const TransformRef& ref) const {
			if (!ref.IsValid() || !m_context) return Vec3::Zero();

			// Prefer LUID resolution over Entity lookup
			if (ref.GetLuid() != 0 && m_context->luidRegistry) {
				auto* transform = ResolveComponentByLuid<ECS::Component::Transform>(
					ref.GetLuid(), m_context->luidRegistry);
				if (transform) return ToSDKVec3(transform->localPosition);
			}

			// Fallback to Entity-based lookup
			//if (m_context->componentManager) {
			//	auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
			//	return ToSDKVec3(transform.localPosition);
			//}

			return Vec3::Zero();
		}

		void IScript::SetPosition(const TransformRef& ref, const Vec3& pos) {
			if (!ref.IsValid() || !m_context) return;

			// Prefer LUID resolution over Entity lookup
			if (ref.GetLuid() != 0 && m_context->luidRegistry) {
				auto* transform = ResolveComponentByLuid<ECS::Component::Transform>(
					ref.GetLuid(), m_context->luidRegistry);
				if (transform) {
					transform->localPosition = ToEngineVec3(pos);
					transform->isDirty = true;
					return;
				}
			}

			// Fallback to Entity-based lookup
			//if (m_context->componentManager) {
			//	auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
			//	transform.localPosition = ToEngineVec3(pos);
			//	transform.isDirty = true;
			//}
		}

		void IScript::SetPosition(const TransformRef& ref, float x, float y, float z) {
			SetPosition(ref, Vec3(x, y, z));
		}

		Vec3 IScript::GetRotation(const TransformRef& ref) const {
			if (!ref.IsValid() || !m_context) return Vec3::Zero();

			// Prefer LUID resolution 
			if (ref.GetLuid() != 0 && m_context->luidRegistry) {
				auto* transform = ResolveComponentByLuid<ECS::Component::Transform>(
					ref.GetLuid(), m_context->luidRegistry);
				if (transform) return ToSDKVec3(transform->localRotationEuler);
			}

			// Fallback to Entity-based lookup
			//if (m_context->componentManager) {
			//	auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
			//	return ToSDKVec3(transform.localRotationEuler);
			//}

			return Vec3::Zero();
		}

		void IScript::SetRotation(const TransformRef& ref, const Vec3& rot) {
			if (!ref.IsValid() || !m_context) return;

			// Prefer LUID resolution over Entity lookup
			if (ref.GetLuid() != 0 && m_context->luidRegistry) {
				auto* transform = ResolveComponentByLuid<ECS::Component::Transform>(
					ref.GetLuid(), m_context->luidRegistry);
				if (transform) {
					transform->localRotationEuler = ToEngineVec3(rot);
					transform->isDirty = true;
					return;
				}
			}

			// Fallback to Entity-based lookup
			//if (m_context->componentManager) {
			//	auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
			//	transform.localRotationEuler = ToEngineVec3(rot);
			//	transform.isDirty = true;
			//}
		}

		Vec3 IScript::GetScale(const TransformRef& ref) const {
			if (!ref.IsValid() || !m_context) return Vec3::One();

			// Prefer LUID resolution over Entity lookup
			if (ref.GetLuid() != 0 && m_context->luidRegistry) {
				auto* transform = ResolveComponentByLuid<ECS::Component::Transform>(
					ref.GetLuid(), m_context->luidRegistry);
				if (transform) return ToSDKVec3(transform->localScale);
			}

			//// Fallback to Entity-based lookup
			//if (m_context->componentManager) {
			//	auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
			//	return ToSDKVec3(transform.localScale);
			//}

			return Vec3::One();
		}

		void IScript::SetScale(const TransformRef& ref, const Vec3& scale) {
			if (!ref.IsValid() || !m_context) return;

			// Prefer LUID resolution over Entity lookup
			if (ref.GetLuid() != 0 && m_context->luidRegistry) {
				auto* transform = ResolveComponentByLuid<ECS::Component::Transform>(
					ref.GetLuid(), m_context->luidRegistry);
				if (transform) {
					transform->localScale = ToEngineVec3(scale);
					transform->isDirty = true;
					return;
				}
			}

			//// Fallback to Entity-based lookup
			//if (m_context->componentManager) {
			//	auto& transform = m_context->componentManager->GetComponent<ECS::Component::Transform>(ref.GetEntity());
			//	transform.localScale = ToEngineVec3(scale);
			//	transform.isDirty = true;
			//}
		}

		Vec3 IScript::GetVelocity(const RigidbodyRef& ref) const {
			if (!ref.IsValid() || !m_context) return Vec3::Zero();

			// Get entity LUID (needed for PhysicsManager which operates on entity LUIDs)
			uint64_t entityLUID = 0;

			// Prefer LUID resolution to get entity
			if (ref.GetLuid() != 0 && m_context->luidRegistry) {
				auto* record = m_context->luidRegistry->Find(ref.GetLuid());
				if (record) {
					Entity entity = static_cast<Entity>(record->m_entityOwner);
					if (m_context->componentManager && m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
						auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(entity);
						entityLUID = meta.luid;
					}
				}
			}

			// Fallback to Entity-based lookup
			if (entityLUID == 0 && m_context->componentManager) {
				Entity entity = ref.GetEntity();
				if (m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
					auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(entity);
					entityLUID = meta.luid;
				}
			}

			if (entityLUID != 0) {
				return ToSDKVec3(Physics::PhysicsManager::GetInstance().GetLinearVelocity(entityLUID));
			}

			return Vec3::Zero();
		}

		void IScript::SetVelocity(const RigidbodyRef& ref, const Vec3& velocity) {
			if (!ref.IsValid() || !m_context) return;

			// Get entity LUID (needed for PhysicsManager which operates on entity LUIDs)
			uint64_t entityLUID = 0;

			// Prefer LUID resolution to get entity
			if (ref.GetLuid() != 0 && m_context->luidRegistry) {
				auto* record = m_context->luidRegistry->Find(ref.GetLuid());
				if (record) {
					Entity entity = static_cast<Entity>(record->m_entityOwner);
					if (m_context->componentManager && m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
						auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(entity);
						entityLUID = meta.luid;
					}
				}
			}

			// Fallback to Entity-based lookup
			if (entityLUID == 0 && m_context->componentManager) {
				Entity entity = ref.GetEntity();
				if (m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
					auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(entity);
					entityLUID = meta.luid;
				}
			}

			if (entityLUID != 0) {
				Physics::PhysicsManager::GetInstance().SetLinearVelocity(entityLUID, ToEngineVec3(velocity));
			}
		}

		void IScript::AddForce(const RigidbodyRef& ref, const Vec3& force) {
			if (!ref.IsValid() || !m_context) return;

			// Get entity LUID (needed for PhysicsManager which operates on entity LUIDs)
			uint64_t entityLUID = 0;

			// Prefer LUID resolution to get entity
			if (ref.GetLuid() != 0 && m_context->luidRegistry) {
				auto* record = m_context->luidRegistry->Find(ref.GetLuid());
				if (record) {
					Entity entity = static_cast<Entity>(record->m_entityOwner);
					if (m_context->componentManager && m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
						auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(entity);
						entityLUID = meta.luid;
					}
				}
			}

			// Fallback to Entity-based lookup
			if (entityLUID == 0 && m_context->componentManager) {
				Entity entity = ref.GetEntity();
				if (m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
					auto& meta = m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(entity);
					entityLUID = meta.luid;
				}
			}

			if (entityLUID != 0) {
				Physics::PhysicsManager::GetInstance().AddForce(entityLUID, ToEngineVec3(force));
			}
		}

		//=========================================================================
		// Renderer ComponentRef Operations
		//=========================================================================

		MaterialRef IScript::GetMaterialRef(const RendererRef& ref) const {
			if (!ref.IsValid() || !m_context) return MaterialRef();

			Entity entity = ref.GetEntity();
			return GetEntityMaterial(entity);
		}

		void IScript::SetMaterialRef(const RendererRef& ref, const MaterialRef& materialRef) {
			if (!ref.IsValid() || !m_context) return;

			Entity entity = ref.GetEntity();

			// Get material UUID from MaterialRef
			std::string materialUUID = GetMaterialUUIDFromRef(materialRef);
			if (materialUUID.empty()) {
				materialUUID = "empty uuid";
			}

			// Set the material UUID on the entity's renderer component
			if (m_context->componentManager->HasComponent<ECS::Component::Renderer>(entity)) {
				auto& renderer = m_context->componentManager->GetComponent<ECS::Component::Renderer>(entity);
				renderer.materialUUID = materialUUID;
				renderer.isDirty = true;
			}
		}

		//=========================================================================
		// Field Registration
		//=========================================================================

		// Helper function to reduce code duplication in field registration
		void IScript::RegisterFieldInternal(
			const std::string& name,
			const std::string& typeToken,
			void* memberPtr,
			std::function<std::string()> getValue,
			std::function<bool(const std::string&)> setValue)
		{
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			FieldRegistry::FieldEntry entry;
			entry.typeToken = typeToken;
			entry.memberPtr = memberPtr;
			entry.getValue = std::move(getValue);
			entry.setValue = std::move(setValue);
			m_fieldRegistry->fields[name] = std::move(entry);
		}

		// Helper to mark a field as containing entity references (needs LUID conversion)
		void IScript::MarkFieldAsEntityReference(const std::string& name) {
			if (!m_context || !m_context->componentManager) return;

			// Access the NativeScript component and mark this field as an entity reference
			if (m_context->componentManager->HasComponent<NE::ECS::Component::NativeScript>(m_entity)) {
				auto& scriptComp = m_context->componentManager->GetComponent<NE::ECS::Component::NativeScript>(m_entity);
				scriptComp.EntityReferenceFields.insert(name);
			}
		}

		void IScript::RegisterFloatField(const std::string& name, float* memberPtr) {
			RegisterFieldInternal(
				name,
				"float",
				memberPtr,
				[memberPtr]() -> std::string { return std::to_string(*memberPtr); },
				[memberPtr](const std::string& value) -> bool {
					try {
						*memberPtr = std::stof(value);
						return true;
					} catch (...) {
						return false;
					}
				}
			);
		}

		void IScript::RegisterIntField(const std::string& name, int* memberPtr) {
			RegisterFieldInternal(
				name,
				"int",
				memberPtr,
				[memberPtr]() -> std::string { return std::to_string(*memberPtr); },
				[memberPtr](const std::string& value) -> bool {
					try {
						*memberPtr = std::stoi(value);
						return true;
					} catch (...) {
						return false;
					}
				}
			);
		}

		void IScript::RegisterBoolField(const std::string& name, bool* memberPtr) {
			RegisterFieldInternal(
				name,
				"bool",
				memberPtr,
				[memberPtr]() -> std::string { return *memberPtr ? "1" : "0"; },
				[memberPtr](const std::string& value) -> bool {
					if (value == "1" || value == "true") {
						*memberPtr = true;
						return true;
					}
					if (value == "0" || value == "false") {
						*memberPtr = false;
						return true;
					}
					return false;
				}
			);
		}

		void IScript::RegisterStringField(const std::string& name, std::string* memberPtr) {
			RegisterFieldInternal(
				name,
				"string",
				memberPtr,
				[memberPtr]() -> std::string { return *memberPtr; },
				[memberPtr](const std::string& value) -> bool {
					try {
						*memberPtr = value;
						return true;
					} catch (...) {
						return false;
					}
				}
			);
		}

		void IScript::RegisterVec3Field(const std::string& name, Vec3* memberPtr) {
			RegisterFieldInternal(
				name,
				"vec3",
				memberPtr,
				[memberPtr]() -> std::string {
					std::ostringstream oss;
					oss << memberPtr->x << ' ' << memberPtr->y << ' ' << memberPtr->z;
					return oss.str();
				},
				[memberPtr](const std::string& value) -> bool {
					try {
						std::istringstream iss(value);
						float x, y, z;
						if (!(iss >> x >> y >> z)) {
							return false;
						}
						memberPtr->x = x;
						memberPtr->y = y;
						memberPtr->z = z;
						return true;
					} catch (...) {
						return false;
					}
				}
			);
		}

		void IScript::RegisterTransformRefField(const std::string& name, TransformRef* memberPtr) {
			RegisterFieldInternal(
				name,
				"transformref",
				memberPtr,
				// Serialize: Store component LUID
				[memberPtr]() -> std::string {
					uint64_t luid = memberPtr->GetLuid();
					// Fallback to Entity-based if LUID is not set (backwards compatibility)
					if (luid == 0) {
						return std::to_string(memberPtr->GetEntity());
					}
					return std::to_string(luid);
				},
				// Deserialize: Restore from component LUID
				[this, memberPtr](const std::string& value) -> bool {
					try {
						uint64_t luid = std::stoull(value);

						// If we have LUID registry, resolve LUID to Entity
						if (m_context && m_context->luidRegistry && luid != 0) {
							Entity entity = GetEntityFromComponentLuid(luid, m_context->luidRegistry);
							if (entity != INVALID_ENTITY) {
								*memberPtr = GetTransformRef(entity);  // This will populate both entity and LUID
								return true;
							}
						}

						// Fallback: treat value as Entity ID (backwards compatibility)
						Entity entity = static_cast<Entity>(luid);
						*memberPtr = GetTransformRef(entity);
						return true;
					} catch (...) {
						return false;
					}
				}
			);
			// NOTE: ComponentRef fields store component LUIDs (64-bit), not entity IDs (32-bit)
			// They should NOT be marked as entity references to avoid truncation during serialization
		}

		void IScript::RegisterRigidbodyRefField(const std::string& name, RigidbodyRef* memberPtr) {
			RegisterFieldInternal(
				name,
				"rigidbodyref",
				memberPtr,
				// Serialize: Store component LUID
				[memberPtr]() -> std::string {
					uint64_t luid = memberPtr->GetLuid();
					// Fallback to Entity-based if LUID is not set (backwards compatibility)
					if (luid == 0) {
						return std::to_string(memberPtr->GetEntity());
					}
					return std::to_string(luid);
				},
				// Deserialize: Restore from component LUID
				[this, memberPtr](const std::string& value) -> bool {
					try {
						uint64_t luid = std::stoull(value);

						// If we have LUID registry, resolve LUID to Entity
						if (m_context && m_context->luidRegistry && luid != 0) {
							Entity entity = GetEntityFromComponentLuid(luid, m_context->luidRegistry);
							if (entity != INVALID_ENTITY) {
								*memberPtr = GetRigidbodyRef(entity);  // This will populate both entity and LUID
								return true;
							}
						}

						// Fallback: treat value as Entity ID (backwards compatibility)
						Entity entity = static_cast<Entity>(luid);
						*memberPtr = GetRigidbodyRef(entity);
						return true;
					} catch (...) {
						return false;
					}
				}
			);
			// NOTE: ComponentRef fields store component LUIDs (64-bit), not entity IDs (32-bit)
			// They should NOT be marked as entity references to avoid truncation during serialization
		}

		void IScript::RegisterRendererRefField(const std::string& name, RendererRef* memberPtr) {
			RegisterFieldInternal(
				name,
				"rendererref",
				memberPtr,
				// Serialize: Store component LUID
				[memberPtr]() -> std::string {
					uint64_t luid = memberPtr->GetLuid();
					// Fallback to Entity-based if LUID is not set (backwards compatibility)
					if (luid == 0) {
						return std::to_string(memberPtr->GetEntity());
					}
					return std::to_string(luid);
				},
				// Deserialize: Restore from component LUID
				[this, memberPtr](const std::string& value) -> bool {
					try {
						uint64_t luid = std::stoull(value);

						// If we have LUID registry, resolve LUID to Entity
						if (m_context && m_context->luidRegistry && luid != 0) {
							Entity entity = GetEntityFromComponentLuid(luid, m_context->luidRegistry);
							if (entity != INVALID_ENTITY) {
								*memberPtr = GetRendererRef(entity);  // This will populate both entity and LUID
								return true;
							}
						}

						// Fallback: treat value as Entity ID (backwards compatibility)
						Entity entity = static_cast<Entity>(luid);
						*memberPtr = GetRendererRef(entity);
						return true;
					} catch (...) {
						return false;
					}
				}
			);
			// NOTE: ComponentRef fields store component LUIDs (64-bit), not entity IDs (32-bit)
			// They should NOT be marked as entity references to avoid truncation during serialization
		}

		void IScript::RegisterAudioSourceRefField(const std::string& name, AudioSourceRef* memberPtr) {
			RegisterFieldInternal(
				name,
				"audiosourceref",
				memberPtr,
				[memberPtr]() -> std::string { return std::to_string(memberPtr->GetEntity()); },
				[this, memberPtr](const std::string& value) -> bool {
					try {
						Entity entity = static_cast<Entity>(std::stoul(value));
						*memberPtr = GetAudioSourceRef(entity);
						return true;
					} catch (...) {
						return false;
					}
				}
			);
			// TODO: AudioSourceRef should be updated to use LUID-based references like Transform/Rigidbody/Renderer
			MarkFieldAsEntityReference(name);  // Track for LUID conversion during scene serialization
		}

		void IScript::RegisterMaterialRefField(const std::string& name, MaterialRef* memberPtr) {
			RegisterFieldInternal(
				name,
				"materialref",
				memberPtr,
				[memberPtr]() -> std::string {
					// For MaterialRef, the ownerEntity field stores a material ID
					// Use the material registry to convert ID back to UUID
					if (!memberPtr->IsValid()) {
						return "";
					}
					return GetMaterialRegistry().GetUUID(memberPtr->GetEntity());
				},
				[this, memberPtr, name](const std::string& value) -> bool {
					try {
						SPD_DEBUG("[MaterialRef] Setting field " << name);

						// Empty string means no material
						if (value.empty()) {
							*memberPtr = MaterialRef();
							return true;
						}

						// Create MaterialRef from UUID
						MaterialRef newRef = GetMaterialRef(value);
						if (!newRef.IsValid()) {
							SPD_ERROR("[MaterialRef] Failed to create valid MaterialRef from UUID: {}", value);
							return false;
						}

						*memberPtr = newRef;
						SPD_DEBUG("[MaterialRef] Successfully assigned material to field '{}'", name);
						return true;
					} catch (const std::exception& e) {
						SPD_ERROR("[MaterialRef] setValue exception for field '{}': {}", name, e.what());
						return false;
					} catch (...) {
						SPD_ERROR("[MaterialRef] setValue unknown exception for field '{}'", name);
						return false;
					}
				}
			);
		}

		void IScript::RegisterPrefabRefField(const std::string& name, PrefabRef* memberPtr) {
			RegisterFieldInternal(
				name,
				"prefabref",
				memberPtr,
				[memberPtr]() -> std::string {
					// For PrefabRef, the ownerEntity field stores a prefab ID
					// Use the prefab registry to convert ID back to UUID
					if (!memberPtr->IsValid()) {
						return "";
					}
					return GetPrefabRegistry().GetPath(memberPtr->GetEntity());
				},
				[this, memberPtr, name](const std::string& value) -> bool {
					try {
						SPD_DEBUG("[PrefabRef] Setting field '{}' to '{}'", name, value.empty() ? "<empty>" : value);

						// Empty string means no prefab
						if (value.empty()) {
							*memberPtr = PrefabRef();
							return true;
						}

						// Create PrefabRef from Path
						PrefabRef newRef = GetPrefabRef(value);
						if (!newRef.IsValid()) {
							SPD_ERROR("[PrefabRef] Failed to create valid PrefabRef from Path: " << value);
							return false;
						}

						*memberPtr = newRef;
						SPD_DEBUG("[PrefabRef] Successfully assigned prefab to field " << name);
						return true;
					} catch (const std::exception& e) {
						SPD_ERROR("[PrefabRef] setValue exception for field " << name << ": " << e.what());
						return false;
					} catch (...) {
						SPD_ERROR("[PrefabRef] setValue unknown exception for field " << name);
						return false;
					}
				}
			);
		}

		void IScript::RegisterGameObjectRefField(const std::string& name, GameObjectRef* memberPtr) {
			RegisterFieldInternal(
				name,
				"gameobjectref",
				memberPtr,
				[memberPtr]() -> std::string {
					// For GameObjectRef, store the entity ID
					if (!memberPtr->IsValid()) {
						return std::to_string(INVALID_ENTITY);
					}
					return std::to_string(memberPtr->GetEntity());
				},
				[this, memberPtr, name](const std::string& value) -> bool {
					try {
						// Parse entity ID from string
						Entity entityId = INVALID_ENTITY;
						if (!value.empty()) {
							try {
								entityId = static_cast<Entity>(std::stoul(value));
							} catch (...) {
								entityId = INVALID_ENTITY;
							}
						}

						memberPtr->SetEntity(entityId);

						// Track as an entity reference for serialization
						MarkFieldAsEntityReference(name);

						return true;
					} catch (const std::exception& e) {
						SPD_ERROR("[GameObjectRef] setValue exception for field " << name << ": " << e.what());
						return false;
					}
				}
			);
		}

		void IScript::RegisterLayerRefField(const std::string& name, LayerRef* memberPtr) {
			RegisterFieldInternal(
				name,
				"layerref",
				memberPtr,
				[memberPtr]() -> std::string {
					// Store the layer ID as a string
					return std::to_string(static_cast<int>(memberPtr->GetID()));
				},
				[memberPtr, name](const std::string& value) -> bool {
					try {
						// Parse layer ID from string
						uint8_t layerId = 0;
						if (!value.empty()) {
							try {
								int parsed = std::stoi(value);
								// Clamp to valid layer range (0-31)
								if (parsed < 0) parsed = 0;
								if (parsed > 31) parsed = 31;
								layerId = static_cast<uint8_t>(parsed);
							} catch (...) {
								layerId = 0;
							}
						}

						memberPtr->SetID(layerId);
						return true;
					} catch (const std::exception& e) {
						SPD_ERROR("[LayerRef] setValue exception for field " << name << ": " << e.what());
						return false;
					}
				}
			);
		}

		void IScript::RegisterMaterialRefVectorField(const std::string& name, std::vector<MaterialRef>* memberPtr) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			FieldRegistry::FieldEntry entry;
			entry.typeToken = "vector<materialref>";
			entry.memberPtr = memberPtr;

			// getValue: Serialize entire vector as "size uuid1 uuid2 ..."
			entry.getValue = [this, memberPtr]() -> std::string {
				std::ostringstream oss;
				oss << memberPtr->size();
				for (const auto& ref : *memberPtr) {
					oss << " ";
					if (ref.IsValid()) {
						oss << GetMaterialRegistry().GetUUID(ref.GetEntity());
					}
				}
				return oss.str();
				};

			// setValue: Deserialize entire vector from "size uuid1 uuid2 ..."
			entry.setValue = [this, memberPtr](const std::string& value) -> bool {
				try {
					std::istringstream iss(value);
					size_t size;
					iss >> size;

					memberPtr->clear();
					memberPtr->reserve(size);

					for (size_t i = 0; i < size; ++i) {
						std::string uuid;
						iss >> uuid;
						if (uuid.empty()) {
							memberPtr->push_back(MaterialRef());
						} else {
							memberPtr->push_back(GetMaterialRef(uuid));
						}
					}
					return true;
				} catch (...) {
					return false;
				}
				};

			// Array operations
			entry.getSize = [memberPtr]() -> size_t {
				return memberPtr->size();
				};

			entry.getElement = [this, memberPtr](size_t index) -> std::string {
				if (index >= memberPtr->size()) return "";
				const auto& materialRef = (*memberPtr)[index];
				if (!materialRef.IsValid()) return "";
				// Return UUID for display in editor
				return GetMaterialRegistry().GetUUID(materialRef.GetEntity());
				};

			entry.setElement = [this, memberPtr](size_t index, const std::string& value) -> bool {
				if (index >= memberPtr->size()) return false;

				if (value.empty()) {
					(*memberPtr)[index] = MaterialRef();
					return true;
				}

				// Convert UUID to MaterialRef
				MaterialRef newRef = GetMaterialRef(value);
				(*memberPtr)[index] = newRef;
				return newRef.IsValid();
				};

			entry.addElement = [memberPtr]() -> void {
				memberPtr->push_back(MaterialRef());
				};

			entry.removeElement = [memberPtr](size_t index) -> void {
				if (index < memberPtr->size()) {
					memberPtr->erase(memberPtr->begin() + index);
				}
				};

			m_fieldRegistry->fields[name] = std::move(entry);
		}

		void IScript::RegisterPrefabRefVectorField(const std::string& name, std::vector<PrefabRef>* memberPtr) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			FieldRegistry::FieldEntry entry;
			entry.typeToken = "vector<prefabref>";
			entry.memberPtr = memberPtr;

			// getValue: Serialize entire vector
			entry.getValue = [this, memberPtr]() -> std::string {
				std::ostringstream oss;
				oss << memberPtr->size();
				for (const auto& ref : *memberPtr) {
					oss << " ";
					if (ref.IsValid()) {
						oss << GetPrefabRegistry().GetPath(ref.GetEntity());
					}
				}
				return oss.str();
				};

			// setValue: Deserialize entire vector from "size uuid1 uuid2 ..."
			entry.setValue = [this, memberPtr](const std::string& value) -> bool {
				try {
					std::istringstream iss(value);
					size_t size;
					iss >> size;

					memberPtr->clear();
					memberPtr->reserve(size);

					for (size_t i = 0; i < size; ++i) {
						std::string path;
						iss >> path;
						if (path.empty()) {
							memberPtr->push_back(PrefabRef());
						} else {
							memberPtr->push_back(GetPrefabRef(path));
						}
					}
					return true;
				} catch (...) {
					return false;
				}
				};

			// Array operations
			entry.getSize = [memberPtr]() -> size_t {
				return memberPtr->size();
				};

			entry.getElement = [this, memberPtr](size_t index) -> std::string {
				if (index >= memberPtr->size()) return "";
				const auto& prefabRef = (*memberPtr)[index];
				if (!prefabRef.IsValid()) return "";
				// Return Path for display in editor
				return GetPrefabRegistry().GetPath(prefabRef.GetEntity());
				};

			entry.setElement = [this, memberPtr](size_t index, const std::string& value) -> bool {
				if (index >= memberPtr->size()) return false;

				if (value.empty()) {
					(*memberPtr)[index] = PrefabRef();
					return true;
				}

				// Convert Path to PrefabRef
				PrefabRef newRef = GetPrefabRef(value);
				(*memberPtr)[index] = newRef;
				return newRef.IsValid();
				};

			entry.addElement = [memberPtr]() -> void {
				memberPtr->push_back(PrefabRef());
				};

			entry.removeElement = [memberPtr](size_t index) -> void {
				if (index < memberPtr->size()) {
					memberPtr->erase(memberPtr->begin() + index);
				}
				};

			m_fieldRegistry->fields[name] = std::move(entry);
		}

		void IScript::RegisterIntVectorField(const std::string& name, std::vector<int>* memberPtr) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			FieldRegistry::FieldEntry entry;
			entry.typeToken = "vector<int>";
			entry.memberPtr = memberPtr;

			// getValue: Serialize entire vector as "size val1 val2 ..."
			entry.getValue = [memberPtr]() -> std::string {
				std::ostringstream oss;
				oss << memberPtr->size();
				for (const auto& val : *memberPtr) {
					oss << " " << val;
				}
				return oss.str();
				};

			// setValue: Deserialize entire vector
			entry.setValue = [memberPtr](const std::string& value) -> bool {
				try {
					std::istringstream iss(value);
					size_t size;
					iss >> size;

					memberPtr->clear();
					memberPtr->reserve(size);

					for (size_t i = 0; i < size; ++i) {
						int val;
						if (!(iss >> val)) return false;
						memberPtr->push_back(val);
					}
					return true;
				} catch (...) {
					return false;
				}
				};

			// Array operations
			entry.getSize = [memberPtr]() -> size_t {
				return memberPtr->size();
				};

			entry.getElement = [memberPtr](size_t index) -> std::string {
				if (index >= memberPtr->size()) return "";
				return std::to_string((*memberPtr)[index]);
				};

			entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
				if (index >= memberPtr->size()) return false;
				try {
					(*memberPtr)[index] = std::stoi(value);
					return true;
				} catch (...) {
					return false;
				}
				};

			entry.addElement = [memberPtr]() -> void {
				memberPtr->push_back(0);
				};

			entry.removeElement = [memberPtr](size_t index) -> void {
				if (index < memberPtr->size()) {
					memberPtr->erase(memberPtr->begin() + index);
				}
				};

			m_fieldRegistry->fields[name] = std::move(entry);
		}

		void IScript::RegisterFloatVectorField(const std::string& name, std::vector<float>* memberPtr) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			FieldRegistry::FieldEntry entry;
			entry.typeToken = "vector<float>";
			entry.memberPtr = memberPtr;

			// getValue: Serialize entire vector as "size val1 val2 ..."
			entry.getValue = [memberPtr]() -> std::string {
				std::ostringstream oss;
				oss << memberPtr->size();
				for (const auto& val : *memberPtr) {
					oss << " " << val;
				}
				return oss.str();
				};

			// setValue: Deserialize entire vector
			entry.setValue = [memberPtr](const std::string& value) -> bool {
				try {
					std::istringstream iss(value);
					size_t size;
					iss >> size;

					memberPtr->clear();
					memberPtr->reserve(size);

					for (size_t i = 0; i < size; ++i) {
						float val;
						if (!(iss >> val)) return false;
						memberPtr->push_back(val);
					}
					return true;
				} catch (...) {
					return false;
				}
				};

			// Array operations
			entry.getSize = [memberPtr]() -> size_t {
				return memberPtr->size();
				};

			entry.getElement = [memberPtr](size_t index) -> std::string {
				if (index >= memberPtr->size()) return "";
				return std::to_string((*memberPtr)[index]);
				};

			entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
				if (index >= memberPtr->size()) return false;
				try {
					(*memberPtr)[index] = std::stof(value);
					return true;
				} catch (...) {
					return false;
				}
				};

			entry.addElement = [memberPtr]() -> void {
				memberPtr->push_back(0.0f);
				};

			entry.removeElement = [memberPtr](size_t index) -> void {
				if (index < memberPtr->size()) {
					memberPtr->erase(memberPtr->begin() + index);
				}
				};

			m_fieldRegistry->fields[name] = std::move(entry);
		}

		void IScript::RegisterBoolVectorField(const std::string& name, std::vector<bool>* memberPtr) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			FieldRegistry::FieldEntry entry;
			entry.typeToken = "vector<bool>";
			entry.memberPtr = memberPtr;

			// getValue: Serialize entire vector as "size val1 val2 ..."
			entry.getValue = [memberPtr]() -> std::string {
				std::ostringstream oss;
				oss << memberPtr->size();
				for (size_t i = 0; i < memberPtr->size(); ++i) {
					oss << " " << ((*memberPtr)[i] ? "1" : "0");
				}
				return oss.str();
				};

			// setValue: Deserialize entire vector
			entry.setValue = [memberPtr](const std::string& value) -> bool {
				try {
					std::istringstream iss(value);
					size_t size;
					iss >> size;

					memberPtr->clear();
					memberPtr->reserve(size);

					for (size_t i = 0; i < size; ++i) {
						std::string val;
						if (!(iss >> val)) return false;
						memberPtr->push_back(val == "1" || val == "true");
					}
					return true;
				} catch (...) {
					return false;
				}
				};

			// Array operations
			entry.getSize = [memberPtr]() -> size_t {
				return memberPtr->size();
				};

			entry.getElement = [memberPtr](size_t index) -> std::string {
				if (index >= memberPtr->size()) return "";
				return (*memberPtr)[index] ? "1" : "0";
				};

			entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
				if (index >= memberPtr->size()) return false;
				(*memberPtr)[index] = (value == "1" || value == "true");
				return true;
				};

			entry.addElement = [memberPtr]() -> void {
				memberPtr->push_back(false);
				};

			entry.removeElement = [memberPtr](size_t index) -> void {
				if (index < memberPtr->size()) {
					memberPtr->erase(memberPtr->begin() + index);
				}
				};

			m_fieldRegistry->fields[name] = std::move(entry);
		}

		void IScript::RegisterStringVectorField(const std::string& name, std::vector<std::string>* memberPtr) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			FieldRegistry::FieldEntry entry;
			entry.typeToken = "vector<string>";
			entry.memberPtr = memberPtr;

			// getValue: Serialize entire vector as "size val1 val2 ..."
			// Strings are encoded with length prefix to handle spaces: "2 5:hello 5:world"
			entry.getValue = [memberPtr]() -> std::string {
				std::ostringstream oss;
				oss << memberPtr->size();
				for (const auto& str : *memberPtr) {
					oss << " " << str.length() << ":" << str;
				}
				return oss.str();
				};

			// setValue: Deserialize entire vector
			entry.setValue = [memberPtr](const std::string& value) -> bool {
				try {
					std::istringstream iss(value);
					size_t size;
					iss >> size;

					memberPtr->clear();
					memberPtr->reserve(size);

					for (size_t i = 0; i < size; ++i) {
						size_t len;
						char colon;
						if (!(iss >> len >> colon) || colon != ':') return false;

						// Read the exact number of characters (including spaces)
						iss.ignore(1); // Skip the space after colon
						std::string str(len, '\0');
						if (len > 0) {
							iss.read(&str[0], len);
							if (iss.gcount() != static_cast<std::streamsize>(len)) return false;
						}
						memberPtr->push_back(str);
					}
					return true;
				} catch (...) {
					return false;
				}
				};

			// Array operations
			entry.getSize = [memberPtr]() -> size_t {
				return memberPtr->size();
				};

			entry.getElement = [memberPtr](size_t index) -> std::string {
				if (index >= memberPtr->size()) return "";
				return (*memberPtr)[index];
				};

			entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
				if (index >= memberPtr->size()) return false;
				(*memberPtr)[index] = value;
				return true;
				};

			entry.addElement = [memberPtr]() -> void {
				memberPtr->push_back("");
				};

			entry.removeElement = [memberPtr](size_t index) -> void {
				if (index < memberPtr->size()) {
					memberPtr->erase(memberPtr->begin() + index);
				}
				};

			m_fieldRegistry->fields[name] = std::move(entry);
		}

		void IScript::RegisterEntityVectorField(const std::string& name, std::vector<Entity>* memberPtr) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			FieldRegistry::FieldEntry entry;
			entry.typeToken = "vector<entity>";
			entry.memberPtr = memberPtr;

			// getValue: Serialize entire vector as "size id1 id2 ..."
			entry.getValue = [memberPtr]() -> std::string {
				std::ostringstream oss;
				oss << memberPtr->size();
				for (const auto& entity : *memberPtr) {
					oss << " " << entity;
				}
				return oss.str();
				};

			// setValue: Deserialize entire vector from "size id1 id2 ..."
			entry.setValue = [memberPtr](const std::string& value) -> bool {
				try {
					std::istringstream iss(value);
					size_t size;
					iss >> size;

					memberPtr->clear();
					memberPtr->reserve(size);

					for (size_t i = 0; i < size; ++i) {
						uint32_t entityId;
						if (!(iss >> entityId)) return false;
						memberPtr->push_back(static_cast<Entity>(entityId));
					}
					return true;
				} catch (...) {
					return false;
				}
				};

			// Array operations
			entry.getSize = [memberPtr]() -> size_t {
				return memberPtr->size();
				};

			entry.getElement = [memberPtr](size_t index) -> std::string {
				if (index >= memberPtr->size()) return "";
				return std::to_string((*memberPtr)[index]);
				};

			entry.setElement = [memberPtr](size_t index, const std::string& value) -> bool {
				if (index >= memberPtr->size()) return false;
				try {
					(*memberPtr)[index] = static_cast<Entity>(std::stoul(value));
					return true;
				} catch (...) {
					return false;
				}
				};

			entry.addElement = [memberPtr]() -> void {
				memberPtr->push_back(NE::ECS::NO_ENTITY);
				};

			entry.removeElement = [memberPtr](size_t index) -> void {
				if (index < memberPtr->size()) {
					memberPtr->erase(memberPtr->begin() + index);
				}
				};

			m_fieldRegistry->fields[name] = std::move(entry);
			MarkFieldAsEntityReference(name);  // Track for LUID conversion during scene serialization
		}

		void IScript::RegisterLayerMaskField(const std::string& name, LayerMask* memberPtr) {
			RegisterFieldInternal(
				name,
				"layermask",
				memberPtr,
				// getValue: Return mask as string
				[memberPtr]() -> std::string {
					return std::to_string(memberPtr->mask);
				},
				// setValue: Set mask from string
				[memberPtr](const std::string& value) -> bool {
					try {
						memberPtr->mask = std::stoul(value);
						return true;
					} catch (...) {
						return false;
					}
				}
			);

			// Set LayerMask callbacks for editor access
			SetFieldLayerMaskCallbacks(name,
				[memberPtr]() -> uint32_t {
					return memberPtr->mask;
				},
				[memberPtr](uint32_t value) {
					memberPtr->mask = value;
				}
			);
		}

		//=========================================================================
		// Helper Methods for Template Functions
		//=========================================================================

		void IScript::SetFieldEnumOptions(const std::string& name, const std::vector<std::string>& options) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			auto it = m_fieldRegistry->fields.find(name);
			if (it != m_fieldRegistry->fields.end()) {
				it->second.enumOptions = options;
			}
		}

		void IScript::SetFieldEnumCallbacks(const std::string& name,
			std::function<int()> getEnumValue,
			std::function<void(int)> setEnumValue) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			auto it = m_fieldRegistry->fields.find(name);
			if (it != m_fieldRegistry->fields.end()) {
				it->second.getEnumValue = getEnumValue;
				it->second.setEnumValue = setEnumValue;
			}
		}

		void IScript::SetFieldLayerMaskCallbacks(const std::string& name,
			std::function<uint32_t()> getLayerMaskValue,
			std::function<void(uint32_t)> setLayerMaskValue) {
			if (!m_fieldRegistry) {
				m_fieldRegistry = new FieldRegistry();
			}

			auto it = m_fieldRegistry->fields.find(name);
			if (it != m_fieldRegistry->fields.end()) {
				it->second.getLayerMaskValue = getLayerMaskValue;
				it->second.setLayerMaskValue = setLayerMaskValue;
			}
		}

		//=========================================================================
		// Field Query Interface
		//=========================================================================

		std::vector<std::string> IScript::GetExposedFieldNames() const {
			if (!m_fieldRegistry) {
				return {};
			}

			std::vector<std::string> names;
			names.reserve(m_fieldRegistry->fields.size());
			for (const auto& [name, entry] : m_fieldRegistry->fields) {
				names.push_back(name);
			}
			return names;
		}

		std::string IScript::GetFieldType(const std::string& name) const {
			if (!m_fieldRegistry) {
				return {};
			}

			auto it = m_fieldRegistry->fields.find(name);
			if (it != m_fieldRegistry->fields.end()) {
				return it->second.typeToken;
			}
			return {};
		}

		std::string IScript::GetFieldValueAsString(const std::string& name) const {
			if (!m_fieldRegistry) {
				return {};
			}

			auto it = m_fieldRegistry->fields.find(name);
			if (it != m_fieldRegistry->fields.end()) {
				return it->second.getValue();
			}
			return {};
		}

		bool IScript::SetFieldValueFromString(const std::string& name, const std::string& value) {
			if (!m_fieldRegistry) {
				return false;
			}

			auto it = m_fieldRegistry->fields.find(name);
			if (it != m_fieldRegistry->fields.end()) {
				return it->second.setValue(value);
			}
			return false;
		}

		// Virtual methods with default implementations for optional override
		std::vector<std::string> IScript::GetEnumOptions(const std::string& fieldName) const {
			if (!m_fieldRegistry) return {};

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && !it->second.enumOptions.empty()) {
				return it->second.enumOptions;
			}
			return {};
		}

		int IScript::GetEnumValue(const std::string& fieldName) const {
			if (!m_fieldRegistry) return 0;

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && it->second.getEnumValue) {
				return it->second.getEnumValue();
			}
			return 0;
		}

		void IScript::SetEnumValue(const std::string& fieldName, int value) {
			if (!m_fieldRegistry) return;

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && it->second.setEnumValue) {
				it->second.setEnumValue(value);
			}
		}

		uint32_t IScript::GetLayerMaskValue(const std::string& fieldName) const {
			if (!m_fieldRegistry) return 0;

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && it->second.getLayerMaskValue) {
				return it->second.getLayerMaskValue();
			}
			return 0;
		}

		void IScript::SetLayerMaskValue(const std::string& fieldName, uint32_t value) {
			if (!m_fieldRegistry) return;

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && it->second.setLayerMaskValue) {
				it->second.setLayerMaskValue(value);
			}
		}

		size_t IScript::GetArraySize(const std::string& fieldName) const {
			if (!m_fieldRegistry) return 0;

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && it->second.getSize) {
				return it->second.getSize();
			}
			return 0;
		}

		std::string IScript::GetArrayElement(const std::string& fieldName, size_t index) const {
			if (!m_fieldRegistry) return "";

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && it->second.getElement) {
				return it->second.getElement(index);
			}
			return "";
		}

		bool IScript::SetArrayElement(const std::string& fieldName, size_t index, const std::string& value) {
			if (!m_fieldRegistry) return false;

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && it->second.setElement) {
				return it->second.setElement(index, value);
			}
			return false;
		}

		void IScript::AddArrayElement(const std::string& fieldName) {
			if (!m_fieldRegistry) return;

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && it->second.addElement) {
				it->second.addElement();
			}
		}

		void IScript::RemoveArrayElement(const std::string& fieldName, size_t index) {
			if (!m_fieldRegistry) return;

			auto it = m_fieldRegistry->fields.find(fieldName);
			if (it != m_fieldRegistry->fields.end() && it->second.removeElement) {
				it->second.removeElement(index);
			}
		}

		template<typename T>
		void IScript::MarkComponentDirty() {
			// what is this even for ??? ~ Irwen

			//// Only mark dirty in Edit mode - runtime changes should not be serialized
			//if (NE::GetEngineState() != NE::EngineState::Edit) {
			//	return;
			//}

			//if (!m_context->componentManager || !m_context->componentManager->HasComponent<T>(m_entity)) {
			//	return;
			//}

			//auto& component = m_context->componentManager->GetComponent<T>(m_entity);

			//// Use C++20 requires to check if the component has an isDirty field
			//if constexpr (requires { component.isDirty; }) {
			//	component.isDirty = true;
			//}
		}

		// === Entity Metadata Functions ===

		std::string IScript::GetEntityName(Entity entity) const {
			if (!m_context->componentManager) return "";

			Entity e = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(e))
				return "";

			return m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(e).name;
		}

		void IScript::SetEntityName(const std::string& name, Entity entity) {
			if (!m_context->componentManager) return;

			Entity e = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(e)) {
				auto& meta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(e);
				meta.name = name;
			}
		}

		/*uint8_t IScript::GetLayer(Entity entity) const {
			if (!m_context->componentManager) return 0;

			Entity e = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(e))
				return 0;

			return m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(e).layer;
		}*/

		/*void IScript::SetLayer(uint8_t layer, Entity entity) {
			if (!m_context->componentManager) return;

			Entity e = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			if (m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(e)) {
				auto& meta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(e);
				meta.layer = layer;
			}
		}*/

		// === Entity Active State Functions ===

		bool IScript::IsActive(Entity e) const {
			if (!m_context->componentManager) return false;

			if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(e))
				return true; // Default to active if no EntityMeta

			return m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(e).isActive;
		}

		void IScript::SetActive(bool active, Entity entity) {
			//if (!m_context->componentManager) return;

			//Entity e = (entity == DEFAULT_ENTITY_PARAM) ? m_entity : entity;

			//if (m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(e)) {
			//	auto& meta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(e);

			//	// Only update if changed
			//	if (meta.isActive != active) {
			//		meta.isActive = active;

			//		// 1. Update rendering visibility
			//		if (m_context->componentManager->HasComponent<NE::ECS::Component::Renderer>(e)) {
			//			auto& renderer = m_context->componentManager->GetComponent<NE::ECS::Component::Renderer>(e);
			//			renderer.visible = active && IsActiveInHierarchy();
			//		}

			//		// 2. Update physics state
			//		if (NE::Physics::PhysicsManager::EntityHasPhysicsBody(e)) {
			//			uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(e);

			//			if (active && IsActiveInHierarchy()) {
			//				// Reactivate physics body only if parent hierarchy is also active
			//				NE::Physics::PhysicsManager::ActivateBody(bodyID);
			//			} else {
			//				// Deactivate physics body (stops collision and physics simulation)
			//				NE::Physics::PhysicsManager::DeactivateBody(bodyID);
			//			}
			//		}

			//		// 3. Update script enabled state
			//		// When entity becomes inactive in hierarchy, the ScriptSystem will skip Update()
			//		// No need to manually disable here - the hierarchy check in ScriptSystem handles it

			//		// 4. Recursively propagate to all children (Unity-style)
			//		if (m_context->componentManager->HasComponent<NE::ECS::Component::Hierarchy>(e)) {
			//			auto& hierarchy = m_context->componentManager->GetComponent<NE::ECS::Component::Hierarchy>(e);
			//			PropagateActiveStateToChildren(hierarchy.children, active);
			//		}
			//	}
			//}
		}

		bool IScript::IsActiveInHierarchy() const {
			if (!m_context || !m_context->componentManager) return false;

			// Check if this entity is active
			if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(m_entity)) {
				return true; // Default to active if no EntityMeta
			}

			auto& meta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(m_entity);
			if (!meta.isActive) {
				return false; // This entity is disabled
			}

			// Check if any parent in the hierarchy is disabled
			if (!m_context->componentManager->HasComponent<NE::ECS::Component::Hierarchy>(m_entity)) {
				return true; // No parent, just check self
			}

			auto& hierarchy = m_context->componentManager->GetComponent<NE::ECS::Component::Hierarchy>(m_entity);
			if (hierarchy.parent == NE::ECS::Component::INVALID_PARENT) {
				return true; // No parent, entity is active
			}

			// Recursively check parent active state
			Entity currentParent = hierarchy.parent;
			while (currentParent != NE::ECS::Component::INVALID_PARENT) {
				if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(currentParent)) {
					break; // Parent has no EntityMeta, assume active
				}

				auto& parentMeta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(currentParent);
				if (!parentMeta.isActive) {
					return false; // Parent is disabled, so this entity is inactive in hierarchy
				}

				// Move up the hierarchy
				if (!m_context->componentManager->HasComponent<NE::ECS::Component::Hierarchy>(currentParent)) {
					break; // No hierarchy on parent, we're done
				}

				auto& parentHierarchy = m_context->componentManager->GetComponent<NE::ECS::Component::Hierarchy>(currentParent);
				currentParent = parentHierarchy.parent;
			}

			return true; // All parents are active
		}

		void IScript::PropagateActiveStateToChildren(const std::vector<uint32_t>& children, bool parentActive) const {
			//if (!m_context || !m_context->componentManager) return;

			//for (Entity childEntity : children) {
			//	// Get child's own isActive state
			//	if (!m_context->componentManager->HasComponent<NE::ECS::Component::EntityMeta>(childEntity)) {
			//		continue;
			//	}

			//	auto& childMeta = m_context->componentManager->GetComponent<NE::ECS::Component::EntityMeta>(childEntity);

			//	// Determine effective active state: parent must be active AND child must be active
			//	bool effectiveActive = parentActive && childMeta.isActive;

			//	// Update child's rendering
			//	if (m_context->componentManager->HasComponent<NE::ECS::Component::Renderer>(childEntity)) {
			//		auto& renderer = m_context->componentManager->GetComponent<NE::ECS::Component::Renderer>(childEntity);
			//		renderer.visible = effectiveActive;
			//	}

			//	// Update child's physics
			//	if (NE::Physics::PhysicsManager::EntityHasPhysicsBody(childEntity)) {
			//		uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(childEntity);

			//		if (effectiveActive) {
			//			NE::Physics::PhysicsManager::ActivateBody(bodyID);
			//		} else {
			//			NE::Physics::PhysicsManager::DeactivateBody(bodyID);
			//		}
			//	}

			//	// Recursively propagate to grandchildren
			//	if (m_context->componentManager->HasComponent<NE::ECS::Component::Hierarchy>(childEntity)) {
			//		auto& childHierarchy = m_context->componentManager->GetComponent<NE::ECS::Component::Hierarchy>(childEntity);
			//		PropagateActiveStateToChildren(childHierarchy.children, effectiveActive);
			//	}
			//}
		}

		//=========================================================================
		// Scene Management API IMPLEMENTATION (SDK → Engine bridge)
		//=========================================================================

		void SwitchScene(const std::string& path) {
			bool wasPlaying = gSceneManager.IsPlaying();

			// Stop runtime if currently playing
			if (wasPlaying) {
				gSceneManager.StopRuntime();
			}

			// Exit current scene and load new one
			gSceneManager.ExitScene();
			gSceneManager.LoadScene(path);

			// Resume playing if we were in play mode
			if (wasPlaying) {
				gSceneManager.LoadRuntime();
			}
		}

		//=========================================================================
		// LOGGING API IMPLEMENTATION (SDK → Engine bridge)
		//=========================================================================

		void Log(LogLevel level, const std::string& message, const std::string& file, int line) {
			// Convert SDK LogLevel to engine SpdLogLevel
			SpdLogLevel engineLevel;
			switch (level) {
			case LogLevel::Debug:    engineLevel = SpdLogLevel::Debug; break;
			case LogLevel::Info:     engineLevel = SpdLogLevel::Info; break;
			case LogLevel::Warning:  engineLevel = SpdLogLevel::Warning; break;
			case LogLevel::Error:    engineLevel = SpdLogLevel::Error; break;
			case LogLevel::Critical: engineLevel = SpdLogLevel::Critical; break;
			default:                 engineLevel = SpdLogLevel::Info; break;
			}

			// Forward to engine logger
			SpdLogger::GetInstance().Log(engineLevel, message, file, line);
		}

		//=========================================================================
		// COROUTINE API IMPLEMENTATION (SDK → Engine bridge)
		//=========================================================================

		CoroutineHandle CreateCoroutine() {
			return Engine_CreateCoroutine();
		}

		void AddCoroutineAction(CoroutineHandle handle, std::function<void()> action) {
			Engine_AddActionCpp(handle, action);
		}

		void AddCoroutineWait(CoroutineHandle handle, float seconds) {
			Engine_AddWaitForSeconds(handle, seconds);
		}

		void StartCoroutine(CoroutineHandle handle) {
			Engine_StartCoroutine(handle);
		}

		//=========================================================================
		// INPUT API IMPLEMENTATION (SDK → Engine bridge)
		//=========================================================================

		bool IsKeyDown(int key) {
			return NE::InputManager::IsKeyDown(key);
		}

		bool WasKeyPressed(int key) {
			return NE::InputManager::WasKeyPressed(key);
		}

		bool WasKeyReleased(int key) {
			return NE::InputManager::WasKeyReleased(key);
		}

		bool IsMouseDown(int button) {
			return NE::InputManager::IsMouseDown(button);
		}

		bool WasMousePressed(int button) {
			return NE::InputManager::WasMousePressed(button);
		}

		bool WasMouseReleased(int button) {
			return NE::InputManager::WasMouseReleased(button);
		}

		std::pair<double, double> MousePos() {
			return NE::InputManager::MousePos();
		}

		std::pair<double, double> MouseDelta() {
			return NE::InputManager::MouseDelta();
		}

		std::pair<double, double> ScrollDelta() {
			return NE::InputManager::ScrollDelta();
		}

		void SetMouseLocked(bool locked) {
			NE::InputManager::SetMouseLocked(locked);
		}

		bool IsMouseLocked() {
			return NE::InputManager::IsMouseLocked();
		}

		//=========================================================================
		// EVENT API IMPLEMENTATION (SDK → Engine bridge)
		//=========================================================================

		void SendScriptEvent(const char* eventName, void* data) {
			NANOEngine::Events::SendScriptEvent(eventName, data);
		}

		void RegisterScriptEventListener(const char* eventName, std::function<void(void*)> callback) {
			NANOEngine::Events::RegisterScriptEventListener(eventName, callback);
		}

		void ClearScriptEventListeners() {
			NANOEngine::Events::ClearScriptEventListeners();
		}

		//=========================================================================
		// TWEEN API IMPLEMENTATION (Wrapper to adapt lambdas to TweenManager)
		//=========================================================================

		// Wrapper objects that adapt lambda callbacks to member function pointers
		// These are lightweight adapters - TweenManager handles all the actual tweening logic

		// Wrapper for lambda-based tweens (receives normalized time 0-1)
		struct LambdaTweenWrapper {
			std::function<void(float)> callback;
			Entity entity;

			void SetValue(float value) {
				if (callback) {
					callback(value);
				}
			}
		};

		// Wrapper for Vec3 tweens
		struct Vec3TweenWrapper {
			std::function<void(const Vec3&)> callback;
			Entity entity;

			void SetValue(const Vec3& value) {
				if (callback) {
					callback(value);
				}
			}
		};

		// Wrapper for float tweens
		struct FloatTweenWrapper {
			std::function<void(float)> callback;
			Entity entity;

			void SetValue(float value) {
				if (callback) {
					callback(value);
				}
			}
		};

		// Global tween wrapper tracking for cleanup
		static std::unordered_map<TweenHandle, void*> s_tweenWrappers;
		static TweenHandle s_nextTweenHandle = 1;

		// Helper to convert SDK TweenType to engine TweenType
		inline ::TweenType ToEngineTweenType(TweenType type) {
			return static_cast<::TweenType>(static_cast<int>(type));
		}

		TweenHandle StartTweenLambda(
			std::function<void(float)> updateFunc,
			float duration,
			TweenType type,
			Entity entity)
		{
			// Create wrapper and call TweenManager::StartTween
			auto* wrapper = new LambdaTweenWrapper{ updateFunc, entity };

			TweenManager::Get().StartTween(
				wrapper,
				&LambdaTweenWrapper::SetValue,
				0.0f,
				1.0f,
				duration,
				ToEngineTweenType(type)
			);

			TweenHandle handle = s_nextTweenHandle++;
			s_tweenWrappers[handle] = wrapper;

			return handle;
		}

		TweenHandle StartTweenVec3(
			std::function<void(const Vec3&)> setter,
			const Vec3& start,
			const Vec3& end,
			float duration,
			TweenType type,
			Entity entity)
		{
			// Create wrapper and call TweenManager::StartTween
			auto* wrapper = new Vec3TweenWrapper{ setter, entity };

			TweenManager::Get().StartTween(
				wrapper,
				&Vec3TweenWrapper::SetValue,
				start,
				end,
				duration,
				ToEngineTweenType(type)
			);

			TweenHandle handle = s_nextTweenHandle++;
			s_tweenWrappers[handle] = wrapper;

			return handle;
		}

		TweenHandle StartTweenFloat(
			std::function<void(float)> setter,
			float start,
			float end,
			float duration,
			TweenType type,
			Entity entity)
		{
			// Create wrapper and call TweenManager::StartTween
			auto* wrapper = new FloatTweenWrapper{ setter, entity };

			TweenManager::Get().StartTween(
				wrapper,
				&FloatTweenWrapper::SetValue,
				start,
				end,
				duration,
				ToEngineTweenType(type)
			);

			TweenHandle handle = s_nextTweenHandle++;
			s_tweenWrappers[handle] = wrapper;

			return handle;
		}

		bool CheckEntityTween(Entity entity) {
			// Check if any wrapper belongs to this entity
			for (const auto& [handle, wrapperPtr] : s_tweenWrappers) {
				// Try each wrapper type
				auto* lambdaWrapper = static_cast<LambdaTweenWrapper*>(wrapperPtr);
				if (TweenManager::Get().CheckTween(lambdaWrapper) && lambdaWrapper->entity == entity) {
					return true;
				}
			}
			return false;
		}

		void StopTween(TweenHandle handle) {
			auto it = s_tweenWrappers.find(handle);
			if (it != s_tweenWrappers.end()) {
				// The wrapper will be cleaned up automatically by TweenManager when tween becomes inactive
				// We just remove our handle tracking
				s_tweenWrappers.erase(it);
			}
		}

		void StopEntityTweens(Entity entity) {
			// Remove all wrapper handles for this entity
			// TweenManager will clean up the actual tweens when they become inactive
			for (auto it = s_tweenWrappers.begin(); it != s_tweenWrappers.end();) {
				void* wrapperPtr = it->second;

				// Check wrapper entity (simplified type check)
				bool shouldErase = false;
				if (auto* wrapper = static_cast<LambdaTweenWrapper*>(wrapperPtr)) {
					if (wrapper->entity == entity) {
						shouldErase = true;
					}
				}

				if (shouldErase) {
					it = s_tweenWrappers.erase(it);
				} else {
					++it;
				}
			}
		}

		void ClearAllTweens() {
			// Use TweenManager's Clean() function to clear all tweens
			TweenManager::Get().Clean();
			s_tweenWrappers.clear();
		}

		//=========================================================================
		// RENDER SETTINGS API IMPLEMENTATION
		//=========================================================================

		// Environment Lighting
		EnvSource GetEnvSource() {
			auto& settings = Renderer::Command::GetRenderSettings();
			return static_cast<EnvSource>(settings.envSource);
		}

		void SetEnvSource(EnvSource source) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.envSource = static_cast<Graphics::RenderSettings::EnvSource>(source);
		}

		Vec3 GetAmbientColor() {
			auto& settings = Renderer::Command::GetRenderSettings();
			return ToSDKVec3(settings.ambientColour);
		}

		void SetAmbientColor(const Vec3& color) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.ambientColour = ToEngineVec3(color);
		}

		void SetAmbientColor(float r, float g, float b) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.ambientColour = Math::Vec3(r, g, b);
		}

		float GetAmbientIntensity() {
			auto& settings = Renderer::Command::GetRenderSettings();
			return settings.ambientIntensity;
		}

		void SetAmbientIntensity(float intensity) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.ambientIntensity = intensity;
		}

		// Fog Settings
		bool IsFogEnabled() {
			auto& settings = Renderer::Command::GetRenderSettings();
			return settings.fogEnabled;
		}

		void SetFogEnabled(bool enabled) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.fogEnabled = enabled;
		}

		FogMode GetFogMode() {
			auto& settings = Renderer::Command::GetRenderSettings();
			return static_cast<FogMode>(settings.fogMode);
		}

		void SetFogMode(FogMode mode) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.fogMode = static_cast<Graphics::RenderSettings::FogMode>(mode);
		}

		Vec3 GetFogColor() {
			auto& settings = Renderer::Command::GetRenderSettings();
			return ToSDKVec3(settings.fogColour);
		}

		void SetFogColor(const Vec3& color) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.fogColour = ToEngineVec3(color);
		}

		void SetFogColor(float r, float g, float b) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.fogColour = Math::Vec3(r, g, b);
		}

		float GetFogStart() {
			auto& settings = Renderer::Command::GetRenderSettings();
			return settings.fogStart;
		}

		void SetFogStart(float start) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.fogStart = start;
		}

		float GetFogEnd() {
			auto& settings = Renderer::Command::GetRenderSettings();
			return settings.fogEnd;
		}

		void SetFogEnd(float end) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.fogEnd = end;
		}

		float GetFogDensity() {
			auto& settings = Renderer::Command::GetRenderSettings();
			return settings.fogDensity;
		}

		void SetFogDensity(float density) {
			auto& settings = Renderer::Command::GetRenderSettings();
			settings.fogDensity = density;
		}

	} // namespace Scripting
} // namespace NE