#include "MetadataHandler.hpp"

#include <fstream>
#include <random>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace NANOEngine::Utility {

    bool MetadataHandler::MetaFileExists(const std::string& assetPath)
    {
        fs::path metaPath = assetPath + ".meta";
        return fs::exists(metaPath);
    }

    void MetadataHandler::GenerateMetaFile(const std::string& assetPath)
    {
        fs::path metaPath = assetPath + ".meta";
        std::string uuid = GenerateUUID();

        std::ofstream ofs(metaPath);
        ofs << "uuid: " << uuid << '\n';
        ofs.close();

        //m_uuidToPath[uuid] = assetPath;
    }

    std::string MetadataHandler::ParseUUIDFromFilePath(const std::string& assetPath)
    {
        fs::path metaPath = assetPath + ".meta";
        std::ifstream ifs(metaPath);
        if (!ifs.is_open())
            return "";

        std::string line;
        std::string uuid;
        if (std::getline(ifs, line)) {
            size_t pos = line.find("uuid: ");
            if (pos != std::string::npos)
                uuid = line.substr(pos + 6);
        }

        //if (!uuid.empty())
        //    m_uuidToPath[uuid] = assetPath;

        return uuid;
    }

    //std::string MetadataHandler::RetrieveFilePathFromUUID(const std::string& uuid)
    //{
    //    auto it = m_uuidToPath.find(uuid);
    //    if (it != m_uuidToPath.end())
    //        return it->second;
    //    return "";
    //}

    std::string MetadataHandler::GenerateUUID()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static std::uniform_int_distribution<> dis2(8, 11);

        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 8; ++i)
            ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; ++i)
            ss << dis(gen);
        ss << "-4"; // version 4
        for (int i = 0; i < 3; ++i)
            ss << dis(gen);
        ss << "-";
        ss << dis2(gen);
        for (int i = 0; i < 3; ++i)
            ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; ++i)
            ss << dis(gen);
        return ss.str();
    }

}
