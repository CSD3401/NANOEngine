#include "IScript.hpp" 
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/AudioSource.hpp"

NE::ECS::Entity IScript::GetEntity() const {
    return m_entity;
}

IScript::~IScript() = default;

void IScript::LinkToEngine(NE::ECS::ComponentManager* componentManager) {
    m_componentManager = componentManager;
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
