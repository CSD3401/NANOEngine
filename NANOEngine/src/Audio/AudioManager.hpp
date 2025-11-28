#pragma once

#include <string>
#include <fmod/fmod_studio.hpp>


namespace NE {

	/**
	 * @class AudioManager
	 * @brief Singleton manager for FMOD Studio audio playback
	 *
	 * Simple, direct audio manager that loads FMOD banks and provides
	 * static methods to play/stop audio events from anywhere in the code.
	 *
	 * Usage:
	 *   AudioManager::GetInstance().Init("Assets/Bank");
	 *   AudioManager::PlaySound("event:/UI/ButtonClick");
	 *   AudioManager::StopSound("event:/Music/MainTheme");
	 */
	class AudioManager {
	public:
		// Singleton access
		static AudioManager& GetInstance();

		// Initialize FMOD and load banks from directory
		void Init(const std::string& bankDirectory);

		// Cleanup FMOD resources
		void Shutdown();

		// Update FMOD (call once per frame)
		void Update();

		// Play an FMOD Studio event by path
		static void PlaySound(const std::string& eventName);

		// Stop all instances of an FMOD Studio event
		static void StopSound(const std::string& eventName);

		// Check if audio system is initialized
		static bool IsInitialized();

	private:
		// Private constructor for singleton
		AudioManager();
		~AudioManager();

		// Delete copy constructor and assignment operator
		AudioManager(const AudioManager&) = delete;
		AudioManager& operator=(const AudioManager&) = delete;

		// Setup FMOD Studio System
		void SetupStudioSystem();

		// Load all .bank files from a directory
		void LoadBankAssets(const std::string& audioDirectory);

		// Cleanup FMOD Studio System
		void CleanupStudioSystem();

		// Debug
		void ListAllEvents();

		// Member variables
		FMOD::Studio::System* m_studioSystem;
		bool m_initialized;
	};

} // namespace NE