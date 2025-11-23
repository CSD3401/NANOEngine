#pragma once
#include "ISceneSerializer.hpp"
#include <vector>

namespace NE::Math {
    struct Vec3;
}

namespace NE::Serialization {
    class JsonSceneSerializer {
    public:
        static void Serialize(SceneManagement::Scene& scene, const std::string& path);
        static void Serialize(SceneManagement::Scene& scene, std::vector<uint32_t>& hierarchy, const std::string& path);
        static void Deserialize(SceneManagement::Scene& scene, const std::string& path);
   
        static std::string SerializePrefab(SceneManagement::Scene& scene, uint32_t entt, const std::string& path);
        static std::vector<uint32_t> DeserializePrefab(SceneManagement::Scene& scene, const std::string& path);
        //static void DeserializePrefab(SceneManagement::Scene& scene, NE::Math::Vec3 camForwardPos, const std::string& path);

        static void SerializeToMemory(SceneManagement::Scene& scene, std::vector<uint8_t>& outBuffer);
        static void DeserializeFromMemory(SceneManagement::Scene& scene, const uint8_t* data, size_t size);
        static void DeserializeFromMemory(SceneManagement::Scene& scene, const std::vector<uint8_t>& buffer);
    };
}