#ifndef METADATA_HANDLER_HPP
#define METADATA_HANDLER_HPP

#include <string>
#include <unordered_map>
#include "../../NANOEngineAPI.hpp"

namespace NANOEngine::Utility {

    class NANOENGINE_API MetadataHandler {
    public:
        static bool MetaFileExists(const std::string& assetPath);
        static void GenerateMetaFile(const std::string& assetPath);
        static std::string ParseUUIDFromFilePath(const std::string& assetPath);
        //static std::string RetrieveFilePathFromUUID(const std::string& uuid);

    private:
        static std::string GenerateUUID();
        //static inline std::unordered_map<std::string, std::string> m_uuidToPath{};
    };

}

#endif // METADATA_HANDLER_HPP