#include "pch.h"
// AudioSystem.cpp - FIXED VERSION
// This version properly loads FMOD banks using the new ResourceManager system

#include "AudioSystem.hpp"
#include "../Components/AudioSource.hpp"
#include "../Components/Transform.hpp"
#include "../../Core/Profiler.hpp"
#include "../../Core/SpdLogger.hpp"
#include "../../ResourceManagement/ResourceManager.hpp"
#include <fmod/fmod_errors.h>
#include <filesystem>
#include <direct.h>
#include <cstring>

using namespace NE::ECS::Component;


namespace NE::ECS::Systems {


#pragma region AudioEngine
	namespace {
		// Private implementation hidden in .cpp

		// Singleton
		struct AudioEngine
		{
			FMOD::System* system = nullptr;
			std::unordered_map<std::string, FMOD::Sound*> loadedClips;

			AudioEngine()
			{
				FMOD::System_Create(&system);
				system->init(512, FMOD_INIT_NORMAL, 0);
				SPD_INFO("AudioEngine constructor called");
			}

			~AudioEngine()
			{
				SPD_INFO("AudioEngine destructor called");
				// Cleanup all loaded sounds
				for (auto& [path, sound] : loadedClips) {
					sound->release();
				}
				loadedClips.clear();

				// Cleanup FMOD system
				if (system) {
					system->close();
					system->release();
				}
			}

			FMOD::Sound* LoadSound(const std::string& filepath, bool loop = false) {
				if (loadedClips.find(filepath) != loadedClips.end()) {
					return loadedClips[filepath];
				}

				FMOD::Sound* sound = nullptr;
				FMOD_MODE mode = FMOD_DEFAULT;
				if (loop) {
					mode |= FMOD_LOOP_NORMAL;
				}

				FMOD_RESULT result = system->createSound(filepath.c_str(), mode, 0, &sound);

				if (result == FMOD_OK) {
					loadedClips[filepath] = sound;
					return sound;
				}

				return nullptr;
			}

		};

		AudioEngine& GetAudioEngine() {
			static AudioEngine instance;
			return instance;
		}
	}
#pragma endregion





	void AudioSystem::ProcessAudioSource(
		NE::ECS::Component::AudioSource& audioSource,
		const NE::ECS::Component::Transform& transform,
		FMOD::System* system)
	{
		// Load audio clip if needed
		if (audioSource.m_sound == nullptr && !audioSource.audioClipPath.empty()) {
			audioSource.m_sound = GetAudioEngine().LoadSound(audioSource.audioClipPath, audioSource.loop);
		}

		// Handle playOnAwake
		if (audioSource.playOnAwake && !audioSource.m_hasPlayed && audioSource.m_sound) {
			PlayAudio(audioSource, transform, system);
			audioSource.m_hasPlayed = true;
		}

		// Update ongoing playback
		if (audioSource.isPlaying && audioSource.m_channel) {
			UpdateAudioPlayback(audioSource, transform, system);
		}
	}

	void AudioSystem::PlayAudio(
		NE::ECS::Component::AudioSource& audioSource,
		const NE::ECS::Component::Transform& /*transform*/,
		FMOD::System* system)
	{

		if (!audioSource.m_sound)
			return;

		// Stop previous playback
		if (audioSource.m_channel) {
			audioSource.m_channel->stop();
			audioSource.m_channel = nullptr;
		}

		// Play the sound
		FMOD::Channel* channel = nullptr;
		if (system->playSound(audioSource.m_sound, 0, false, &channel) == FMOD_OK && channel) {
			audioSource.m_channel = channel;
			audioSource.isPlaying = true;
			audioSource.isPaused = false;

			// Set audio properties
			channel->setVolume(audioSource.volume);
			channel->setPitch(audioSource.pitch);

			// Set 3D attributes if spatial audio
			//if (audioSource.spatialBlend > 0.0f) {
			//	FMOD_VECTOR pos = { transform.position.x, transform.position.y, transform.position.z };
			//	FMOD_VECTOR vel = { 0, 0, 0 }; // Zero velocity for now
			//	channel->set3DAttributes(&pos, &vel);
			//	channel->set3DMinMaxDistance(audioSource.minDist, audioSource.maxDist);
			//}
		}
	}

	void AudioSystem::UpdateAudioPlayback(
		NE::ECS::Component::AudioSource& audioSource,
		const NE::ECS::Component::Transform& transform,
		FMOD::System* /*system*/)
	{

		if (!audioSource.m_channel) return;

		// Update volume and pitch
		audioSource.m_channel->setVolume(audioSource.volume);
		audioSource.m_channel->setPitch(audioSource.pitch);

		// Update 3D position for spatial audio
		if (audioSource.spatialBlend != 0.0f) {
			FMOD_VECTOR pos = { transform.localPosition.x, transform.localPosition.y, transform.localPosition.z };
			FMOD_VECTOR vel = { 0, 0, 0 };
			audioSource.m_channel->set3DAttributes(&pos, &vel);
		}

		// Check if sound finished playing
		bool isPlaying = false;
		audioSource.m_channel->isPlaying(&isPlaying);
		if (!isPlaying) {
			audioSource.isPlaying = false;
			audioSource.m_channel = nullptr;
		}
	}

	// create -> init -> load bank
	void AudioSystem::SetupStudioSystem()
	{
		// Already set up
		if (studioSystem != nullptr)
			return;

		FMOD_RESULT result = FMOD::Studio::System::create(&studioSystem);
		if (result != FMOD_OK) {
			SPD_ERROR("Failed to create FMOD Studio System: " << FMOD_ErrorString(result));
			return;
		}

		// initialize the system!
		result = studioSystem->initialize(
			1024, // max channels
			FMOD_STUDIO_INIT_NORMAL,
			FMOD_INIT_NORMAL,
			nullptr
		);
		if (result != FMOD_OK)
		{
			SPD_ERROR("FMOD init failed: " << FMOD_ErrorString(result));
			return;
		}

		SPD_INFO("FMOD Studio System initialized successfully");
	}

	void AudioSystem::PlaySound(const std::string& eventName)
	{
		if (!studioSystem) {
			SPD_ERROR("PlaySound failed: Studio system not initialized");
			return;
		}

		// Some audio property in studio that im not too familiar with yet
		FMOD::Studio::EventDescription* eventDesc = nullptr;
		FMOD_RESULT result = studioSystem->getEvent(eventName.c_str(), &eventDesc);

		if (result != FMOD_OK || eventDesc == nullptr)
		{
			SPD_ERROR("Failed to get event: " << eventName << " - " << FMOD_ErrorString(result));
			return;
		}

		FMOD::Studio::EventInstance* eventInstance = nullptr;
		result = eventDesc->createInstance(&eventInstance);

		if (result != FMOD_OK || eventInstance == nullptr)
		{
			SPD_ERROR("Failed to create eventInstance for: " << eventName << " - " << FMOD_ErrorString(result));
			return;
		}

		result = eventInstance->start();
		if (result != FMOD_OK) {
			SPD_ERROR("Failed to start event: " << eventName << " - " << FMOD_ErrorString(result));
			eventInstance->release();
			return;
		}

		// Optional: Release the instance after it finishes playing
		// eventInstance->release(); // Let FMOD handle cleanup
	}

	void AudioSystem::CleanupStudioSystem()
	{
		if (studioSystem) {
			// Unload all banks first
			for (auto& [path, bankPtr] : m_loadedBanks) {
				if (bankPtr) {
					bankPtr->Unload();
				}
			}
			m_loadedBanks.clear();

			studioSystem->release();
			studioSystem = nullptr;
		}
	}


	void AudioSystem::LoadBankAssets(const std::string& audioDirectory)
	{
		SPD_INFO("LoadBankAssets - Checking directory: " << audioDirectory);

		// Check if directory exists
		if (!std::filesystem::exists(audioDirectory)) {
			SPD_WARNING("Audio bank directory does not exist: " << audioDirectory);
			return;
		}

		if (!studioSystem) {
			SPD_ERROR("Cannot load banks: FMOD Studio System not initialized");
			return;
		}

		size_t banksLoaded = 0;

		try {
			// PASS 1: Load all .bank files (including strings banks)
			std::vector<std::string> bankPaths;
			std::vector<std::string> stringsBankPaths;

			for (const auto& entry : std::filesystem::directory_iterator(audioDirectory))
			{
				if (entry.path().extension() == ".bank")
				{
					std::string bankPath = entry.path().string();
					std::string filename = entry.path().filename().string();

					// Separate strings banks from regular banks
					if (filename.find(".strings.bank") != std::string::npos) {
						stringsBankPaths.push_back(bankPath);
					}
					else {
						bankPaths.push_back(bankPath);
					}
				}
			}

			// Load strings banks first (they must be loaded before main banks)
			for (const std::string& bankPath : stringsBankPaths)
			{
				FMOD::Studio::Bank* fmodBank = nullptr;
				FMOD_RESULT result = studioSystem->loadBankFile(
					bankPath.c_str(),
					FMOD_STUDIO_LOAD_BANK_NORMAL,
					&fmodBank
				);

				if (result == FMOD_OK && fmodBank != nullptr)
				{
					// Create AudioBank wrapper
					auto bankAsset = std::make_shared<NE::Asset::AudioBank>();
					bankAsset->SetFMODBank(fmodBank);

					// Store in our map
					m_loadedBanks[bankPath] = bankAsset;

					banksLoaded++;
					std::string displayName = std::filesystem::path(bankPath).stem().string();
					SPD_INFO("Loaded strings bank: " << displayName);
				}
				else
				{
					SPD_ERROR("Failed to load strings bank: " << bankPath << " - " << FMOD_ErrorString(result));
				}
			}

			// Now load main banks
			for (const std::string& bankPath : bankPaths)
			{
				FMOD::Studio::Bank* fmodBank = nullptr;
				FMOD_RESULT result = studioSystem->loadBankFile(
					bankPath.c_str(),
					FMOD_STUDIO_LOAD_BANK_NORMAL,
					&fmodBank
				);

				if (result == FMOD_OK && fmodBank != nullptr)
				{
					// Create AudioBank wrapper
					auto bankAsset = std::make_shared<NE::Asset::AudioBank>();
					bankAsset->SetFMODBank(fmodBank);

					// Extract events from this bank
					bankAsset->ExtractEvents(studioSystem);

					// Store in our map
					m_loadedBanks[bankPath] = bankAsset;

					banksLoaded++;
					std::string displayName = std::filesystem::path(bankPath).stem().string();
					SPD_INFO("Loaded bank: " << displayName << " with " << bankAsset->GetEvents().size() << " events");

					// Log all events in this bank
					for (const auto& [eventPath, eventInfo] : bankAsset->GetEvents()) {
						SPD_INFO("  - Event: " << eventPath);
					}
				}
				else
				{
					SPD_ERROR("Failed to load bank: " << bankPath << " - " << FMOD_ErrorString(result));
				}
			}

			SPD_INFO(banksLoaded << " bank file(s) loaded successfully");

		}
		catch (const std::exception& e) {
			SPD_ERROR("Error loading bank assets: " << e.what());
		}
	}

	AudioSystem::AudioSystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void AudioSystem::OnEntityAdded(Entity)
	{
	}

	void AudioSystem::OnEntityRemoved(Entity)
	{
	}

	void AudioSystem::Init()
	{
		SPD_INFO("AudioSystem::Init() - Starting initialization");

		// Get current directory as std::string
		std::string currentDir = std::filesystem::current_path().string();
		std::string bankDir = currentDir + "/Assets/Bank";
		SPD_INFO("Working directory: " << currentDir);
		SPD_INFO("Bank directory: " << bankDir);

		// Initialize FMOD Studio System first
		SetupStudioSystem();

		// Load all bank files from the directory
		LoadBankAssets(bankDir);

		// Test play a sound to verify everything works
		SPD_INFO("Testing audio playback...");
		//PlaySound("event:/ForestBGM");


		FMOD::Studio::Bus* masterBus = nullptr;
		FMOD_RESULT result = studioSystem->getBus("bus:/", &masterBus);
		if (result == FMOD_OK && masterBus != nullptr) {
			SPD_INFO("AudioSystem::Init() - Success get bus:/ (Master)");
		}
		else {
			SPD_ERROR("AudioSystem::Init() - Failed to get bus:/ (Master): " << FMOD_ErrorString(result));
		}

		FMOD::Studio::Bus* bgmBus = nullptr;
		result = studioSystem->getBus("bus:/BGM", &bgmBus);
		if (result == FMOD_OK && bgmBus != nullptr) {
			SPD_INFO("AudioSystem::Init() - Success get bus:/BGM " << FMOD_ErrorString(result));
		}
		else {
			SPD_ERROR("AudioSystem::Init() - Failed to get bus:/BGM: " << FMOD_ErrorString(result));
		}

		FMOD::Studio::Bus* sfxBus = nullptr;
		result = studioSystem->getBus("bus:/SFX", &sfxBus);
		if (result == FMOD_OK && sfxBus != nullptr) {
			SPD_INFO("AudioSystem::Init() - Success get bus:/SFX " << FMOD_ErrorString(result));
		}
		else {
			SPD_ERROR("AudioSystem::Init() - Failed to get bus:/SFX: " << FMOD_ErrorString(result));
		}

		FMOD::Studio::Bus* ambienceBus = nullptr;
		result = studioSystem->getBus("bus:/Ambience", &ambienceBus);
		if (result == FMOD_OK && ambienceBus != nullptr) {
			SPD_INFO("AudioSystem::Init() - Success get bus:/Ambience " << FMOD_ErrorString(result));
		}
		else {
			SPD_ERROR("AudioSystem::Init() - Failed to get bus:/Ambience: " << FMOD_ErrorString(result));
		}

		FMOD::Studio::Bus* dummyBus = nullptr;
		result = studioSystem->getBus("bus:/Dummy", &dummyBus);
		if (result == FMOD_OK && dummyBus != nullptr) {
			SPD_INFO("AudioSystem::Init() - Success get bus:/Dummy " << FMOD_ErrorString(result));
		}
		else {
			SPD_ERROR("AudioSystem::Init() - Failed to get bus:/Dummy: " << FMOD_ErrorString(result));
		}

		SPD_INFO("AudioSystem::Init() - Completed");
	}

	void AudioSystem::Update(double)
	{
		// Update FMOD Studio System
		if (studioSystem) {
			studioSystem->update();
		}

		// Legacy audio source processing (commented out for now)
		// Uncomment if you want to support old AudioSource component system
		/*
		NE_PROFILE_FUNCTION();
		const auto& entities = GetEntities();
		auto& engine = GetAudioEngine();

		for (Entity e : entities) {
			if (m_componentManager->HasComponent<Component::AudioSource>(e) &&
				m_componentManager->HasComponent<Component::Transform>(e)) {

				auto& audioSource = m_componentManager->GetComponent<Component::AudioSource>(e);
				auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

				ProcessAudioSource(audioSource, transform, engine.system);
			}
		}

		if (engine.system) {
			engine.system->update();
		}
		*/
	}

	void AudioSystem::Exit()
	{
		CleanupStudioSystem();
		SPD_INFO("AudioSystem shutdown");
	}

	std::unordered_map<std::string, NE::Asset::AudioBank::EventInfo> AudioSystem::GetAllEvents() const
	{
		std::unordered_map<std::string, NE::Asset::AudioBank::EventInfo> allEvents;

		// Collect events from all loaded banks
		for (const auto& [bankPath, bankPtr] : m_loadedBanks) {
			if (bankPtr && bankPtr->IsLoaded()) {
				const auto& bankEvents = bankPtr->GetEvents();
				allEvents.insert(bankEvents.begin(), bankEvents.end());
			}
		}

		return allEvents;
	}

	void AudioSystem::SetMasterVolume(float volume)
	{
		if (!studioSystem) {
			SPD_ERROR("Cannot set Master volume: AudioSystem not initialized");
			return;
		}

		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = studioSystem->getBus("bus:/", &bus);

		if (result != FMOD_OK || bus == nullptr) {
			// Try alternative path
			result = studioSystem->getBus("bus:/Master", &bus);
		}

		if (result == FMOD_OK && bus != nullptr) {
			volume = std::max(0.0f, std::min(1.0f, volume));
			bus->setVolume(volume);
			SPD_DEBUG("Set Master volume to " << volume);
		}
		else {
			SPD_ERROR("Cannot set Master volume: bus not found");
		}
	}

	float AudioSystem::GetMasterVolume() const
	{
		if (!studioSystem) {
			return -1.0f;
		}

		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = studioSystem->getBus("bus:/", &bus);

		if (result != FMOD_OK || bus == nullptr) {
			result = studioSystem->getBus("bus:/Master", &bus);
		}

		if (result == FMOD_OK && bus != nullptr) {
			float volume = 0.0f;
			bus->getVolume(&volume);
			return volume;
		}

		return -1.0f;
	}

	void AudioSystem::SetBGMVolume(float volume)
	{
		if (!studioSystem) {
			SPD_ERROR("Cannot set BGM volume: AudioSystem not initialized");
			return;
		}

		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = studioSystem->getBus("bus:/BGM", &bus);

		if (result == FMOD_OK && bus != nullptr) {
			volume = std::max(0.0f, std::min(1.0f, volume));
			bus->setVolume(volume);
			SPD_DEBUG("Set BGM volume to " << volume);
		}
		else {
			SPD_ERROR("Cannot set BGM volume: bus not found");
		}
	}

	float AudioSystem::GetBGMVolume() const
	{
		if (!studioSystem) {
			return -1.0f;
		}

		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = studioSystem->getBus("bus:/BGM", &bus);

		if (result == FMOD_OK && bus != nullptr) {
			float volume = 0.0f;
			bus->getVolume(&volume);
			return volume;
		}

		return -1.0f;
	}

	void AudioSystem::SetSFXVolume(float volume)
	{
		if (!studioSystem) {
			SPD_ERROR("Cannot set SFX volume: AudioSystem not initialized");
			return;
		}

		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = studioSystem->getBus("bus:/SFX", &bus);

		if (result == FMOD_OK && bus != nullptr) {
			volume = std::max(0.0f, std::min(1.0f, volume));
			bus->setVolume(volume);
			SPD_DEBUG("Set SFX volume to " << volume);
		}
		else {
			SPD_ERROR("Cannot set SFX volume: bus not found");
		}
	}

	float AudioSystem::GetSFXVolume() const
	{
		if (!studioSystem) {
			return -1.0f;
		}

		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = studioSystem->getBus("bus:/SFX", &bus);

		if (result == FMOD_OK && bus != nullptr) {
			float volume = 0.0f;
			bus->getVolume(&volume);
			return volume;
		}

		return -1.0f;
	}

	void AudioSystem::SetAmbienceVolume(float volume)
	{
		if (!studioSystem) {
			SPD_ERROR("Cannot set Ambience volume: AudioSystem not initialized");
			return;
		}

		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = studioSystem->getBus("bus:/Ambience", &bus);

		if (result == FMOD_OK && bus != nullptr) {
			volume = std::max(0.0f, std::min(1.0f, volume));
			bus->setVolume(volume);
			SPD_DEBUG("Set Ambience volume to " << volume);
		}
		else {
			SPD_ERROR("Cannot set Ambience volume: bus not found");
		}
	}

	float AudioSystem::GetAmbienceVolume() const
	{
		if (!studioSystem) {
			return -1.0f;
		}

		FMOD::Studio::Bus* bus = nullptr;
		FMOD_RESULT result = studioSystem->getBus("bus:/Ambience", &bus);

		if (result == FMOD_OK && bus != nullptr) {
			float volume = 0.0f;
			bus->getVolume(&volume);
			return volume;
		}

		return -1.0f;
	}

	// dun use
	void AudioSystem::ApplyMasterVolume()
	{
		// Map 0..5 -> 0.0..1.0
		const float v = std::clamp(static_cast<float>(m_masterVolumeLevel) / 5.0f, 0.0f, 1.0f);

		// Core (non-studio) sounds
		auto& engine = GetAudioEngine();
		if (engine.system) {
			FMOD::ChannelGroup* masterGroup = nullptr;
			if (engine.system->getMasterChannelGroup(&masterGroup) == FMOD_OK && masterGroup) {
				masterGroup->setVolume(v);
			}
		}

		// Studio events (FMOD Studio)
		if (studioSystem) {
			FMOD::Studio::Bus* masterBus = nullptr;
			if (studioSystem->getBus("bus:/", &masterBus) == FMOD_OK && masterBus) {
				masterBus->setVolume(v);
			}
		}
	}

	// dun use
	void AudioSystem::SetMasterVolumeLevel(int level)
	{
		m_masterVolumeLevel = std::clamp(level, 0, 5);
		ApplyMasterVolume();
	}


}