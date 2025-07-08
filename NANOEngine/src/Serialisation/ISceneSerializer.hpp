#pragma once
#include <string>

namespace NANOEngine {
    namespace SceneManagement { class Scene; }
    namespace Serialization {
        class ISceneSerializer {
        public:
            virtual ~ISceneSerializer() = default;
            virtual void Serialize(SceneManagement::Scene& scene, const std::string& path) = 0;
            virtual void Deserialize(SceneManagement::Scene& scene, const std::string& path) = 0;
        };
    }
}