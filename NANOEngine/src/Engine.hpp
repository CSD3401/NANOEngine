#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "NANOEngineAPI.hpp"
#include "Graphics/Core/Material.hpp"
#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"
#include "Math/Quat.hpp"
#include "ECS/Core/Entity.hpp"
#include "Graphics/OpenGL/GLShader.hpp"

namespace NE {
	namespace SceneManagement {
		class Scene;
	}
	namespace Asset {
		class AudioBank;
	}
	namespace Animation {
		class AnimationClip;
	}

	// internal usage
	NANOENGINE_API SceneManagement::Scene& GetScene();

	NANOENGINE_API void Initialize();
	NANOENGINE_API void Run(double dt);
	NANOENGINE_API void Shutdown();

	NANOENGINE_API void* GetNativeWindowHandle();
	NANOENGINE_API bool WindowShouldClose();
	NANOENGINE_API void RequestClose();
	NANOENGINE_API uint32_t GetSceneColorAttachment();
	NANOENGINE_API uint32_t GetSceneDebugAttachment();
	NANOENGINE_API uint32_t GetGameColorAttachment();
	NANOENGINE_API void SetScenePreviewMode(uint8_t mode);
	NANOENGINE_API uint8_t GetScenePreviewMode();
	NANOENGINE_API void SetScenePreviewUvScale(float scale);
	NANOENGINE_API float GetScenePreviewUvScale();

	// Game View (Editor/Game panel) render resolution.
	// This controls the main camera render target size and post-processing output size.
	NANOENGINE_API void SetGameViewResolution(uint32_t width, uint32_t height);
	NANOENGINE_API uint32_t GetGameViewWidth();
	NANOENGINE_API uint32_t GetGameViewHeight();
	NANOENGINE_API void UpdateEditorCameraData();
	NANOENGINE_API void SetEditorCamera(void* camera);

	NANOENGINE_API uint32_t GetPickedEntity(uint32_t x, uint32_t y);
	NANOENGINE_API std::vector<uint32_t> GetPickedEntities(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
	
	NANOENGINE_API void CookScene(const std::vector<ECS::Entity>& rootNodes, const std::string& _artifactPath);
	NANOENGINE_API bool LoadScene(const std::string& _uuid);

	// For binary fail fallbacks
	NANOENGINE_API void CreateSceneFallback(const std::string& _artifactPath);
	NANOENGINE_API void StartSceneFallback();

	NANOENGINE_API void CookPrefab(const ECS::Entity rootNodes, const std::string& _artifactPath);
	NANOENGINE_API uint32_t LoadPrefab(const std::string& _uuid);

	NANOENGINE_API const std::vector<uint32_t>& GetNumEntities();
	NANOENGINE_API std::vector<uint32_t> DeserializePrefab(std::string prefabPath);
	NANOENGINE_API std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid);
	NANOENGINE_API std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid, Math::Vec3 pos);
	NANOENGINE_API bool LoadPrefabScene(std::string prefabPath);
	NANOENGINE_API void ReloadAllInstancesOfPrefab(std::string prefabUUID);
	NANOENGINE_API void ClosePrefabScene();

	NANOENGINE_API uint32_t DuplicateEntity(uint32_t entity);
	NANOENGINE_API std::vector<uint8_t> CopyEntity(uint32_t entity);
	NANOENGINE_API uint32_t PasteEntity(const std::vector<uint8_t>& clipboard);

	NANOENGINE_API void CreatePrefabFromEntity(uint32_t entity, std::string& uuid, uint32_t& localID, bool isRoot = false);
	NANOENGINE_API void UnpackPrefab(uint32_t entity, bool isRoot = false);

	//NANOENGINE_API const std::vector<std::pair<std::string, std::shared_ptr<Asset::AudioBank>>>& GetAllAudioBanks();

	NANOENGINE_API bool CookShader(const std::string& sourcePath, const std::string& outPath, std::unordered_map<unsigned int, std::string>& shaderStages); // here for now

	NANOENGINE_API void UseProductionSceneManager();

	NANOENGINE_API void StartRuntime();
	NANOENGINE_API void StopRuntime();
	//NANOENGINE_API void EditorPause();

	NANOENGINE_API int GetDrawCallCount();

	NANOENGINE_API uint32_t GetUIScreenWidth();
	NANOENGINE_API uint32_t GetUIScreenHeight();

	NANOENGINE_API void DisplayFinalOutput(int windowWidth, int windowHeight);

	NANOENGINE_API unsigned int LoadCookedThumbnailGL(const std::string& uuid);
	NANOENGINE_API void DestroyGLTexture(unsigned int id);

	NANOENGINE_API bool CookMeshCollider(const std::vector<Math::Vec3>& vertices,
		const std::vector<uint32_t>& indices, std::vector<uint8_t>& outBlob);

	NANOENGINE_API void PreviewAnimation(uint32_t entity, const Animation::AnimationClip& animClip, float timeInSeconds);

	void SetCursorVisible(bool visible);
}
