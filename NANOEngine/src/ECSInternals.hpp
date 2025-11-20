#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>
#include "NANOEngineAPI.hpp"
#include <typeindex>


namespace NE {

	NANOENGINE_API std::vector<uint32_t>& GetEntities();

	NANOENGINE_API void SetMotionType(uint32_t e, uint8_t motionType);
}
