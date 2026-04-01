#include "pch.h"
// AudioSystem.cpp - FIXED VERSION
// This version properly loads FMOD banks using the new ResourceManager system

#include "AudioSystem.hpp"
#include "CameraSystem.hpp"
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
				system->init(512, FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED, nullptr);
				SPD_INFO("AudioEngine constructor called");
			}

			~AudioEngine()
			{
				// Shutdown() should be called explicitly from AudioSystem::Exit().
				// Calling close() during static destruction can deadlock FMOD -- do nothing.
				SPD_INFO("AudioEngine destructor (already shut down: " << (system == nullptr) << ")");
			}

			void Shutdown()
			{
				if (!system) return;
				SPD_INFO("AudioEngine::Shutdown - releasing sounds and closing FMOD core");
				for (auto& [path, sound] : loadedClips) {
					sound->release();
				}
				loadedClips.clear();
				system->close();
				system->release();
				system = nullptr;
			}

			FMOD::Sound* LoadSound(const std::string& filepath, bool loop = false, bool is3D = false) {
				std::string cacheKey = filepath + (is3D ? "|3D" : "|2D");
				if (loadedClips.find(cacheKey) != loadedClips.end()) {
					return loadedClips[cacheKey];
				}

				FMOD::Sound* sound = nullptr;
				FMOD_MODE mode = is3D ? FMOD_3D : FMOD_2D;
				if (loop) {
					mode |= FMOD_LOOP_NORMAL;
				}

				FMOD_RESULT result = system->createSound(filepath.c_str(), mode, nullptr, &sound);

				if (result == FMOD_OK) {
					loadedClips[cacheKey] = sound;
					return sound;
				}

				return nullptr;
			}

			void Set3DListener(const NE::Math::Vec3& pos, const NE::Math::Vec3& forward, const NE::Math::Vec3& up) {
				if (!system) return;
				FMOD_VECTOR fpos     = { pos.x,     pos.y,     pos.z };
				FMOD_VECTOR fvel     = { 0.f, 0.f, 0.f };
				FMOD_VECTOR fforward = { forward.x, forward.y, forward.z };
				FMOD_VECTOR fup      = { up.x,      up.y,      up.z };
				system->set3DListenerAttributes(0, &fpos, &fvel, &fforward, &fup);
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
			audioSource.m_sound = GetAudioEngine().LoadSound(audioSource.audioClipPath, audioSource.loop, audioSource.spatialBlend > 0.0f);
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
		const NE::ECS::Component::Transform& transform,
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

			// Set 2D/3D blend (0=pure 2D, 1=pure 3D) and spatial attributes
			channel->set3DLevel(audioSource.spatialBlend);
			if (audioSource.spatialBlend > 0.0f) {
				NE::Math::Vec3 worldPos = transform.worldMatrix.GetTranslation();
				FMOD_VECTOR fpos = { worldPos.x, worldPos.y, worldPos.z };
				FMOD_VECTOR fvel = { 0.f, 0.f, 0.f };
				channel->set3DAttributes(&fpos, &fvel);
				channel->set3DMinMaxDistance(audioSource.minDist, audioSource.maxDist);
			}
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
			NE::Math::Vec3 worldPos = transform.worldMatrix.GetTranslation();
		FMOD_VECTOR pos = { worldPos.x, worldPos.y, worldPos.z };
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
			FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED,
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
					//for (const auto& [eventPath, eventInfo] : bankAsset->GetEvents()) {
					//	SPD_INFO("  - Event: " << eventPath);
					//}
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

	void AudioSystem::OnEntityRemoved(Entity e)
	{
		StopEntitySound(e);
	}

	void AudioSystem::OnEntityActive(Entity /*entity*/) {}
	void AudioSystem::OnEntityInactive(Entity /*entity*/) {}

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

		// Update FMOD listener from main camera
		if (m_cameraSystem) {
			auto mainCam = m_cameraSystem->GetMainCameraEntity();
			if (mainCam.has_value() && m_componentManager->HasComponent<Component::Transform>(*mainCam)) {
				const auto& camTransform = m_componentManager->GetComponent<Component::Transform>(*mainCam);
				const NE::Math::Vec3 pos     = camTransform.worldMatrix.GetTranslation();
				const NE::Math::Vec3 forward = camTransform.worldMatrix.Forward();
				const NE::Math::Vec3 up      = camTransform.worldMatrix.Up();

				GetAudioEngine().Set3DListener(pos, forward, up);

				if (studioSystem) {
					FMOD_3D_ATTRIBUTES attrs = {};
					attrs.position = { pos.x,     pos.y,     pos.z };
					attrs.velocity = { 0.f, 0.f, 0.f };
					attrs.forward  = { forward.x, forward.y, forward.z };
					attrs.up       = { up.x,      up.y,      up.z };
					studioSystem->setListenerAttributes(0, &attrs, nullptr);
				}
			}
		}

		// Per-entity AudioSource processing
		NE_PROFILE_FUNCTION();
		const auto& entities = GetEntities();
		auto& engine = GetAudioEngine();

		for (Entity e : entities) {
			auto& audioSource = m_componentManager->GetComponent<Component::AudioSource>(e);
			const auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

			// Studio event path: audioClipPath starting with "event:" uses per-entity instance tracking
			if (!audioSource.audioClipPath.empty() && audioSource.audioClipPath.rfind("event:", 0) == 0) {
				if (audioSource.playOnAwake && !audioSource.m_hasPlayed) {
					PlayEntitySound(e, audioSource.audioClipPath, transform);
					audioSource.m_hasPlayed = true;
				}
				audioSource.isPlaying = IsEntitySoundPlaying(e);
				continue;
			}

			// Legacy core path: raw audio file (.wav, .mp3, etc.)
			ProcessAudioSource(audioSource, transform, engine.system);
		}

		if (engine.system) {
			engine.system->update();
		}

		// Update 3D attributes and clean up finished Studio entity instances
		for (auto it = m_entityInstances.begin(); it != m_entityInstances.end(); ) {
			FMOD::Studio::EventInstance* inst = it->second;
			if (!inst) { it = m_entityInstances.erase(it); continue; }

			FMOD_STUDIO_PLAYBACK_STATE state;
			inst->getPlaybackState(&state);
			if (state == FMOD_STUDIO_PLAYBACK_STOPPED) {
				inst->release();
				if (m_componentManager->HasComponent<Component::AudioSource>(it->first))
					m_componentManager->GetComponent<Component::AudioSource>(it->first).isPlaying = false;
				it = m_entityInstances.erase(it);
				continue;
			}

			// Update 3D position each frame
			if (m_componentManager->HasComponent<Component::Transform>(it->first)) {
				const auto& t = m_componentManager->GetComponent<Component::Transform>(it->first);
				const NE::Math::Vec3 pos = t.worldMatrix.GetTranslation();
				const NE::Math::Vec3 fwd = t.worldMatrix.Forward();
				const NE::Math::Vec3 up  = t.worldMatrix.Up();
				FMOD_3D_ATTRIBUTES attrs = {};
				attrs.position = { pos.x, pos.y, pos.z };
				attrs.velocity = { 0.f, 0.f, 0.f };
				attrs.forward  = { fwd.x, fwd.y, fwd.z };
				attrs.up       = { up.x,  up.y,  up.z };
				inst->set3DAttributes(&attrs);
			}
			++it;
		}
	}

	void AudioSystem::Exit()
	{
		// Stop and release all per-entity Studio instances
		for (auto& [e, inst] : m_entityInstances) {
			if (inst) {
				inst->stop(FMOD_STUDIO_STOP_IMMEDIATE);
				inst->release();
			}
		}
		m_entityInstances.clear();

		GetAudioEngine().Shutdown();
		CleanupStudioSystem();
		SPD_INFO("AudioSystem shutdown");
	}

	void AudioSystem::PlayEntitySound(Entity e, const std::string& eventPath, const Component::Transform& transform)
	{
		if (!studioSystem) return;

		StopEntitySound(e); // release any existing instance for this entity

		FMOD::Studio::EventDescription* eventDesc = nullptr;
		if (studioSystem->getEvent(eventPath.c_str(), &eventDesc) != FMOD_OK || !eventDesc) {
			SPD_ERROR("[Audio] PlayEntitySound: event not found: " << eventPath);
			return;
		}

		FMOD::Studio::EventInstance* instance = nullptr;
		if (eventDesc->createInstance(&instance) != FMOD_OK || !instance) {
			SPD_ERROR("[Audio] PlayEntitySound: failed to create instance for: " << eventPath);
			return;
		}

		// Set initial 3D attributes before start to avoid a position pop on the first frame
		bool is3D = false;
		eventDesc->is3D(&is3D);
		if (is3D) {
			const NE::Math::Vec3 pos = transform.worldMatrix.GetTranslation();
			const NE::Math::Vec3 fwd = transform.worldMatrix.Forward();
			const NE::Math::Vec3 up  = transform.worldMatrix.Up();
			FMOD_3D_ATTRIBUTES attrs = {};
			attrs.position = { pos.x, pos.y, pos.z };
			attrs.velocity = { 0.f, 0.f, 0.f };
			attrs.forward  = { fwd.x, fwd.y, fwd.z };
			attrs.up       = { up.x,  up.y,  up.z };
			instance->set3DAttributes(&attrs);
		}

		if (instance->start() != FMOD_OK) {
			SPD_ERROR("[Audio] PlayEntitySound: failed to start: " << eventPath);
			instance->release();
			return;
		}

		m_entityInstances[e] = instance;
	}

	void AudioSystem::StopEntitySound(Entity e)
	{
		auto it = m_entityInstances.find(e);
		if (it != m_entityInstances.end()) {
			if (it->second) {
				it->second->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
				it->second->release();
			}
			m_entityInstances.erase(it);
		}
	}

	bool AudioSystem::IsEntitySoundPlaying(Entity e) const
	{
		auto it = m_entityInstances.find(e);
		if (it == m_entityInstances.end() || !it->second) return false;
		FMOD_STUDIO_PLAYBACK_STATE state;
		it->second->getPlaybackState(&state);
		return state != FMOD_STUDIO_PLAYBACK_STOPPED;
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