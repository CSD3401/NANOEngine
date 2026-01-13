#pragma once

#include <cstdint>
#include <string>
#include "../NANOEngineAPI.hpp"
#include "../ECS/Core/Entity.hpp"
#include "Math/Vec3.hpp"


namespace NE::Physics {
	namespace Query {
	}

	namespace Command {
		void NANOENGINE_API DrawSelectedCollider(ECS::Entity e);
	}
}