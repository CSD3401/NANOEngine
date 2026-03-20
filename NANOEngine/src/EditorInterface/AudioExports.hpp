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
	 * @brief Play an FMOD Studio event bound to an entity (supports 3D spatial audio)
	 * @param entity The entity whose world transform drives the 3D position
	 * @param eventName The event path (e.g., "event:/SFX/Footstep")
	 */
	NANOENGINE_API void PlayEntityAudio(uint32_t entity, const std::string& eventName);

	/**
	 * @brief Stop the FMOD Studio event currently playing on an entity
	 */
	NANOENGINE_API void StopEntityAudio(uint32_t entity);

	/**
	 * @brief Returns true if the entity currently has a Studio event instance playing
	 */
	NANOENGINE_API bool IsEntityAudioPlaying(uint32_t entity);

	/**
	 * @brief Stop all instances of an FMOD Studio event
	 * @param eventName The event path to stop
	 */
	NANOENGINE_API void StopAudio(const std::string& eventName);

	/**
 * @brief Stop all currently playing sounds
 */
	NANOENGINE_API void StopAllAudio();


	// Bus volume control
	NANOENGINE_API void SetMasterVolume(float volume);
	NANOENGINE_API void SetBGMVolume(float volume);
	NANOENGINE_API void SetSFXVolume(float volume);
	NANOENGINE_API void SetAmbienceVolume(float volume);

	NANOENGINE_API float GetMasterVolume();
	NANOENGINE_API float GetBGMVolume();
	NANOENGINE_API float GetSFXVolume();
	NANOENGINE_API float GetAmbienceVolume();


	NANOENGINE_API void SetMasterVolumeLevel(int level);
	NANOENGINE_API int GetMasterVolumeLevel();

} // namespace NE::Audio