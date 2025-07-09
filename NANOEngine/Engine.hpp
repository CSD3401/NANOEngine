#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "NANOEngineAPI.hpp"
#include "src/Graphics/Core/Material.hpp"
#include "src/Math/Vec3.hpp"
#include "src/Math/Mat4.hpp"

namespace NANOEngine {
	NANOENGINE_API void Initialize();
	NANOENGINE_API void Run(double dt);
	NANOENGINE_API void Shutdown();

	NANOENGINE_API void* GetNativeWindowHandle();
	NANOENGINE_API bool WindowShouldClose();
	NANOENGINE_API uint32_t GetSceneFrameBuffer();
	NANOENGINE_API void SetEditorCamera(void* camera);

	NANOENGINE_API uint32_t GetPickedEntity(uint32_t x, uint32_t y);
	
	NANOENGINE_API void SaveCurrentScene(std::string path);
	NANOENGINE_API void LoadTargetScene(std::string targetPath);
}
