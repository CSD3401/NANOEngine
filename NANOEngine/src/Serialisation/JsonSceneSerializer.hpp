#pragma once
#include "ISceneSerializer.hpp"
#include <vector>
#include <rapidjson/document.h>

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
        static std::vector<uint32_t> DeserializePrefab(SceneManagement::Scene& scene, const std::string& path); // Deprecated
        static std::vector<uint32_t> DeserializePrefab(SceneManagement::Scene& scene, const std::string& path, NE::Math::Vec3 camForwardPos);
        static void ReloadComponentsForEntity(SceneManagement::Scene& scene,
            uint32_t entity,
            uint32_t rootEntity,
            const rapidjson::Value& entVal);

        static void SerializePrefabToMemory(SceneManagement::Scene& scene,
            uint32_t rootEnt,
            std::vector<uint8_t>& outBuffer);

        static std::vector<uint32_t> DeserializePrefabFromMemory(SceneManagement::Scene& scene,
            const uint8_t* data,
            size_t size);

        static std::vector<uint32_t> DeserializePrefabFromMemory(SceneManagement::Scene& scene,
            const std::vector<uint8_t>& buffer);

        static void SerializeToMemory(SceneManagement::Scene& scene, std::vector<uint8_t>& outBuffer);
        static void DeserializeFromMemory(SceneManagement::Scene& scene, const uint8_t* data, size_t size);
        static void DeserializeFromMemory(SceneManagement::Scene& scene, const std::vector<uint8_t>& buffer);
    };
}