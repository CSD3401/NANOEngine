#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "NANOEngineAPI.hpp"
#include "Graphics/Core/Material.hpp"
#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"
#include "Audio/AudioBank.hpp"
#include "Graphics/OpenGL/GLShader.hpp"

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

	//NANOENGINE_API void LoadShader(std::string_view);
	//NANOENGINE_API void LoadTexture(std::string_view filePath);

	//NANOENGINE_API std::shared_ptr<Graphics::OpenGL::GLTexture> GetTexture(std::string_view filePath);
	//NANOENGINE_API std::shared_ptr<Graphics::Material> GetMaterial(std::string_view path);
	//NANOENGINE_API const std::vector<std::pair<std::string, std::shared_ptr<Graphics::Model>>>& GetAllModels();
	//NANOENGINE_API const std::vector<std::pair<std::string, std::shared_ptr<Graphics::OpenGL::GLShader>>>& GetAllShaders();
	NANOENGINE_API size_t GetNumEntities();

	//NANOENGINE_API const std::vector<std::pair<std::string, std::shared_ptr<Asset::AudioBank>>>& GetAllAudioBanks();

	NANOENGINE_API std::shared_ptr<NE::Graphics::Material> LoadMaterial(std::string uuid);
	NANOENGINE_API bool CookShader(const std::string& sourcePath, const std::string& outPath, std::unordered_map<unsigned int, std::string>& shaderStages); // here for now

	NANOENGINE_API void EditorPlay();
	NANOENGINE_API void EditorPause();
	NANOENGINE_API void EditorEdit();

	NANOENGINE_API int GetDrawCallCount();
}
