#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <string>
#include <vector>

#include "ECS/Core/Entity.hpp"

namespace NE::ECS { class ECSCoordinator; }

namespace NE {
	namespace Serialization {
		void SerializeScene(ECS::ECSCoordinator& ecs, const std::vector<ECS::Entity>& rootNodes, const std::string& path);
		void SerializePrefab();
	}

	namespace Deserialization {
		void DeserializeScene(ECS::ECSCoordinator& ecs, const std::string& path);
		void DeserializePrefab();
	}
}

#endif // !SERIALIZER_HPP
