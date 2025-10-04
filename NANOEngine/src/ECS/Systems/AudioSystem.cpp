#include "AudioSystem.hpp"
#include "../Components/AudioSource.hpp"
#include "../Components/Transform.hpp"
#include "../../Core/Profiler.hpp"
#include "../../src/EngineState.hpp"
#include <fmod/fmod_errors.h>


#include <iostream>
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
				std::cout << "AudioEngine constructor called \n";
			}

			~AudioEngine() 
			{
				std::cout << "AudioEngine destructor called \n";
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


	const std::vector<std::pair<std::string, std::shared_ptr<NE::Asset::AudioBank>>>& AudioSystem::GetLoadedBanks() const 
	{
		auto& assetManager = NE::Asset::AssetManager::GetInstance();
		return assetManager.GetAssetsOfType<NE::Asset::AudioBank>();
	}

	std::unordered_map<std::string, NE::Asset::AudioBank::EventInfo> AudioSystem::GetAllEvents() const 
	{
		std::unordered_map<std::string, NE::Asset::AudioBank::EventInfo> allEvents;

		auto banks = GetLoadedBanks(); // This now returns the correct type

		for (const auto& [uuid, bank] : banks) {
			const auto& bankEvents = bank->GetEvents();
			allEvents.insert(bankEvents.begin(), bankEvents.end());
		}

		return allEvents;
	}

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
		(void)transform;

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
			//	FMOD_VECTOR vel = { 0, 0,  ⁠0 }; // Zero velocity for now
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
		(void)system;

		if (!audioSource.m_channel) return;

		// Update volume and pitch
		audioSource.m_channel->setVolume(audioSource.volume);
		audioSource.m_channel->setPitch(audioSource.pitch);

		// Update 3D position for spatial audio
		if (audioSource.spatialBlend != false) {
			FMOD_VECTOR pos = { transform.position.x, transform.position.y, transform.position.z };
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
		if (result != FMOD_OK)
			std::cout << "Failed to create FMOD Studio System " << FMOD_ErrorString(result) << std::endl;

		// initialize the system!
		result = studioSystem->initialize(
			1024, // max channels
			FMOD_STUDIO_INIT_NORMAL,
			FMOD_INIT_NORMAL,
			nullptr
		);
		if (result != FMOD_OK) 
		{
			std::cout << "FMOD init failed: " << FMOD_ErrorString(result) << std::endl;
			return;
		}

		// Prepare for Extraction
		auto banks = GetLoadedBanks();

		std::cout << "Loading " << banks.size() << " banks into FMOD..." << std::endl;

		// First pass: Load all .bank files
		for (const auto& [uuid, bank] : banks)
		{
			FMOD::Studio::Bank* fmodBank = nullptr;
			FMOD_RESULT bankResult = studioSystem->loadBankFile(
				bank->filePath.c_str(),
				FMOD_STUDIO_LOAD_BANK_NORMAL,
				&fmodBank
			);

			if (bankResult == FMOD_OK && fmodBank != nullptr) 
			{
				bank->SetFMODBank(fmodBank);
				std::cout << "Loaded bank: " << bank->GetDisplayName() << std::endl;
			}
		}

		// SECOND PASS: Look for and load the strings bank specifically
		std::cout << "Looking for strings bank..." << std::endl;
		for (const auto& [uuid, bank] : banks) 
		{
			std::string bankName = bank->GetDisplayName();

			// Check if this is a strings bank (usually ends with .strings)
			if (bankName.find("strings") != std::string::npos ||
				bank->filePath.find("strings") != std::string::npos) {

				std::cout << "Loading strings bank: " << bankName << std::endl;

				// The strings bank should already be loaded from first pass, but make sure
				// it's processed for string data
				FMOD::Studio::Bank* fmodBank = bank->GetFMODBank();
				if (fmodBank) 
				{
					// Strings bank is automatically used by FMOD once loaded
					std::cout << "Strings bank ready: " << bankName << std::endl;
				}
			}
		}

		// Note: This step does not work if there is invalid bank or string bank
		// THIRD PASS: Now extract events (after strings bank is loaded)
		std::cout << "Extracting events from all banks..." << std::endl;
		for (const auto& [uuid, bank] : banks) 
		{
			// Skip strings banks for event extraction
			if (bank->GetDisplayName().find("strings") != std::string::npos) 
			{
				continue;
			}

			bank->ExtractEvents(studioSystem);
		}

	}

	void AudioSystem::PlaySound(const std::string& eventName)
	{
		// Some audio property in studio that im not too familiar with yet
		FMOD::Studio::EventDescription* eventDesc = nullptr;
		studioSystem->getEvent(eventName.c_str(), &eventDesc);

		if (eventDesc == nullptr) 
		{
			std::cout << "Failed to get event: " << eventName << std::endl;
			return;
		}

		FMOD::Studio::EventInstance* eventInstance = nullptr;
		eventDesc->createInstance(&eventInstance);

		if (eventInstance == nullptr)
		{
			std::cout << "Failed to create eventInstance" << std::endl;
			return;
		}

		eventInstance->start();	
	}

	void AudioSystem::CleanupStudioSystem()
	{
		studioSystem->release();
	}


	void AudioSystem::LoadBankAssets(const std::string& audioDirectory)
	{
		auto& assetManager = Asset::AssetManager::GetInstance();
		size_t banksLoaded = 0;
		for (const auto& entry : std::filesystem::directory_iterator(audioDirectory))
		{
			if (entry.path().extension() == ".bank")
			{
				// Load thru AssetManager (this creates AudioBank assets)
				std::string bankPath = entry.path().string();
				auto bankAsset = assetManager.Load<NE::Asset::AudioBank>(bankPath, false);
				if (bankAsset)
				{
					banksLoaded++;
					std::cout << "Loaded bank asset: " << bankAsset->GetDisplayName() << std::endl;
				}
			}
		}
		std::cout << banksLoaded << " banksLoaded" << std::endl;
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
		// Get current directory as std::string
		std::string currentDir = std::filesystem::current_path().string();
		std::string bankDir = currentDir + "/Assets/Bank";
		//std::cout << "Target Bank Dir: " << bankDir << std::endl;

		LoadBankAssets(bankDir);
		SetupStudioSystem();
		PlaySound("event:/OnClick");

		return;
	}

	void AudioSystem::Update(double)
	{

		studioSystem->update();
		return; // end of studio testing stuff

		//if (this == nullptr) 
		//	return;

		//NE_PROFILE_FUNCTION();
		//const auto& entities = GetEntities();
		//auto& engine = GetAudioEngine();

		//for (Entity e : entities) {
		//	if (m_componentManager->HasComponent<Component::AudioSource>(e) &&
		//		m_componentManager->HasComponent<Component::Transform>(e)) {

		//		auto& audioSource = m_componentManager->GetComponent<Component::AudioSource>(e);
		//		auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

		//		ProcessAudioSource(audioSource, transform, engine.system);
		//	}
		//}

		//if (engine.system) {
		//	engine.system->update();
		//}
	}

	void AudioSystem::Exit()
	{
		CleanupStudioSystem();
		std::cout << "AudioSystem shutdown" << std::endl;
	}

	
}

