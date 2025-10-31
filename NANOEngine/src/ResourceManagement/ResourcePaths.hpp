// AssetPaths.hpp (shared by editor+engine; tiny, standalone)
#pragma once
#include <string>

namespace NE::Resource {

    inline std::string ComputeArtifactPathFromUUID(const std::string& uuid) {
        std::string shard = uuid.substr(0, 2);
        return "Library/NANOArtifacts/" + shard + "/" + uuid + ".ntexbin"; // need to change to auto set extension based on type
    }

}
