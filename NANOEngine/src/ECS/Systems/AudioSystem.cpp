#include "AudioSystem.hpp"
#include "../Components/AudioSource.hpp"
#include "../../Core/Profiler.hpp"
#include "../../src/EngineState.hpp"

#include <iostream>
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
			}

			~AudioEngine() 
			{
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

	void AudioSystem::help()
	{

	}

	// helper function, idk why stuff gets error when i put it as variable
	// / function in the hpp
	void AudioSystem::ProcessAudioSource(
		NE::ECS::Component::AudioSource& source,
		NE::ECS::Component::AudioClip& clip,
		FMOD::System* system)
	{
		// Load the sound if not loaded	
		if (clip.filepath && !clip.sound) {
			clip.sound = GetAudioEngine().LoadSound(clip.filepath, source.loop);
		}

		// Handle playOnStart
		if (source.playOnStart && !source.hasPlayed && clip.sound) {
			PlayAudio(source, clip, system);
			source.hasPlayed = true;
		}

		// Update existing playback
		if (source.isPlaying && source.channel) {
			UpdateAudioPlayback(source, clip, system);
		}
	}

	void AudioSystem::PlayAudio(
		NE::ECS::Component::AudioSource& source,
		NE::ECS::Component::AudioClip& clip,
		FMOD::System* system)
	{
		if (!clip.sound) return;

		// Stop previous playback if any
		if (source.channel) {
			source.channel->stop();
		}

		// Play the sound
		FMOD::Channel* channel = nullptr;
		FMOD_RESULT result = system->playSound(clip.sound, 0, false, &channel);

		if (result == FMOD_OK && channel) {
			source.channel = channel;
			source.isPlaying = true;
			source.isPaused = false;

			// Set initial properties
			channel->setVolume(source.volume);
			channel->setPitch(source.pitch);

			// Set 3D position if needed
			FMOD_VECTOR pos = {
				source.position.x,
				source.position.y,
				source.position.z
			};
			channel->set3DAttributes(&pos, nullptr);
		}
	}

	void AudioSystem::UpdateAudioPlayback(
		NE::ECS::Component::AudioSource& source,
		NE::ECS::Component::AudioClip& clip,
		FMOD::System* system)
	{
		if (!source.channel) return;

		// Update volume and pitch
		source.channel->setVolume(source.volume);
		source.channel->setPitch(source.pitch);

		// Update 3D position
		FMOD_VECTOR pos = {
			source.position.x,
			source.position.y,
			source.position.z
		};
		source.channel->set3DAttributes(&pos, nullptr);

		// Check if sound finished playing
		bool isPlaying = false;
		source.channel->isPlaying(&isPlaying);
		if (!isPlaying) {
			source.isPlaying = false;
			source.channel = nullptr;
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
		// TODO: remove parenting and stuff
	}

	void AudioSystem::Init()
	{
		// AudioEngine is automatically initialized when first accessed
		auto& engine = GetAudioEngine();

		// Basic test: Check if FMOD system was created successfully
		if (engine.system) {
			std::cout << "FMOD system created successfully!" << std::endl;

			// Get FMOD version to verify it's working
			unsigned int version;
			FMOD_RESULT result = engine.system->getVersion(&version);

			if (result == FMOD_OK) {
				std::cout << "FMOD version: " << version << std::endl;
				std::cout << "Audio system is working!" << std::endl;
			}
		}
		else {
			std::cout << "FMOD system creation failed!" << std::endl;
		}



		/// TEST PLAY SOUND

		 // Create a test sound
		FMOD::Sound* testSound = nullptr;
		FMOD_RESULT result = engine.system->createSound(
			"ForestAmbienceLOOP.wav",  // Change to your actual test file path
			FMOD_DEFAULT,
			0,
			&testSound
		);

		if (result != FMOD_OK || !testSound) {
			std::cout << "Failed to create test sound!" << std::endl;
			return;
		}

		// Play the test sound
		FMOD::Channel* channel = nullptr;
		result = engine.system->playSound(testSound, 0, false, &channel);

		if (result == FMOD_OK) {
			std::cout << "Test sound playing successfully!" << std::endl;

			// Wait a bit for the sound to play (optional)
			bool isPlaying = true;
			while (isPlaying) {
				engine.system->update();
				if (channel) {
					channel->isPlaying(&isPlaying);
				}
				// Small delay to prevent busy waiting
				//std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			std::cout << "Test sound finished playing" << std::endl;
		}
		else {
			std::cout << "Failed to play test sound!" << std::endl;
		}

		// Cleanup
		testSound->release();
	}

	void AudioSystem::Update(double)
	{
		if (this == nullptr) return;

		NE_PROFILE_FUNCTION();
		const auto& entities = GetEntities();
		auto& engine = GetAudioEngine();

		for (Entity e : entities) {
			if (m_componentManager->HasComponent<Component::AudioSource>(e) &&
				m_componentManager->HasComponent<Component::AudioClip>(e)) {

				auto& source = m_componentManager->GetComponent<Component::AudioSource>(e);
				auto& clip = m_componentManager->GetComponent<Component::AudioClip>(e);

				ProcessAudioSource(source, clip, engine.system);
			}
		}

		if (engine.system) {
			engine.system->update();
		}
	}

	void AudioSystem::Exit()
	{
		// AudioEngine cleanup is automatic (RAII)
		std::cout << "AudioSystem shutdown" << std::endl;
	}

	
}

