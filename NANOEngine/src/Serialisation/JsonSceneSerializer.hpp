#pragma once
#include "ISceneSerializer.hpp"
#include <vector>

namespace NE::Serialization {
    class JsonSceneSerializer {
    public:
        static void Serialize(SceneManagement::Scene& scene, const std::string& path);
        static void Serialize(SceneManagement::Scene& scene, std::vector<uint32_t>& hierarchy, const std::string& path);
        static void Deserialize(SceneManagement::Scene& scene, const std::string& path);
        static void ReloadScene(SceneManagement::Scene& scene, std::vector<uint32_t>& hierarchy, const std::string& path);
   
        static void SerializeToMemory(SceneManagement::Scene& scene, std::vector<uint8_t>& outBuffer);
        static void DeserializeFromMemory(SceneManagement::Scene& scene, const uint8_t* data, size_t size);
        static void DeserializeFromMemory(SceneManagement::Scene& scene, const std::vector<uint8_t>& buffer);
    };
}