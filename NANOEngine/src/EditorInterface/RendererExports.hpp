#pragma once

#include <cstdint>
#include <string>
#include "../NANOEngineAPI.hpp"

// Forward Decl
namespace NE::ECS::Component {
	struct Renderer;
}

namespace NE::Renderer {

	namespace Query {
		NANOENGINE_API std::string GetModel(uint32_t e);
		NANOENGINE_API std::string GetMaterial(uint32_t e);
//FOG
		NANOENGINE_API bool  GetFogEnabled();
		NANOENGINE_API int   GetFogMode();
		NANOENGINE_API void  GetFogColor(float outColor[3]);
		NANOENGINE_API float GetFogDensity();
		NANOENGINE_API void  GetFogRange(float& start, float& end);
		//END FOG
	}

	namespace Command {
		NANOENGINE_API void AssignModel(uint32_t e, const std::string& uuid);
		NANOENGINE_API void AssignMaterial(uint32_t e, const std::string& uuid);
//FOG
		NANOENGINE_API void SetFogEnabled(bool v);
		NANOENGINE_API void SetFogMode(int mode);
		NANOENGINE_API void SetFogColor(float r, float g, float b);
		NANOENGINE_API void SetFogDensity(float d);
		NANOENGINE_API void SetFogRange(float start, float end);
		//END FOG
	}

}
