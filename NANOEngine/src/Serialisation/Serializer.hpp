#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>
#include <vector>

#include "ECS/Core/Entity.hpp"

namespace NE::ECS { class ECSCoordinator; }

namespace NE {
	namespace Serialization {
		void SerializeScene(ECS::ECSCoordinator& ecs, const std::vector<ECS::Entity>& rootNodes, const std::string& path);
		void SerializePrefab(ECS::ECSCoordinator& ecs, const ECS::Entity rootNodes, const std::string& path);
		void SerializeEntitiesToMemory(ECS::ECSCoordinator& ecs, const uint32_t rootEnt, std::vector<uint8_t>& outBuffer);
	}

	namespace Deserialization {
		bool DeserializeScene(ECS::ECSCoordinator& ecs, const std::string& path);
		uint32_t DeserializePrefab(ECS::ECSCoordinator& ecs, const std::string& path);
		uint32_t DeserializeEntitiesFromMemory(ECS::ECSCoordinator& ecs, std::vector<uint8_t>& outBuffer);
	}
}

#endif // !SERIALIZER_HPP
