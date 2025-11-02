#include "IScript.hpp" 
#include <Math/Vec3.hpp>
#include <sstream>
#include <unordered_map>
#include <functional>

#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/AudioSource.hpp"
#include "../Physics/PhysicsManager.hpp"

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
    if (!m_componentManager) return NE::Math::Vec3{0, 0, 0};
    
  if (!m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity))
        return NE::Math::Vec3{0, 0, 0};
    
    return m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity).position;
}

void IScript::SetPosition(const NE::Math::Vec3& pos) {
    if (!m_componentManager) return;
    
  if (m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity)) {
     auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity);
        transform.position = pos;
      transform.isDirty = true;
    }
}

void IScript::SetPosition(float x, float y, float z) {
    SetPosition(NE::Math::Vec3{x, y, z});
}

NE::Math::Vec3 IScript::GetRotation() const {
    if (!m_componentManager) return NE::Math::Vec3{0, 0, 0};
    
    if (!m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity))
        return NE::Math::Vec3{0, 0, 0};
    
    return m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity).rotation;
}

void IScript::SetRotation(const NE::Math::Vec3& rot) {
 if (!m_componentManager) return;
    
    if (m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity)) {
        auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity);
   transform.rotation = rot;
 transform.isDirty = true;
    }
}

void IScript::SetRotation(float x, float y, float z) {
    SetRotation(NE::Math::Vec3{x, y, z});
}

NE::Math::Vec3 IScript::GetScale() const {
    if (!m_componentManager) return NE::Math::Vec3{1, 1, 1};
    
  if (!m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity))
      return NE::Math::Vec3{1, 1, 1};
    
    return m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity).scale;
}

void IScript::SetScale(const NE::Math::Vec3& scale) {
    if (!m_componentManager) return;
    
    if (m_componentManager->HasComponent<NE::ECS::Component::Transform>(m_entity)) {
  auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(m_entity);
     transform.scale = scale;
        transform.isDirty = true;
    }
}

void IScript::SetScale(float x, float y, float z) {
    SetScale(NE::Math::Vec3{x, y, z});
}

void IScript::SetScale(float uniformScale) {
    SetScale(NE::Math::Vec3{uniformScale, uniformScale, uniformScale});
}

void IScript::Translate(const NE::Math::Vec3& translation) {
    SetPosition(GetPosition() + translation);
}

void IScript::Translate(float x, float y, float z) {
    Translate(NE::Math::Vec3{x, y, z});
}

void IScript::Rotate(const NE::Math::Vec3& rotation) {
    SetRotation(GetRotation() + rotation);
}

void IScript::Rotate(float x, float y, float z) {
    Rotate(NE::Math::Vec3{x, y, z});
}

// === Rigidbody Helper Functions ===

bool IScript::HasRigidbody() const {
    if (!m_componentManager) return false;
    return m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity);
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
    if (!m_componentManager) return;
    
    if (m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity)) {
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
    if (!m_componentManager) return;
    
    if (m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity)) {
        auto& rb = m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity);
        NE::Physics::PhysicsManager::LockRotation(rb.bodyID, lockX, lockY, lockZ);
    }
}

// === Velocity and Force Methods ===

NE::Math::Vec3 IScript::GetVelocity() const {
    if (!m_componentManager) return NE::Math::Vec3{0, 0, 0};
    
    if (!m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity))
        return NE::Math::Vec3{0, 0, 0};
    
    const auto& rb = m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity);
    return NE::Physics::PhysicsManager::GetLinearVelocity(rb.bodyID);
}

void IScript::SetVelocity(const NE::Math::Vec3& velocity) {
    if (!m_componentManager) return;
    
    if (m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity)) {
       auto& rb = m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity);
      NE::Physics::PhysicsManager::SetLinearVelocity(rb.bodyID, velocity);
    }
}

void IScript::SetVelocity(float x, float y, float z) {
    SetVelocity(NE::Math::Vec3{x, y, z});
}

void IScript::AddForce(const NE::Math::Vec3& force) {
    if (!m_componentManager) return;
 
    if (m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity)) {
        auto& rb = m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity);
       NE::Physics::PhysicsManager::AddForce(rb.bodyID, force);
    }
}

void IScript::AddForce(float x, float y, float z) {
    AddForce(NE::Math::Vec3{x, y, z});
}

void IScript::AddImpulse(const NE::Math::Vec3& impulse) {
    if (!m_componentManager) return;
    
    if (m_componentManager->HasComponent<NE::ECS::Component::Rigidbody>(m_entity)) {
    auto& rb = m_componentManager->GetComponent<NE::ECS::Component::Rigidbody>(m_entity);
   NE::Physics::PhysicsManager::AddImpulse(rb.bodyID, impulse);
    }
}

void IScript::AddImpulse(float x, float y, float z) {
    AddImpulse(NE::Math::Vec3{x, y, z});
}

// === Physics Raycasting Methods ===

IScript::RaycastHit IScript::Raycast(const NE::Math::Vec3& origin, const NE::Math::Vec3& direction, float maxDistance) const {
    RaycastHit result;
    
    if (!m_componentManager) {
  result.hasHit = false;
 return result;
    }
    
    // Call PhysicsManager raycast
    auto hit = NE::Physics::PhysicsManager::Raycast(origin, direction, maxDistance);
    
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
              float maxDistance) const {
    return Raycast(NE::Math::Vec3{originX, originY, originZ}, 
  NE::Math::Vec3{dirX, dirY, dirZ}, 
            maxDistance);
}

std::vector<IScript::RaycastHit> IScript::RaycastAll(const NE::Math::Vec3& origin, const NE::Math::Vec3& direction, float maxDistance) const {
    std::vector<RaycastHit> results;
    
    if (!m_componentManager) {
      return results;
    }
    
    // Call PhysicsManager raycast
    auto hits = NE::Physics::PhysicsManager::RaycastAll(origin, direction, maxDistance);
    
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
