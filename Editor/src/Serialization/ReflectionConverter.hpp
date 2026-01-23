#include <fstream>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include <Core/SpdLogger.hpp>
#include <Serialisation/BinaryReflection.hpp>

#include "JSONReflection.hpp"

namespace Editor::Utils {

    template <NE::Core::Reflectable T>
    inline bool JSONToBinaryFile(const std::filesystem::path& jsonPath, const std::filesystem::path& binaryPath) {
        std::ifstream ifs(jsonPath);
        if (!ifs.is_open()) {
            SPD_ERROR("Failed to open JSON file: " << jsonPath);
            return false;
        }

        rapidjson::IStreamWrapper isw(ifs);
        rapidjson::Document doc;
        doc.ParseStream(isw);

        if (doc.HasParseError()) {
            SPD_ERROR("JSON Parse Error: " << doc.GetParseError());
            return false;
        }

        T objectInstance;
        Deserialization::FromJSON(doc, objectInstance);

        NE::ByteBuffer buffer;
        NE::Serialization::ToBinary(buffer, objectInstance);

        std::ofstream ofs(binaryPath, std::ios::binary);
        if (!ofs.is_open()) {
            SPD_ERROR("Failed to open output binary file: " << binaryPath);
            return false;
        }

        ofs.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        return true;
    }
}