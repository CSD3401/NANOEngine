#pragma once
#include <string>
#include "../NANOEngineAPI.hpp"

namespace NE::Audio {

	/**
	 * @brief Play an FMOD Studio event
	 * @param eventName The event path (e.g., "event:/UI/ButtonClick")
	 */
	NANOENGINE_API void PlayAudio(const std::string& eventName);

	/**
	 * @brief Stop all instances of an FMOD Studio event
	 * @param eventName The event path to stop
	 */
	NANOENGINE_API void StopAudio(const std::string& eventName);

} // namespace NE::Audio