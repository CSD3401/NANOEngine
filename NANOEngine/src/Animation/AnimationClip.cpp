#include "pch.h"
#include "AnimationClip.hpp"

#include <algorithm>

#include "Core/SpdLogger.hpp"
#include "Serialisation/BinaryReflection.hpp"

namespace NE::Animation {
	namespace {
		inline void SortCurveKeys(AnimCurveF& c) {
			std::sort(c.keys.begin(), c.keys.end(),
				[](const AnimKeyF& a, const AnimKeyF& b) { return a.time < b.time; });
		}
	}

	bool AnimationClip::Preload(NE::Resource::BinaryView blobView) {
		using NE::Resource::NanoAnimClipHeader;
		using NE::Resource::NANC_MAGIC;
		using NE::Resource::CURRENT_NANOANIMCLIP_FORMAT_VERSION;

		if (blobView.size < sizeof(NanoAnimClipHeader)) return false;

		const auto* hdr = blobView.as<NanoAnimClipHeader>(0);
		if (!hdr) return false;
		if (hdr->magic != NANC_MAGIC) return false;
		if (hdr->version != CURRENT_NANOANIMCLIP_FORMAT_VERSION) return false;

		const size_t payloadOff = sizeof(NanoAnimClipHeader);
		const size_t payloadSize = (blobView.size > payloadOff) ? (blobView.size - payloadOff) : 0;

		if (hdr->payloadBytes != 0 && hdr->payloadBytes != payloadSize) {
			// allow mismatch if you want, but better to gate
			return false;
		}

		const uint8_t* data = blobView.at(payloadOff, payloadSize);
		if (!data && payloadSize != 0) return false;

		const uint8_t* it = data;
		const uint8_t* end = data + payloadSize;

		AnimClipBlob in{};
		if (!Deserialization::FromBinary(it, end, in)) return false;

		// Optional: ensure fully consumed (can allow trailing for future)
		// if (it != end) return false;

		// Commit to runtime object
		name = std::move(in.name);
		lengthSeconds = in.lengthSeconds;
		looping = in.looping;
		tracks = std::move(in.tracks);

		return true;
	}

	void AnimationClip::Finalize() {
		// Sort keys & normalize
		float maxT = 0.0f;

		for (auto& tr : tracks) {
			SortCurveKeys(tr.x);
			SortCurveKeys(tr.y);
			SortCurveKeys(tr.z);
			SortCurveKeys(tr.w);

			auto scan = [&](const AnimCurveF& c) {
				if (!c.keys.empty()) maxT = std::max(maxT, c.keys.back().time);
				};
			scan(tr.x); scan(tr.y); scan(tr.z); scan(tr.w);
		}

		if (lengthSeconds <= 0.0f) lengthSeconds = maxT;

		// later: build binding cache, track->field pointers, etc.

        m_stage = ParsedAnimClip{};
	}

	const std::string& AnimationClip::GetName() const {
		return name;
	}

	float AnimationClip::GetLengthSeconds() const {
		return lengthSeconds;
	}

	void AnimationClip::SetLengthSeconds(float len) {
		lengthSeconds = len;
	}

	bool AnimationClip::IsLooping() const {
		return looping;
	}

	std::vector<AnimTrack>& AnimationClip::GetTracksMutable() {
		return tracks;
	}

	const std::vector<AnimTrack>& AnimationClip::GetTracks() const {
		return tracks;
	}
}