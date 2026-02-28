#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/AudioSource.hpp"
#include "../Components/Transform.hpp"
#include <fmod/fmod.hpp>
#include <fmod/fmod_studio.hpp>
#include "../../Audio/AudioBank.hpp"
#include <map>
#include <memory>
#include <unordered_map>

namespace NE::ECS::Systems
{

	class AudioSystem final : public System {
	public:
		explicit AudioSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void Exit() override;

		// Fmod Studio linking test code
		FMOD::Studio::System* studioSystem = nullptr;
		void SetupStudioSystem();
		void PlaySound(const std::string& eventName);
		void CleanupStudioSystem();
		void LoadBankAssets(const std::string& audioDirectory);

		// Bus volume control functions
		void SetMasterVolume(float volume);
		void SetBGMVolume(float volume);
		void SetSFXVolume(float volume);
		void SetAmbienceVolume(float volume);

		float GetMasterVolume() const;
		float GetBGMVolume() const;
		float GetSFXVolume() const;
		float GetAmbienceVolume() const;

		struct AudioEvent
		{
			std::string path;        // "event:/Footsteps/Concrete"
			std::string displayName; // "Footsteps Concrete"
			std::string bankName;    // "Master.bank"
		};

		struct BankData
		{
			std::string filepath;
			FMOD::Studio::Bank* bank;
		};

		std::unordered_map<std::string, NE::Asset::AudioBank::EventInfo> GetAllEvents() const;
		// Master volume control (0..5 discrete levels)
		void SetMasterVolumeLevel(int level);
		int  GetMasterVolumeLevel() const { return m_masterVolumeLevel; }


	private:
		int m_masterVolumeLevel = 5; // 0..5 -> razi dun use this
		void ApplyMasterVolume();  // razi -> dun use this

		ComponentManager* m_componentManager;

		// Cached bus pointers
		FMOD::Studio::Bus* m_masterBus = nullptr;
		FMOD::Studio::Bus* m_bgmBus = nullptr;
		FMOD::Studio::Bus* m_sfxBus = nullptr;
		FMOD::Studio::Bus* m_ambienceBus = nullptr;

		// Store loaded banks (key = bank file path, value = AudioBank shared_ptr)
		std::unordered_map<std::string, std::shared_ptr<NE::Asset::AudioBank>> m_loadedBanks;

		// Following function are deprecated do not use
		// Generally non-Raphael should only be using PlaySound()

		void ProcessAudioSource(
			NE::ECS::Component::AudioSource& source,
			const NE::ECS::Component::Transform& transform,
			FMOD::System* system);

		void PlayAudio(
			NE::ECS::Component::AudioSource& source,
			const NE::ECS::Component::Transform& transform,
			FMOD::System* system);

		void UpdateAudioPlayback(
			NE::ECS::Component::AudioSource& source,
			const NE::ECS::Component::Transform& transform,
			FMOD::System* system);
	};
}