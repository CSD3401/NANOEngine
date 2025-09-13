#pragma once

#include "../../../NANOEngine/ThirdParty/include/fmod/fmod.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

	// This is just a data container, AudioSource is the one playing audio
	struct AudioClip 
	{
		const char* filepath; // filepath of audio
		FMOD::Sound* sound = nullptr; // sound source of audio
		bool is3D = false;
		bool loop = false;
		float minDistance = 1.0f;
		float maxDistance = 1000.0f;
	};

	struct AudioSource {
        // Exposed
        Math::Vec3 position{ 0.f, 0.f, 0.f };
        //Math::Vec3 velocity{ 0.f, 0.f, 0.f }; // For Doppler effect
        float volume = 1.0f;
        float pitch = 1.0f;

        // Playback control
        bool playOnStart = false;
        bool isPlaying = false;
        bool isPaused = false;
        bool loop = false;

        // Internal
        bool hasPlayed = false; // to track is playOnStart was triggered
        FMOD::Channel* channel = nullptr; // For controlling playback

        NE_REFLECT_BEGIN(AudioSource)
            NE_REFLECT_FIELD_NAMED(volume, "Volume"),
            NE_REFLECT_FIELD_NAMED(pitch, "Pitch"),
            NE_REFLECT_FIELD_NAMED(playOnStart, "Play On Start"),
            NE_REFLECT_FIELD_NAMED(loop, "Loop")
            NE_REFLECT_END()
	};

}