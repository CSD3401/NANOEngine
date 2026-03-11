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
		NANOENGINE_API void DrawSelectedCollider(ECS::Entity e);
	}
}