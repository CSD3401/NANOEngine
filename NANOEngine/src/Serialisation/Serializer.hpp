#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>
#include <vector>

#include "ECS/Core/Entity.hpp"

namespace NE::ECS { class ECSCoordinator; }
namespace NE::SceneManagement { class Scene; }

namespace NE {
	namespace Serialization {
		void SerializeScene(ECS::ECSCoordinator& ecs, const std::vector<ECS::Entity>& rootNodes, const std::string& path);

		[[deprecated("No longer being used due to instability")]]
		void SerializePrefab(ECS::ECSCoordinator& ecs, const ECS::Entity rootNodes, const std::string& path);
		void SerializeEntitiesToMemory(ECS::ECSCoordinator& ecs, const uint32_t rootEnt, std::vector<uint8_t>& outBuffer);
	}

	namespace Deserialization {
		bool DeserializeScene(ECS::ECSCoordinator& ecs, const std::string& path, SceneManagement::Scene* owningScene = nullptr);
		uint32_t DeserializePrefab(ECS::ECSCoordinator& ecs, const std::string& path);
		bool DeserializePrefab(ECS::ECSCoordinator& ecs, const std::string& path, uint32_t root);
		uint32_t DeserializeEntitiesFromMemory(ECS::ECSCoordinator& ecs, std::vector<uint8_t>& outBuffer);
	}
}

#endif // !SERIALIZER_HPP
