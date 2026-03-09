#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Core/Reflection.hpp"
#include <Lighting/LightingAsset.hpp>

namespace NE::SceneManagement {

	struct LightingContainer {
		bool enabled = false;
		bool valid = false;
		std::string lightingAssetRef;
		std::string lightingRevisionId;
		std::string dependencySignature;

		NE_REFLECT_BEGIN(LightingContainer)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(valid),
			NE_REFLECT_FIELD(lightingAssetRef),
			NE_REFLECT_FIELD(lightingRevisionId),
			NE_REFLECT_FIELD(dependencySignature)
		NE_REFLECT_END()

		std::shared_ptr<NE::Lighting::LightingAsset> resolvedAsset;
		std::unordered_map<std::string, uint32_t> pageIdToSlot;
		std::string statusMessage;
	};

}
