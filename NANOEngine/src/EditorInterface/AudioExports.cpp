#include "pch.h"
#include "AudioExports.hpp"
#include "../ECS/Systems/AudioSystem.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../Core/SpdLogger.hpp"
#include <fmod/fmod_studio.hpp>
#include <vector>

namespace NE {
	// Forward declaration - defined elsewhere in engine
	SceneManagement::Scene& GetScene();
}

namespace NE::Audio {

	namespace {
		std::shared_ptr<NE::ECS::Systems::AudioSystem> GetAudioSystem()
		{
			auto& scene = NE::GetScene();
			auto audioSystem = scene.GetECSCoordinator().m_audioSystem;
			if (!audioSystem) {
				SPD_ERROR("[Audio] AudioSystem not found");
			}
			return audioSystem;
		}
	}

	// Bus volume control implementations
	void SetMasterVolume(float volume)
	{
		auto audioSystem = GetAudioSystem();
		if (audioSystem) {
			audioSystem->SetMasterVolume(volume);
		}
	}

	void SetBGMVolume(float volume)
	{
		auto audioSystem = GetAudioSystem();
		if (audioSystem) {
			audioSystem->SetBGMVolume(volume);
		}
	}

	void SetSFXVolume(float volume)
	{
		auto audioSystem = GetAudioSystem();
		if (audioSystem) {
			audioSystem->SetSFXVolume(volume);
		}
	}

	void SetAmbienceVolume(float volume)
	{
		auto audioSystem = GetAudioSystem();
		if (audioSystem) {
			audioSystem->SetAmbienceVolume(volume);
		}
	}

	float GetMasterVolume()
	{
		auto audioSystem = GetAudioSystem();
		if (audioSystem) {
			return audioSystem->GetMasterVolume();
		}
		return 0.0f;
	}

	float GetBGMVolume()
	{
		auto audioSystem = GetAudioSystem();
		if (audioSystem) {
			return audioSystem->GetBGMVolume();
		}
		return 0.0f;
	}

	float GetSFXVolume()
	{
		auto audioSystem = GetAudioSystem();
		if (audioSystem) {
			return audioSystem->GetSFXVolume();
		}
		return 0.0f;
	}

	float GetAmbienceVolume()
	{
		auto audioSystem = GetAudioSystem();
		if (audioSystem) {
			return audioSystem->GetAmbienceVolume();
		}
		return 0.0f;
	}

	void PlayEntityAudio(uint32_t entity, const std::string& eventName) {
		auto audioSystem = GetAudioSystem();
		if (!audioSystem) return;
		auto& coordinator = NE::GetScene().GetECSCoordinator();
		if (!coordinator.HasComponent<NE::ECS::Component::Transform>(entity)) {
			SPD_ERROR("[Audio] PlayEntityAudio: entity has no Transform");
			return;
		}
		const auto& transform = coordinator.GetComponent<NE::ECS::Component::Transform>(entity);
		audioSystem->PlayEntitySound(entity, eventName, transform);
	}

	void StopEntityAudio(uint32_t entity) {
		auto audioSystem = GetAudioSystem();
		if (audioSystem) audioSystem->StopEntitySound(entity);
	}

	bool IsEntityAudioPlaying(uint32_t entity) {
		auto audioSystem = GetAudioSystem();
		return audioSystem ? audioSystem->IsEntitySoundPlaying(entity) : false;
	}

	void PlayAudio(const std::string& eventName) {
		auto& scene = NE::GetScene();
		auto audioSystem = scene.GetECSCoordinator().m_audioSystem;

		if (audioSystem) {
			audioSystem->PlaySound(eventName);
		}
		else {
			SPD_ERROR("[Audio] AudioSystem not found - cannot play event: " << eventName);
		}
	}

	void StopAudio(const std::string& eventName) {
		auto& scene = NE::GetScene();
		auto audioSystem = scene.GetECSCoordinator().m_audioSystem;

		if (audioSystem && audioSystem->studioSystem) {
			FMOD::Studio::EventDescription* eventDesc = nullptr;
			FMOD_RESULT result = audioSystem->studioSystem->getEvent(eventName.c_str(), &eventDesc);

			if (result == FMOD_OK && eventDesc != nullptr) {
				int instanceCount = 0;
				eventDesc->getInstanceCount(&instanceCount);

				if (instanceCount > 0) {
					std::vector<FMOD::Studio::EventInstance*> instances(instanceCount);
					int retrieved = 0;
					eventDesc->getInstanceList(instances.data(), instanceCount, &retrieved);

					for (int i = 0; i < retrieved; ++i) {
						instances[i]->stop(FMOD_STUDIO_STOP_IMMEDIATE);
					}
				}
			}
			else {
				SPD_WARNING("[Audio] Failed to find event for stopping: " << eventName);
			}
		}
		else {
			SPD_ERROR("[Audio] AudioSystem not available");
		}
	}

	void StopAllAudio() {
		auto& scene = NE::GetScene();
		auto audioSystem = scene.GetECSCoordinator().m_audioSystem;

		if (audioSystem && audioSystem->studioSystem) {
			auto allEvents = audioSystem->GetAllEvents();

			for (const auto& [eventPath, eventInfo] : allEvents) {
				if (eventInfo.eventDesc) {
					int instanceCount = 0;
					eventInfo.eventDesc->getInstanceCount(&instanceCount);

					if (instanceCount > 0) {
						std::vector<FMOD::Studio::EventInstance*> instances(instanceCount);
						int retrieved = 0;
						eventInfo.eventDesc->getInstanceList(instances.data(), instanceCount, &retrieved);

						for (int i = 0; i < retrieved; ++i) {
							instances[i]->stop(FMOD_STUDIO_STOP_IMMEDIATE);
						}
					}
				}
			}

			SPD_INFO("[Audio] Stopped all FMOD events");
		}
		else {
			SPD_ERROR("[Audio] AudioSystem not available");
		}
	}




	void SetMasterVolumeLevel(int level) {
		auto& scene = NE::GetScene();
		auto audioSystem = scene.GetECSCoordinator().m_audioSystem;

		if (audioSystem) {
			audioSystem->SetMasterVolumeLevel(level);
		}
		else {
			SPD_ERROR("[Audio] AudioSystem not found - cannot set master volume");
		}
	}

	int GetMasterVolumeLevel() {
		auto& scene = NE::GetScene();
		auto audioSystem = scene.GetECSCoordinator().m_audioSystem;

		if (audioSystem) {
			return audioSystem->GetMasterVolumeLevel();
		}
		SPD_ERROR("[Audio] AudioSystem not found - cannot get master volume");
		return 5;
	}

} // namespace NE::Audio