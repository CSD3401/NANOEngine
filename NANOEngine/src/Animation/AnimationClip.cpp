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

		inline void SortCurveKeys(AnimCurveS& c) {
			std::sort(c.keys.begin(), c.keys.end(),
				[](const AnimKeyS& a, const AnimKeyS& b) { return a.time < b.time; });
		}

		struct AnimTrackV1 {
			std::string relativePath;
			uint32_t componentTypeId = 0;
			uint32_t fieldId = 0;
			AnimValueType type = AnimValueType::Float;
			AnimCurveF x, y, z, w;

			NE_REFLECT_BEGIN(AnimTrackV1)
				NE_REFLECT_FIELD(relativePath),
				NE_REFLECT_FIELD(componentTypeId),
				NE_REFLECT_FIELD(fieldId),
				NE_REFLECT_FIELD(type),
				NE_REFLECT_FIELD(x),
				NE_REFLECT_FIELD(y),
				NE_REFLECT_FIELD(z),
				NE_REFLECT_FIELD(w)
				NE_REFLECT_END()
		};

		struct AnimClipBlobV1 {
			std::string name;
			float lengthSeconds = 0.0f;
			bool looping = false;
			std::vector<AnimTrackV1> tracks;

			NE_REFLECT_BEGIN(AnimClipBlobV1)
				NE_REFLECT_FIELD(name),
				NE_REFLECT_FIELD(lengthSeconds),
				NE_REFLECT_FIELD(looping),
				NE_REFLECT_FIELD(tracks)
				NE_REFLECT_END()
		};
	}

	bool AnimationClip::Preload(NE::Resource::BinaryView blobView) {
		using NE::Resource::NanoAnimClipHeader;
		using NE::Resource::NANC_MAGIC;
		using NE::Resource::CURRENT_NANOANIMCLIP_FORMAT_VERSION;

		if (blobView.size < sizeof(NanoAnimClipHeader)) return false;

		const auto* hdr = blobView.as<NanoAnimClipHeader>(0);
		if (!hdr) return false;
		if (hdr->magic != NANC_MAGIC) return false;
		if (hdr->version != 1 && hdr->version != CURRENT_NANOANIMCLIP_FORMAT_VERSION) return false;

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

		if (hdr->version == 1) {
			AnimClipBlobV1 in{};
			if (!Deserialization::FromBinary(it, end, in)) return false;

			name = std::move(in.name);
			lengthSeconds = in.lengthSeconds;
			looping = in.looping;

			tracks.clear();
			tracks.reserve(in.tracks.size());
			for (auto& oldTrack : in.tracks) {
				AnimTrack tr{};
				tr.relativePath = std::move(oldTrack.relativePath);
				tr.componentTypeId = oldTrack.componentTypeId;
				tr.fieldId = oldTrack.fieldId;
				tr.type = oldTrack.type;
				tr.x = std::move(oldTrack.x);
				tr.y = std::move(oldTrack.y);
				tr.z = std::move(oldTrack.z);
				tr.w = std::move(oldTrack.w);
				tracks.push_back(std::move(tr));
			}
		} else {
			AnimClipBlob in{};
			if (!Deserialization::FromBinary(it, end, in)) return false;

			// Commit to runtime object
			name = std::move(in.name);
			lengthSeconds = in.lengthSeconds;
			looping = in.looping;
			tracks = std::move(in.tracks);
		}

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
			SortCurveKeys(tr.s);

			auto scan = [&](const AnimCurveF& c) {
				if (!c.keys.empty()) maxT = std::max(maxT, c.keys.back().time);
				};
			scan(tr.x); scan(tr.y); scan(tr.z); scan(tr.w);
			if (!tr.s.keys.empty()) maxT = std::max(maxT, tr.s.keys.back().time);
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
