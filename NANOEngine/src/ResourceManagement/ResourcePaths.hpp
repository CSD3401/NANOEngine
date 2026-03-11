#ifndef RESOURCE_PATHS_HPP
#define RESOURCE_PATHS_HPP

#include <string_view>

#include "ResourceTypes.hpp"
#include "Core/SpdLogger.hpp"

namespace NE::Resource {
	constexpr std::string_view artifactPath = "Library/Artifacts/";
	constexpr std::string_view thumbnailPath = "Library/.thumbs/";

	inline std::string ComputeArtifactPathFromUUID(std::string_view uuid, ResourceType type) {
		std::string path;
		path += artifactPath;
		path.append(uuid.substr(0, 2));
		path += '/';
		path += uuid;

		switch (type) {
		case ResourceType::Texture:				path += ".ntexbin"; break;
		case ResourceType::Model:				path += ".nmodbin"; break;
		case ResourceType::Shader:				path += ".nshdbin"; break;
		case ResourceType::Material:			path += ".nmatbin"; break;
		case ResourceType::Audio:				path += ".naudbin"; break;
		case ResourceType::Prefab:				path += ".nfabbin"; break;
		case ResourceType::Scene:				path += ".nscebin"; break;
		case ResourceType::AnimationClip:		path += ".nancbin"; break;
		case ResourceType::AnimatorController:	path += ".nconbin"; break;
		case ResourceType::Font:				path += ".nfntbin"; break;
		case ResourceType::Lighting:			path += ".nlgtbin"; break;
		default:
			//SPD_WARNING("Invalid Artifact Path");
			break;
		}

		return path;
	}

	inline std::string ComputeThumbnailPathFromUUID(std::string_view uuid, bool withExtension = true) {
		std::string path;
		path += thumbnailPath;
		path.append(uuid.substr(0, 2));
		path += '/';
		path += uuid;

		if (withExtension) {
			path += ".thumb";
		}

		return path;
	}
}

#endif
