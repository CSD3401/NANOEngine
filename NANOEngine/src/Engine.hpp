#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "NANOEngineAPI.hpp"
#include "Graphics/Core/Material.hpp"
#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"

#include "Graphics/OpenGL/GLShader.hpp"

namespace NE {

	namespace Asset {
		class AudioBank;
	}

	NANOENGINE_API void Initialize();
	NANOENGINE_API void LoadStartupScene();
	NANOENGINE_API void Run(double dt);
	NANOENGINE_API void Shutdown();

	NANOENGINE_API void* GetNativeWindowHandle();
	NANOENGINE_API bool WindowShouldClose();
	NANOENGINE_API uint32_t GetSceneColorAttachment();
	NANOENGINE_API uint32_t GetGameColorAttachment();
	NANOENGINE_API void UpdateEditorCameraData();
	NANOENGINE_API void SetEditorCamera(void* camera);

	NANOENGINE_API uint32_t GetPickedEntity(uint32_t x, uint32_t y);
	
	NANOENGINE_API void SaveCurrentScene(std::string path);
	NANOENGINE_API void SaveSceneIfDirty(std::string path = "");
	NANOENGINE_API bool IsSceneDirty();
	NANOENGINE_API void MarkSceneDirty();
	NANOENGINE_API void LoadTargetScene(std::string targetPath);

	NANOENGINE_API const std::vector<uint32_t>& GetNumEntities();
	NANOENGINE_API std::string SerializePrefab(uint32_t entt, std::string targetPath);
	NANOENGINE_API std::vector<uint32_t> DeserializePrefab(std::string prefabPath);
	NANOENGINE_API std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid);
	NANOENGINE_API std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid, Math::Vec3 pos);
	NANOENGINE_API void LoadPrefabScene(std::string prefabPath);
	NANOENGINE_API void SavePrefabScene(std::string prefabPath);
	NANOENGINE_API void ReloadAllInstancesOfPrefab(std::string prefabUUID, std::string prefabPath);
	NANOENGINE_API void ClosePrefabScene();

	NANOENGINE_API std::vector<uint32_t> DuplicateEntity(uint32_t entity);
	NANOENGINE_API std::vector<uint8_t> CopyEntity(uint32_t entity);
	NANOENGINE_API std::vector<uint32_t> PasteEntity(std::vector<uint8_t> clipboard, Math::Vec3 pos);

	//NANOENGINE_API const std::vector<std::pair<std::string, std::shared_ptr<Asset::AudioBank>>>& GetAllAudioBanks();

	NANOENGINE_API std::shared_ptr<NE::Graphics::Material> LoadMaterial(std::string uuid);
	NANOENGINE_API bool CookShader(const std::string& sourcePath, const std::string& outPath, std::unordered_map<unsigned int, std::string>& shaderStages); // here for now

	NANOENGINE_API void EditorPlay();
	NANOENGINE_API void EditorPause();
	NANOENGINE_API void EditorEdit();

	NANOENGINE_API int GetDrawCallCount();
}
