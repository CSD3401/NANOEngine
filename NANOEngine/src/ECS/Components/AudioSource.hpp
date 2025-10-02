#pragma once

#include <fmod/fmod.hpp>
#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"
#include "../../Core/Reflection.hpp"
#include <filesystem>

namespace NE::ECS::Component {

	// This is just a data container, AudioSource is the one playing audio
	//struct AudioClip 
	//{
	//	const char* filepath; // filepath of audio
	//	FMOD::Sound* sound = nullptr; // sound source of audio
	//	bool is3D = false;
	//	bool loop = false;
	//	float minDistance = 1.0f;
	//	float maxDistance = 1000.0f;
	//};

	struct AudioSource {
        // Exposed
    
        // Audio clip reference liKe Unity
        std::filesystem::path modelPath;
        std::string audioClipPath;

        // Properties
        float volume = 1.0f;
        float pitch = 1.0f;
        bool playOnAwake = false;
        bool loop = false;
        bool spatialBlend = 0.0f; // 0 - 2d, 1 - 3d not in use now

        // 3D Properties
        float minDist = 1.0f;
        float maxDist = 500.0f;

        // State
        bool isPlaying = false;
        bool isPaused = false;

        // Internal
        bool hasPlayed = false; // to track is playOnStart was triggered
        FMOD::Channel* channel = nullptr; // For controlling playback

        NE_REFLECT_BEGIN(AudioSource)
            NE_REFLECT_FIELD_NAMED(audioClipPath, "Audio Clip"),
            NE_REFLECT_FIELD_NAMED(volume, "Volume"),
            NE_REFLECT_FIELD_NAMED(pitch, "Pitch"),
            NE_REFLECT_FIELD_NAMED(playOnAwake, "Play On Awake"),
            NE_REFLECT_FIELD_NAMED(loop, "Loop"),
            NE_REFLECT_FIELD_NAMED(spatialBlend, "Spatial Blend"),
            NE_REFLECT_FIELD_NAMED(minDist, "Min Distance"),
            NE_REFLECT_FIELD_NAMED(maxDist, "Max Distance")
            NE_REFLECT_END()

    //private:
        // FMOD Properties
        FMOD::Sound* m_sound = nullptr;
        FMOD::Channel* m_channel = nullptr;
        bool m_hasPlayed = false;
        //friend class Systems::AudioSystem; // Allow AudioSystem access private
	};

}