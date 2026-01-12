#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "NANOEngineAPI.hpp"
#include "Graphics/Core/Material.hpp"
#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"
#include "ECS/Core/Entity.hpp"
#include "Graphics/OpenGL/GLShader.hpp"

namespace NE {
	namespace SceneManagement {
		class Scene;
	}
	namespace Asset {
		class AudioBank;
	}

	// internal usage
	NANOENGINE_API SceneManagement::Scene& GetScene();

	NANOENGINE_API void Initialize();
	NANOENGINE_API void Run(double dt);
	NANOENGINE_API void Shutdown();

	NANOENGINE_API void* GetNativeWindowHandle();
	NANOENGINE_API bool WindowShouldClose();
	NANOENGINE_API uint32_t GetSceneColorAttachment();
	NANOENGINE_API uint32_t GetGameColorAttachment();
	NANOENGINE_API void UpdateEditorCameraData();
	NANOENGINE_API void SetEditorCamera(void* camera);

	NANOENGINE_API uint32_t GetPickedEntity(uint32_t x, uint32_t y);
	NANOENGINE_API std::vector<uint32_t> GetPickedEntities(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
	
	NANOENGINE_API void CookScene(const std::vector<ECS::Entity>& rootNodes, const std::string& _artifactPath);
	NANOENGINE_API void LoadScene(const std::string& _artifactPath);

	NANOENGINE_API const std::vector<uint32_t>& GetNumEntities();
	NANOENGINE_API std::string SerializePrefab(uint32_t entt, std::string targetPath);
	NANOENGINE_API std::vector<uint32_t> DeserializePrefab(std::string prefabPath);
	NANOENGINE_API std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid);
	NANOENGINE_API std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid, Math::Vec3 pos);
	NANOENGINE_API void LoadPrefabScene(std::string prefabPath);
	NANOENGINE_API void SavePrefabScene(std::string prefabPath);
	NANOENGINE_API void ReloadAllInstancesOfPrefab(std::string prefabUUID, std::string prefabPath);
	NANOENGINE_API void ClosePrefabScene();

	NANOENGINE_API uint32_t DuplicateEntity(uint32_t entity);
	NANOENGINE_API std::vector<uint8_t> CopyEntity(uint32_t entity);
	NANOENGINE_API uint32_t PasteEntity(std::vector<uint8_t> clipboard);

	//NANOENGINE_API const std::vector<std::pair<std::string, std::shared_ptr<Asset::AudioBank>>>& GetAllAudioBanks();

	NANOENGINE_API bool CookShader(const std::string& sourcePath, const std::string& outPath, std::unordered_map<unsigned int, std::string>& shaderStages); // here for now

	NANOENGINE_API void StartRuntime();
	NANOENGINE_API void StopRuntime();
	//NANOENGINE_API void EditorPause();

	NANOENGINE_API int GetDrawCallCount();

	NANOENGINE_API void DisplayFinalOutput(int windowWidth, int windowHeight);
}
