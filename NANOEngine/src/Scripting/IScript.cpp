#include "IScript.hpp"
#include <Math/Vec3.hpp>
#include <sstream>
#include <unordered_map>
#include <functional>

#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/AudioSource.hpp"
#include "../ECS/Components/EntityMeta.hpp"
#include "../Physics/PhysicsManager.hpp"
#include "../EngineState.hpp"  // Include EngineState for dirty flag logic
#include "../Engine.hpp"  // Include Engine for MarkSceneDirty()

// PIMPL implementation to hide std containers from DLL interface
class IScript::FieldRegistry {
public:
	struct FieldEntry {
		std::string typeToken;
		void* memberPtr;
		std::function<std::string()> getValue;
		std::function<bool(const std::string&)> setValue;
	};

	std::unordered_map<std::string, FieldEntry> fields;
};

NE::ECS::Entity IScript::GetEntity() const {
	return m_entity;
}

IScript::~IScript() {
	delete m_fieldRegistry;
}

void IScript::LinkToEngine(NE::ECS::ComponentManager* componentManager) {
	m_componentManager = componentManager;

	// Initialize field registry if not already done
	if (!m_fieldRegistry) {
		m_fieldRegistry = new FieldRegistry();
	}
}

// === Field management implementation ===

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

void IScript::RegisterFloatField(const std::string& name, float* memberPtr) {
	// Initialize field registry if not already done
	if (!m_fieldRegistry) {
		m_fieldRegistry = new FieldRegistry();
	}

	FieldRegistry::FieldEntry entry;
	entry.typeToken = "float";
	entry.memberPtr = memberPtr;
	entry.getValue = [memberPtr]() -> std::string {
		return std::to_string(*memberPtr);
		};
	entry.setValue = [memberPtr](const std::string& value) -> bool {
		try {
			*memberPtr = std::stof(value);
			return true;
		}
		catch (...) {
			return false;
		}
		};
	m_fieldRegistry->fields[name] = std::move(entry);
}

void IScript::RegisterIntField(const std::string& name, int* memberPtr) {
	// Initialize field registry if not already done
	if (!m_fieldRegistry) {
		m_fieldRegistry = new FieldRegistry();
	}

	FieldRegistry::FieldEntry entry;
	entry.typeToken = "int";
	entry.memberPtr = memberPtr;
	entry.getValue = [memberPtr]() -> std::string {
		return std::to_string(*memberPtr);
		};
	entry.setValue = [memberPtr](const std::string& value) -> bool {
		try {
			*memberPtr = std::stoi(value);
			return true;
		}
		catch (...) {
			return false;
		}
		};
	m_fieldRegistry->fields[name] = std::move(entry);
}

void IScript::RegisterBoolField(const std::string& name, bool* memberPtr) {
	// Initialize field registry if not already done
	if (!m_fieldRegistry) {
		m_fieldRegistry = new FieldRegistry();
	}

	FieldRegistry::FieldEntry entry;
	entry.typeToken = "bool";
	entry.memberPtr = memberPtr;
	entry.getValue = [memberPtr]() -> std::string {
		return *memberPtr ? "1" : "0";
		};
	entry.setValue = [memberPtr](const std::string& value) -> bool {
		if (value == "1" || value == "true") {
			*memberPtr = true;
			return true;
		}
		if (value == "0" || value == "false") {
			*memberPtr = false;
			return true;
		}
		return false;
		};
	m_fieldRegistry->fields[name] = std::move(entry);
}

void IScript::RegisterStringField(const std::string& name, std::string* memberPtr) {
	// Initialize field registry if not already done
	if (!m_fieldRegistry) {
		m_fieldRegistry = new FieldRegistry();
	}

	FieldRegistry::FieldEntry entry;
	entry.typeToken = "string";
	entry.memberPtr = memberPtr;
	entry.getValue = [memberPtr]() -> std::string {
		return *memberPtr;
		};
	entry.setValue = [memberPtr](const std::string& value) -> bool {
		try {
			*memberPtr = value;
			return true;
		}
		catch (...) {
			return false;
		}
		};
	m_fieldRegistry->fields[name] = std::move(entry);
}

void IScript::RegisterVec3Field(const std::string& name, NE::Math::Vec3* memberPtr) {
	// Initialize field registry if not already done
	if (!m_fieldRegistry) {
		m_fieldRegistry = new FieldRegistry();
	}

	FieldRegistry::FieldEntry entry;
	entry.typeToken = "vec3";
	entry.memberPtr = memberPtr;
	entry.getValue = [memberPtr]() -> std::string {
		std::ostringstream oss;
		oss << memberPtr->x << ' ' << memberPtr->y << ' ' << memberPtr->z;
		return oss.str();
		};
	entry.setValue = [memberPtr](const std::string& value) -> bool {
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
		}
		catch (...) {
			return false;
		}
		};
	m_fieldRegistry->fields[name] = std::move(entry);
}

// === Transform Helper Functions ===

NE::Math::Vec3 IScript::GetPosition() const {
	if (!m_componentManager) return NE::Math::Vec3{ 0, 0, 0 };

	if (!m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity))
		return NE::Math::Vec3{ 0, 0, 0 };

	return m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity).position;
}

void IScript::SetPosition(const NE::Math::Vec3& pos) {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity)) {
		auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity);
		transform.position = pos;
		transform.isDirty = true;
		
		// Only mark dirty for serialization if in Edit mode
		if (NE::GetEngineState() == NE::EngineState::Edit) {
			transform.isDirty = true;  // This will trigger scene save
		}
	}
}

void IScript::SetPosition(float x, float y, float z) {
	SetPosition(NE::Math::Vec3{ x, y, z });
}

NE::Math::Vec3 IScript::GetRotation() const {
	if (!m_componentManager) return NE::Math::Vec3{ 0, 0, 0 };

	if (!m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity))
		return NE::Math::Vec3{ 0, 0, 0 };

	return m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity).rotation;
}

void IScript::SetRotation(const NE::Math::Vec3& rot) {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity)) {
		auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity);
		transform.rotation = rot;
		transform.isDirty = true;
		
		// Only mark dirty for serialization if in Edit mode
		if (NE::GetEngineState() == NE::EngineState::Edit) {
			transform.isDirty = true;  // This will trigger scene save
		}
	}
}

void IScript::SetRotation(float x, float y, float z) {
	SetRotation(NE::Math::Vec3{ x, y, z });
}

NE::Math::Vec3 IScript::GetScale() const {
	if (!m_componentManager) return NE::Math::Vec3{ 1, 1, 1 };

	if (!m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity))
		return NE::Math::Vec3{ 1, 1, 1 };

	return m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity).scale;
}

void IScript::SetScale(const NE::Math::Vec3& scale) {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity)) {
		auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity);
		transform.scale = scale;
		transform.isDirty = true;
		
		// Only mark dirty for serialization if in Edit mode
		if (NE::GetEngineState() == NE::EngineState::Edit) {
			transform.isDirty = true;  // This will trigger scene save
		}
	}
}

void IScript::SetScale(float x, float y, float z) {
	SetScale(NE::Math::Vec3{ x, y, z });
}

void IScript::SetScale(float uniformScale) {
	SetScale(NE::Math::Vec3{ uniformScale, uniformScale, uniformScale });
}

void IScript::Translate(const NE::Math::Vec3& translation) {
	SetPosition(GetPosition() + translation);
}

void IScript::Translate(float x, float y, float z) {
	Translate(NE::Math::Vec3{ x, y, z });
}

void IScript::Rotate(const NE::Math::Vec3& rotation) {
	SetRotation(GetRotation() + rotation);
}

void IScript::Rotate(float x, float y, float z) {
	Rotate(NE::Math::Vec3{ x, y, z });
}

Component::Transform& IScript::GetTransform(Entity entt) {
	return m_componentManager->GetComponent<NE::ECS::Component::Transform>(entt);
}

// === Transform Direction Vectors ===

NE::Math::Vec3 IScript::GetForward() const {
	NE::Math::Vec3 rotation = GetRotation(); // (pitch, yaw, roll) in degrees

	// Convert degrees to radians
	float pitch = rotation.x * (3.14159265f / 180.0f);
	float yaw = rotation.y * (3.14159265f / 180.0f);

	// Calculate forward vector from Euler angles (Y-up, Z-forward, X-right)
	NE::Math::Vec3 forward;
	forward.x = std::cos(pitch) * std::sin(yaw);
	forward.y = -std::sin(pitch);
	forward.z = -std::cos(pitch) * std::cos(yaw);

	// Normalize to get unit vector
	float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (length > 0.0001f) {
		forward.x /= length;
		forward.y /= length;
		forward.z /= length;
	}

	return forward;
}

NE::Math::Vec3 IScript::GetRight() const {
	NE::Math::Vec3 rotation = GetRotation(); // (pitch, yaw, roll) in degrees

	// Convert degrees to radians
	float yaw = rotation.y * (3.14159265f / 180.0f);

	// Right vector is perpendicular to forward in XZ plane
	NE::Math::Vec3 right;
	right.x = std::cos(yaw);
	right.y = 0.0f;
	right.z = std::sin(yaw);

	// Normalize
	float length = std::sqrt(right.x * right.x + right.z * right.z);
	if (length > 0.0001f) {
		right.x /= length;
		right.z /= length;
	}

	return right;
}

NE::Math::Vec3 IScript::GetUp() const {
	// Up is always world up in this simple implementation
	// For more complex scenarios, you might want to calculate it from forward and right
	return NE::Math::Vec3{ 0.0f, 1.0f, 0.0f };
}

// === Rigidbody Helper Functions ===

bool IScript::HasRigidbody() const {
	// Check if entity has a physics body registered with PhysicsManager
	return NE::Physics::PhysicsManager::EntityHasPhysicsBody(m_entity);
}

float IScript::GetMass() const {
	if (!m_componentManager) return 0.0f;

	if (!m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity))
		return 0.0f;

	return m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity).mass;
}

void IScript::SetMass(float mass) {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity)) {
		auto& rigidbody = m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity);
		rigidbody.mass = mass;
	}
}

bool IScript::GetUseGravity() const {
	if (!m_componentManager) return false;

	if (!m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity))
		return false;

	return m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity).useGravity;
}

void IScript::SetUseGravity(bool use) {
	if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

	uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(m_entity);
	NE::Physics::PhysicsManager::SetGravityEnabled(bodyID, use);

	// Also update Rigidbody component if it exists
	if (m_componentManager && m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity)) {
		auto& rigidbody = m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity);
		rigidbody.useGravity = use;
	}
}

bool IScript::IsStatic() const {
	if (!m_componentManager) return false;

	if (!m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity))
		return false;

	return m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity).isStatic;
}

void IScript::SetStatic(bool isStatic) {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity)) {
		auto& rigidbody = m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity);
		rigidbody.isStatic = isStatic;
	}
}

void IScript::LockRotation(bool lockX, bool lockY, bool lockZ) {
	if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

	uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(m_entity);
	NE::Physics::PhysicsManager::LockRotation(bodyID, lockX, lockY, lockZ);
}

// === Velocity and Force Methods ===

NE::Math::Vec3 IScript::GetVelocity() const {
	if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) {
		return NE::Math::Vec3{ 0, 0, 0 };
	}

	uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(m_entity);
	return NE::Physics::PhysicsManager::GetLinearVelocity(bodyID);
}

void IScript::SetVelocity(const NE::Math::Vec3& velocity) {
	if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

	uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(m_entity);
	NE::Physics::PhysicsManager::SetLinearVelocity(bodyID, velocity);
}

void IScript::SetVelocity(float x, float y, float z) {
	SetVelocity(NE::Math::Vec3{ x, y, z });
}

void IScript::AddForce(const NE::Math::Vec3& force) {
	if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

	uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(m_entity);
	NE::Physics::PhysicsManager::AddForce(bodyID, force);
}

void IScript::AddForce(float x, float y, float z) {
	AddForce(NE::Math::Vec3{ x, y, z });
}

void IScript::AddImpulse(const NE::Math::Vec3& impulse) {
	if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(m_entity)) return;

	uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(m_entity);
	NE::Physics::PhysicsManager::AddImpulse(bodyID, impulse);
}

void IScript::AddImpulse(float x, float y, float z) {
	AddImpulse(NE::Math::Vec3{ x, y, z });
}

// === Physics Raycasting Methods ===

// === Physics Raycasting Methods ===
IScript::RaycastHit IScript::Raycast(const NE::Math::Vec3& origin, const NE::Math::Vec3& direction, float maxDistance, uint32_t layerMask) const {
	RaycastHit result;

	if (!m_componentManager) {
		result.hasHit = false;
		return result;
	}

	// Call PhysicsManager raycast with layer mask
	auto hit = NE::Physics::PhysicsManager::Raycast(origin, direction, maxDistance, layerMask);

	// Convert PhysicsManager::RaycastHit to IScript::RaycastHit
	result.hasHit = hit.hasHit;
	result.point = hit.point;
	result.normal = hit.normal;
	result.distance = hit.distance;
	result.entity = hit.entity;
	return result;
}

IScript::RaycastHit IScript::Raycast(float originX, float originY, float originZ,
	float dirX, float dirY, float dirZ,
	float maxDistance, uint32_t layerMask) const {
	return Raycast(NE::Math::Vec3{ originX, originY, originZ },
		NE::Math::Vec3{ dirX, dirY, dirZ },
		maxDistance,
		layerMask);
}

std::vector<IScript::RaycastHit> IScript::RaycastAll(const NE::Math::Vec3& origin, const NE::Math::Vec3& direction, float maxDistance, uint32_t layerMask) const {
	std::vector<RaycastHit> results;

	if (!m_componentManager) {
		return results;
	}

	// Call PhysicsManager raycast with layer mask
	auto hits = NE::Physics::PhysicsManager::RaycastAll(origin, direction, maxDistance, layerMask);

	// Convert PhysicsManager::RaycastHit to IScript::RaycastHit
	results.reserve(hits.size());
	for (const auto& hit : hits) {
		RaycastHit result;
		result.hasHit = hit.hasHit;
		result.point = hit.point;
		result.normal = hit.normal;
		result.distance = hit.distance;
		result.entity = hit.entity;
		results.push_back(result);
	}

	return results;
}

// === AudioSource Helper Functions ===

bool IScript::HasAudioSource() const {
	if (!m_componentManager) return false;
	return m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity);
}

// === Entity Active State Functions ===

bool IScript::IsActive() const {
	if (!m_componentManager) return false;

	if (!m_componentManager->HasComponent<NE::ECS::Component::EntityMeta>(m_entity))
		return true; // Default to active if no EntityMeta

	return m_componentManager->GetComponent<NE::ECS::Component::EntityMeta>(m_entity).isActive;
}

void IScript::SetActive(bool active) {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::EntityMeta>(m_entity)) {
		auto& meta = m_componentManager->GetComponent<NE::ECS::Component::EntityMeta>(m_entity);
		
		// Only update if changed
		if (meta.isActive != active) {
			meta.isActive = active;
			
			// Mark scene dirty when active state changes
			if (NE::GetEngineState() == NE::EngineState::Edit) {
				NE::MarkSceneDirty();
			}
		}
	}
}

void IScript::PlayAudio() {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity)) {
		auto& audioSource = m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity);

		// If already playing and not paused, stop first
		if (audioSource.m_channel && audioSource.isPlaying && !audioSource.isPaused) {
			audioSource.m_channel->stop();
		}

		// Reset state - AudioSystem will handle actual playback
		audioSource.isPlaying = true;
		audioSource.isPaused = false;
		audioSource.m_hasPlayed = false; // Trigger playback in AudioSystem
	}
}

void IScript::StopAudio() {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity)) {
		auto& audioSource = m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity);

		if (audioSource.m_channel) {
			audioSource.m_channel->stop();
		}

		audioSource.isPlaying = false;
		audioSource.isPaused = false;
	}
}

void IScript::PauseAudio() {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity)) {
		auto& audioSource = m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity);

		if (audioSource.m_channel && audioSource.isPlaying) {
			audioSource.m_channel->setPaused(true);
			audioSource.isPaused = true;
		}
	}
}

void IScript::ResumeAudio() {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity)) {
		auto& audioSource = m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity);

		if (audioSource.m_channel && audioSource.isPaused) {
			audioSource.m_channel->setPaused(false);
			audioSource.isPaused = false;
		}
	}
}

bool IScript::IsAudioPlaying() const {
	if (!m_componentManager) return false;

	if (!m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity))
		return false;

	const auto& audioSource = m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity);
	return audioSource.isPlaying && !audioSource.isPaused;
}

float IScript::GetVolume() const {
	if (!m_componentManager) return 0.0f;

	if (!m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity))
		return 0.0f;

	return m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity).volume;
}

void IScript::SetVolume(float volume) {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity)) {
		auto& audioSource = m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity);
		audioSource.volume = volume;

		// Apply immediately if playing
		if (audioSource.m_channel) {
			audioSource.m_channel->setVolume(volume);
		}
	}
}

float IScript::GetPitch() const {
	if (!m_componentManager) return 1.0f;

	if (!m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity))
		return 1.0f;

	return m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity).pitch;
}

void IScript::SetPitch(float pitch) {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity)) {
		auto& audioSource = m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity);
		audioSource.pitch = pitch;

		// Apply immediately if playing
		if (audioSource.m_channel) {
			audioSource.m_channel->setPitch(pitch);
		}
	}
}

void IScript::SetAudioLoop(bool loop) {
	if (!m_componentManager) return;

	if (m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(m_entity)) {
		auto& audioSource = m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(m_entity);
		audioSource.loop = loop;
	}
}

void IScript::RefreshComponentReferences() {
	if (!m_fieldRegistry || !m_componentManager) {
		return;
	}

	// Iterate through all registered fields
	for (auto& [fieldName, fieldEntry] : m_fieldRegistry->fields) {
		// Only process component reference fields
		if (fieldEntry.typeToken.starts_with("componentref:")) {
			// Extract component type from typeToken (e.g., "Transform" from "componentref:Transform")
			std::string componentType = fieldEntry.typeToken.substr(13); // Skip "componentref:"
			
			// Get the stored entity ID (from ComponentRef.ownerEntity)
			std::string entityIdStr = fieldEntry.getValue();
			
			if (entityIdStr.empty() || entityIdStr == "0") {
				continue; // No entity assigned
			}
			
			try {
				uint32_t entityId = static_cast<uint32_t>(std::stoul(entityIdStr));
				
				// Now resolve the entity ID to a component pointer
				if (componentType == "Transform") {
					auto* compRef = static_cast<ComponentRef<NE::ECS::Component::Transform>*>(fieldEntry.memberPtr);
					if (m_componentManager->HasComponent<NE::ECS::Component::Transform>(entityId)) {
						auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(entityId);
						compRef->componentPtr = &transform;
						compRef->ownerEntity = entityId;
					} else {
						// Component doesn't exist - clear reference
						compRef->componentPtr = nullptr;
						compRef->ownerEntity = 0;
					}
				}
				else if (componentType == "Rigidbody") {
					auto* compRef = static_cast<ComponentRef<NE::ECS::Component::Rigidbody>*>(fieldEntry.memberPtr);
					if (m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(entityId)) {
						auto& rb = m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(entityId);
						compRef->componentPtr = &rb;
						compRef->ownerEntity = entityId;
					} else {
						compRef->componentPtr = nullptr;
						compRef->ownerEntity = 0;
					}
				}
				else if (componentType == "AudioSource") {
					auto* compRef = static_cast<ComponentRef<NE::ECS::Component::AudioSource>*>(fieldEntry.memberPtr);
					if (m_componentManager->HasComponent<NE::ECS::Component::AudioSource>(entityId)) {
						auto& audio = m_componentManager->GetComponent<NE::ECS::Component::AudioSource>(entityId);
						compRef->componentPtr = &audio;
						compRef->ownerEntity = entityId;
					} else {
						compRef->componentPtr = nullptr;
						compRef->ownerEntity = 0;
					}
				}
			}
			catch (...) {
				// Invalid entity ID - skip
			}
		}
	}
}

// === Component Reference Field Registration (Template Specializations) ===

template<>
void IScript::RegisterComponentRefField<NE::ECS::Component::Transform>(const std::string& name, ComponentRef<NE::ECS::Component::Transform>* memberPtr) {
	// Initialize field registry if not already done
	if (!m_fieldRegistry) {
		m_fieldRegistry = new FieldRegistry();
	}

	FieldRegistry::FieldEntry entry;
	entry.typeToken = "componentref:Transform";
	entry.memberPtr = memberPtr;

	// Store the entity ID as a string
	entry.getValue = [memberPtr]() -> std::string {
		if (memberPtr->GetOwnerEntity() == NE::ECS::NO_ENTITY) {
			return std::to_string(NE::ECS::NO_ENTITY);
		}
		return std::to_string(memberPtr->GetOwnerEntity());
	};

	// Set the entity ID 
	entry.setValue = [memberPtr](const std::string& value) -> bool {
		try {
			std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);
			if (value == noEntityStr || value.empty()) {
				memberPtr->Set(nullptr, NE::ECS::NO_ENTITY);
				return true;
			}
			
			// Parse entity ID and store it (but DON'T resolve pointer yet)
			uint32_t entityId = static_cast<uint32_t>(std::stoul(value));
			
			// Store the entity ID but leave pointer as nullptr for now
			// It will be resolved by RefreshComponentReferences()
			memberPtr->ownerEntity = entityId;
			memberPtr->componentPtr = nullptr;
			
			return true;
		}
		catch (...) {
			memberPtr->Set(nullptr, NE::ECS::NO_ENTITY);
			return false;
		}
	};

	m_fieldRegistry->fields[name] = std::move(entry);
}

template<>
void IScript::RegisterComponentRefField<NE::ECS::Component::Rigidbody>(const std::string& name, ComponentRef<NE::ECS::Component::Rigidbody>* memberPtr) {
	// Initialize field registry if not already done
	if (!m_fieldRegistry) {
		m_fieldRegistry = new FieldRegistry();
	}

	FieldRegistry::FieldEntry entry;
	entry.typeToken = "componentref:Rigidbody";
	entry.memberPtr = memberPtr;

	entry.getValue = [memberPtr]() -> std::string {
		if (memberPtr->GetOwnerEntity() == NE::ECS::NO_ENTITY) {
			return std::to_string(NE::ECS::NO_ENTITY);
		}
		return std::to_string(memberPtr->GetOwnerEntity());
	};

	entry.setValue = [memberPtr](const std::string& value) -> bool {
		try {
			std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);
			if (value == noEntityStr || value.empty()) {
				memberPtr->Set(nullptr, NE::ECS::NO_ENTITY);
				return true;
			}
			
			uint32_t entityId = static_cast<uint32_t>(std::stoul(value));
			
			// Store entity ID only - will be resolved later
			memberPtr->ownerEntity = entityId;
			memberPtr->componentPtr = nullptr;
			
			return true;
		}
		catch (...) {
			memberPtr->Set(nullptr, NE::ECS::NO_ENTITY);
			return false;
		}
	};

	m_fieldRegistry->fields[name] = std::move(entry);
}

template<>
void IScript::RegisterComponentRefField<NE::ECS::Component::AudioSource>(const std::string& name, ComponentRef<NE::ECS::Component::AudioSource>* memberPtr) {
	// Initialize field registry if not already done
	if (!m_fieldRegistry) {
		m_fieldRegistry = new FieldRegistry();
	}

	FieldRegistry::FieldEntry entry;
	entry.typeToken = "componentref:AudioSource";
	entry.memberPtr = memberPtr;

	entry.getValue = [memberPtr]() -> std::string {
		if (memberPtr->GetOwnerEntity() == NE::ECS::NO_ENTITY) {
			return std::to_string(NE::ECS::NO_ENTITY);
		}
		return std::to_string(memberPtr->GetOwnerEntity());
	};

	entry.setValue = [memberPtr](const std::string& value) -> bool {
		try {
			std::string noEntityStr = std::to_string(NE::ECS::NO_ENTITY);
			if (value == noEntityStr || value.empty()) {
				memberPtr->Set(nullptr, NE::ECS::NO_ENTITY);
				return true;
			}
			
			uint32_t entityId = static_cast<uint32_t>(std::stoul(value));
			
			// Store entity ID only - will be resolved later
			memberPtr->ownerEntity = entityId;
			memberPtr->componentPtr = nullptr;
			
			return true;
		}
		catch (...) {
			memberPtr->Set(nullptr, NE::ECS::NO_ENTITY);
			return false;
		}
	};

	m_fieldRegistry->fields[name] = std::move(entry);
}

// Explicit template instantiation definitions (required for DLL export)
template void IScript::RegisterComponentRefField<NE::ECS::Component::Transform>(const std::string&, ComponentRef<NE::ECS::Component::Transform>*);
template void IScript::RegisterComponentRefField<NE::ECS::Component::Rigidbody>(const std::string&, ComponentRef<NE::ECS::Component::Rigidbody>*);
template void IScript::RegisterComponentRefField<NE::ECS::Component::AudioSource>(const std::string&, ComponentRef<NE::ECS::Component::AudioSource>*);