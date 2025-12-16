#ifndef RESOURCE_PATHS_HPP
#define RESOURCE_PATHS_HPP

#include <string_view>

#include "ResourceTypes.hpp"
#include "Core/SpdLogger.hpp"

namespace NE::Resource {
	constexpr std::string_view artifactPath = "Library/Artifacts/";

	inline std::string ComputeArtifactPathFromUUID(std::string_view uuid, ResourceType type) {
		std::string path;
		path += artifactPath;
		path.append(uuid.substr(0, 2));
		path += '/';
		path += uuid;

		switch (type) {
		case ResourceType::Texture:		path += ".ntexbin"; break;
		case ResourceType::Model:		path += ".nmodbin"; break;
		case ResourceType::Shader:		path += ".nshdbin"; break;
		case ResourceType::Material:	path += ".nmatbin"; break;
		case ResourceType::Audio:		path += ".naudbin"; break;
		case ResourceType::Prefab:		path += ".nfabbin"; break;
		case ResourceType::Scene:		path += ".nscebin"; break;
		default:
			SPD_WARNING("Invalid Artifact Path");
			break;
		}

		return path;
	}
}

#endif