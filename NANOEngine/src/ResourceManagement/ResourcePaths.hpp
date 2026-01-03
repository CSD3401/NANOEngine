// AssetPaths.hpp (shared by editor+engine; tiny, standalone)
#pragma once
#include <string>

namespace NE::Resource {

    inline std::string ComputeArtifactPathFromUUID(const std::string& uuid) {
        std::string shard = uuid.substr(0, 2);
        // Default to texture extension for backward compatibility
        // TODO: Determine extension based on asset type
        return "Library/NANOArtifacts/" + shard + "/" + uuid + ".ntexbin";
    }
    
    inline std::string ComputeFontArtifactPathFromUUID(const std::string& uuid) {
        std::string shard = uuid.substr(0, 2);
        return "Library/NANOArtifacts/" + shard + "/" + uuid + ".nfontbin";
    }

}
