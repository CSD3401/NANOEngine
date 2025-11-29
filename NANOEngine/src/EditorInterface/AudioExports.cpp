#include "AudioExports.hpp"
#include "../Audio/AudioManager.hpp"

namespace NE::Audio {

	void PlayAudio(const std::string& eventName) {
		AudioManager::PlaySound(eventName);
	}

	void StopAudio(const std::string& eventName) {
		AudioManager::StopSound(eventName);
	}

	void StopAllAudio() {
		AudioManager::StopAllSounds();
	}

} // namespace NE::Audio