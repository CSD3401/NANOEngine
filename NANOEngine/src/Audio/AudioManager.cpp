#include "AudioManager.hpp"
#include "Core/SpdLogger.hpp"
#include <fmod/fmod_errors.h>
#include <filesystem>

namespace NE {

	AudioManager::AudioManager()
		: m_studioSystem(nullptr)
		, m_initialized(false)
	{
	}

	AudioManager::~AudioManager()
	{
		Shutdown();
	}

	AudioManager& AudioManager::GetInstance()
	{
		static AudioManager instance;
		return instance;
	}

	void AudioManager::Init(const std::string& bankDirectory)
	{
		if (m_initialized) {
			SPD_WARNING("AudioManager already initialized");
			return;
		}

		SPD_INFO("AudioManager::Init() - Starting initialization");

		// Setup FMOD Studio System
		SetupStudioSystem();

		// Load banks from directory
		LoadBankAssets(bankDirectory);

		m_initialized = true;

		ListAllEvents();

		SPD_INFO("AudioManager::Init() - Completed");

		//PlaySound("event:/OnClick");
	}

	void AudioManager::Shutdown()
	{
		if (!m_initialized) {
			return;
		}

		SPD_INFO("AudioManager::Shutdown() - Cleaning up");
		CleanupStudioSystem();
		m_initialized = false;
	}

	void AudioManager::Update()
	{
		if (m_studioSystem && m_initialized) {
			m_studioSystem->update();
		}
	}

	void AudioManager::PlaySound(const std::string& eventName)
	{
		auto& instance = GetInstance();

		if (!instance.m_initialized || !instance.m_studioSystem) {
			SPD_ERROR("AudioManager not initialized - cannot play sound: " << eventName);
			return;
		}

		// Get event description
		FMOD::Studio::EventDescription* eventDesc = nullptr;
		FMOD_RESULT result = instance.m_studioSystem->getEvent(eventName.c_str(), &eventDesc);

		if (result != FMOD_OK || eventDesc == nullptr) {
			SPD_ERROR("Failed to get event: " << eventName << " - " << FMOD_ErrorString(result));
			return;
		}

		// Create and start event instance
		FMOD::Studio::EventInstance* eventInstance = nullptr;
		result = eventDesc->createInstance(&eventInstance);

		if (result != FMOD_OK || eventInstance == nullptr) {
			SPD_ERROR("Failed to create event instance for: " << eventName);
			return;
		}

		result = eventInstance->start();
		if (result != FMOD_OK) {
			SPD_ERROR("Failed to start event: " << eventName << " - " << FMOD_ErrorString(result));
		}

		// Note: Event instance will be automatically released by FMOD when it finishes playing
		// For one-shot sounds this is fine. For looping sounds, you'd want to track the instance.
	}

	void AudioManager::StopSound(const std::string& eventName)
	{
		auto& instance = GetInstance();

		if (!instance.m_initialized || !instance.m_studioSystem) {
			SPD_WARNING("AudioManager not initialized - cannot stop sound: " << eventName);
			return;
		}

		// Get event description
		FMOD::Studio::EventDescription* eventDesc = nullptr;
		FMOD_RESULT result = instance.m_studioSystem->getEvent(eventName.c_str(), &eventDesc);

		if (result != FMOD_OK || eventDesc == nullptr) {
			SPD_WARNING("Failed to get event for stopping: " << eventName);
			return;
		}

		// Get all instances of this event and stop them
		int instanceCount = 0;
		eventDesc->getInstanceCount(&instanceCount);

		if (instanceCount > 0) {
			std::vector<FMOD::Studio::EventInstance*> instances(instanceCount);
			eventDesc->getInstanceList(instances.data(), instanceCount, &instanceCount);

			for (int i = 0; i < instanceCount; ++i) {
				instances[i]->stop(FMOD_STUDIO_STOP_IMMEDIATE);
			}

			SPD_INFO("Stopped " << instanceCount << " instance(s) of: " << eventName);
		}
	}

	bool AudioManager::IsInitialized()
	{
		return GetInstance().m_initialized;
	}

	void AudioManager::SetupStudioSystem()
	{
		// Already set up
		if (m_studioSystem != nullptr) {
			SPD_WARNING("FMOD Studio System already created");
			return;
		}

		// Create FMOD Studio System
		FMOD_RESULT result = FMOD::Studio::System::create(&m_studioSystem);
		if (result != FMOD_OK) {
			SPD_ERROR("Failed to create FMOD Studio System: " << FMOD_ErrorString(result));
			return;
		}

		// Initialize the system
		result = m_studioSystem->initialize(
			1024,                        // max channels
			FMOD_STUDIO_INIT_NORMAL,     // studio flags
			FMOD_INIT_NORMAL,            // core flags
			nullptr                      // extra driver data
		);

		if (result != FMOD_OK) {
			SPD_ERROR("FMOD Studio System init failed: " << FMOD_ErrorString(result));
			m_studioSystem->release();
			m_studioSystem = nullptr;
			return;
		}

		SPD_INFO("FMOD Studio System initialized successfully");
	}

	void AudioManager::LoadBankAssets(const std::string& audioDirectory)
	{
		SPD_INFO("LoadBankAssets - Checking directory: " << audioDirectory);

		// Check if directory exists
		if (!std::filesystem::exists(audioDirectory)) {
			SPD_WARNING("Audio bank directory does not exist: " << audioDirectory);
			return;
		}

		// Make sure studio system is created first
		if (m_studioSystem == nullptr) {
			SPD_ERROR("Studio system must be set up before loading banks");
			return;
		}

		size_t banksLoaded = 0;

		try {
			// Load banks directly using FMOD Studio System
			for (const auto& entry : std::filesystem::directory_iterator(audioDirectory))
			{
				if (entry.path().extension() == ".bank")
				{
					std::string bankPath = entry.path().string();
					FMOD::Studio::Bank* fmodBank = nullptr;

					FMOD_RESULT result = m_studioSystem->loadBankFile(
						bankPath.c_str(),
						FMOD_STUDIO_LOAD_BANK_NORMAL,
						&fmodBank
					);

					if (result == FMOD_OK && fmodBank != nullptr)
					{
						banksLoaded++;
						std::string bankName = entry.path().filename().string();
						SPD_INFO("Loaded bank: " << bankName << " from " << bankPath);
					}
					else
					{
						SPD_ERROR("Failed to load bank: " << bankPath << " - " << FMOD_ErrorString(result));
					}
				}
			}
			SPD_INFO(banksLoaded << " bank(s) loaded successfully");
		}
		catch (const std::exception& e) {
			SPD_ERROR("Error loading bank assets: " << e.what());
		}
	}

	void AudioManager::ListAllEvents()
	{
		if (!m_studioSystem) return;

		// Get all loaded banks
		int bankCount = 0;
		m_studioSystem->getBankCount(&bankCount);

		std::vector<FMOD::Studio::Bank*> banks(bankCount);
		m_studioSystem->getBankList(banks.data(), bankCount, &bankCount);

		SPD_INFO("=== Listing all events from " << bankCount << " banks ===");

		for (auto* bank : banks) {
			if (!bank) continue;

			// Get bank name
			char bankName[256];
			bank->getPath(bankName, 256, nullptr);

			// Get event count
			int eventCount = 0;
			bank->getEventCount(&eventCount);

			if (eventCount > 0) {
				std::vector<FMOD::Studio::EventDescription*> events(eventCount);
				bank->getEventList(events.data(), eventCount, &eventCount);

				SPD_INFO("Bank: " << bankName << " has " << eventCount << " events:");

				for (int i = 0; i < eventCount; ++i) {
					char eventPath[256];
					events[i]->getPath(eventPath, 256, nullptr);
					SPD_INFO("  - " << eventPath);
				}
			}
		}
	}

	void AudioManager::CleanupStudioSystem()
	{
		if (m_studioSystem) {
			m_studioSystem->release();
			m_studioSystem = nullptr;
			SPD_INFO("FMOD Studio System released");
		}
	}

} // namespace NE