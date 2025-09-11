#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "NANOEngineAPI.hpp"
#include "Graphics/Core/Material.hpp"
#include "Graphics/Core/Model.hpp"
#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"

namespace NE {
	NANOENGINE_API void Initialize();
	NANOENGINE_API void LoadStartupScene();
	NANOENGINE_API void Run(double dt);
	NANOENGINE_API void Shutdown();

	NANOENGINE_API void* GetNativeWindowHandle();
	NANOENGINE_API bool WindowShouldClose();
	NANOENGINE_API uint32_t GetSceneFrameBuffer();
	NANOENGINE_API void SetEditorCamera(void* camera);

	NANOENGINE_API uint32_t GetPickedEntity(uint32_t x, uint32_t y);
	
	NANOENGINE_API void SaveCurrentScene(std::string path);
	NANOENGINE_API void LoadTargetScene(std::string targetPath);

	NANOENGINE_API void LoadShader(std::string_view);

	NANOENGINE_API std::shared_ptr<Graphics::Material> GetMaterial(std::string_view path);
	NANOENGINE_API const std::vector<std::pair<std::string, std::shared_ptr<Graphics::Model>>>& GetAllModels();
	NANOENGINE_API size_t GetNumEntities();

	NANOENGINE_API void EditorPlay();
	NANOENGINE_API void EditorPause();
	NANOENGINE_API void EditorEdit();
}
