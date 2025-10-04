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
    };
}