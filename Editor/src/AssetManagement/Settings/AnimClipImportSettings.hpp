#pragma once

#include <string>
#include <vector>

#include <Core/Reflection.hpp>
#include <Animation/AnimationClip.hpp>

namespace Editor::Assets {
	struct AnimClipImportSettings {
		std::string name;
		float lengthSeconds = 0;
		bool looping = true;
		std::vector<NE::Animation::AnimTrack> tracks;

		NE_REFLECT_BEGIN(AnimClipImportSettings)
			NE_REFLECT_FIELD(name),
			NE_REFLECT_FIELD(lengthSeconds),
			NE_REFLECT_FIELD(looping),
			NE_REFLECT_FIELD(tracks)
			NE_REFLECT_END()
	};
}