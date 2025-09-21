#include "AudioSystem.hpp"
#include "../Components/AudioSource.hpp"
#include "../Components/Transform.hpp"
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
		FMOD::System* system)
	{
		if (!audioSource.m_channel) return;

		// Update volume and pitch
		audioSource.m_channel->setVolume(audioSource.volume);
		audioSource.m_channel->setPitch(audioSource.pitch);

		// Update 3D position for spatial audio
		if (audioSource.spatialBlend > 0.0f) {
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
		std::cout << "Audio Sytem Init Start" << std::endl;
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

		// // Create a test sound
		//FMOD::Sound* testSound = nullptr;
		//FMOD_RESULT result = engine.system->createSound(
		//	"ForestAmbienceLOOP.wav",  // Change to your actual test file path
		//	FMOD_DEFAULT,
		//	0,
		//	&testSound
		//);

		//if (result != FMOD_OK || !testSound) {
		//	std::cout << "Failed to create test sound!" << std::endl;
		//	return;
		//}

		//// Play the test sound
		//FMOD::Channel* channel = nullptr;
		//result = engine.system->playSound(testSound, 0, false, &channel);

		//if (result == FMOD_OK) {
		//	std::cout << "Test sound playing successfully!" << std::endl;

		//	// Wait a bit for the sound to play (optional)
		//	bool isPlaying = true;
		//	while (isPlaying) {
		//		engine.system->update();
		//		if (channel) {
		//			channel->isPlaying(&isPlaying);
		//		}
		//		// Small delay to prevent busy waiting
		//		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
		//	}

		//	std::cout << "Test sound finished playing" << std::endl;
		//}
		//else {
		//	std::cout << "Failed to play test sound!" << std::endl;
		//}

		//// Cleanup
		//testSound->release();
	}

	void AudioSystem::Update(double)
	{
		if (this == nullptr) 
			return;

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
	}

	void AudioSystem::Exit()
	{
		// AudioEngine cleanup is automatic (RAII)
		std::cout << "AudioSystem shutdown" << std::endl;
	}

	
}

