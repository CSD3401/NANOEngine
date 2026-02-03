#include "Engine.hpp"

#include <memory>
#include <fstream>
#include "Graphics/Core/Window.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Core/Profiler.hpp"
#include "SceneManagement/Scene.hpp"
#include "Physics/PhysicsManager.hpp"
#include "Physics/JoltDebugRenderer.hpp"
#include "SceneManagement/SceneManager.hpp"
#include "Tween/TweenManager.hpp"
#include "Core/SpdLogger.hpp"
#include <glad/glad.h>
#include "ResourceManagement/BinaryHeaders/NanoShdHeader.hpp"
#include "Input/InputManager.hpp"
#include "Audio/AudioBank.hpp"
#include "Scripting/ScriptingEngine.hpp"
#include "PrefabManagement/PrefabManager.hpp"
#include "Serialisation/Serializer.hpp"
#include "ResourceManagement/ResourcePaths.hpp"
#include <glfw/glfw3.h>
#include "ECS/Components/PrefabLink.hpp"
#include "ECS/Components/PrefabInstance.hpp"
#include "ECS/Components/Hierarchy.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "Animation/AnimationClip.hpp"
#include "ECS/Systems/AnimatorSystem.hpp"

namespace {

	bool Compile(const std::unordered_map<GLenum, std::string>& shaderSources, uint32_t& programID)
	{
		uint32_t program = glCreateProgram();
		glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
		std::vector<GLuint> shaderIDs;

		for (auto& [type, source] : shaderSources) {
			GLuint shader = glCreateShader(type);
			const char* src = source.c_str();
			glShaderSource(shader, 1, &src, nullptr);
			glCompileShader(shader);

			GLint compiled;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
			if (compiled != GL_TRUE) {
				char log[1024];
				glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
				SPD_WARNING("Shader compilation failed: " << log << "\nShader Source: " << source);
				return false;
			}

			glAttachShader(program, shader);
			shaderIDs.push_back(shader);
		}

		glLinkProgram(program);
		GLint linked;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (linked != GL_TRUE) {
			char log[1024];
			glGetProgramInfoLog(program, sizeof(log), nullptr, log);
			SPD_WARNING("Program linking failed: " << log);
			return false;
		}

		for (auto id : shaderIDs) {
			glDetachShader(program, id);
			glDeleteShader(id);
		}

		programID = program;
		return true;
	}

}

namespace NE {

	static std::unique_ptr<Graphics::Window> s_window;
	static std::unique_ptr<Graphics::IRenderContext> s_renderContext;

	SceneManagement::SceneManager gSceneManager;

	void Initialize() {
		Graphics::WindowProperties props;
		props.Title = "NANOEngine";
		props.Width = 1920;
		props.Height = 1080;
		props.VSync = true;

		s_window = std::make_unique<Graphics::Window>(props);
		s_renderContext = std::make_unique<Graphics::OpenGL::GLContext>();
		s_renderContext->Init(s_window->GetNativeWindow());

		// here for now
		glEnable(GL_CULL_FACE);

		Graphics::GraphicsManager::Init();
		Physics::PhysicsManager::GetInstance().Init();
		Scripting::ScriptingEngine::GetInstance().Initialize();
	}

	void Run(double dt) {
		NE_PROFILE_FUNCTION();
		//s_window->PollEvents();

		//Physics::PhysicsManager::Update(static_cast<float>(dt));
		Physics::JoltDebugRenderer::BeginFrame();
		
		gSceneManager.Update(dt);

		//Graphics::GraphicsManager::SubmitSkybox(); // Submit skybox once per frame

		Physics::JoltDebugRenderer::EndFrame();
		gSceneManager.Render();

		Graphics::GraphicsManager::Clear(); // Clear draw commands after rendering

		TweenManager::Get().Update(static_cast<float>(dt));

		if (InputManager::WasKeyPressed(GLFW_KEY_ESCAPE)) {
			glfwSetInputMode(static_cast<GLFWwindow*>(s_window->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

	void Shutdown() {
		Graphics::GraphicsManager::Shutdown();
		Physics::PhysicsManager::GetInstance().Shutdown();
		

		gSceneManager.ExitScene();

		s_renderContext->Shutdown();
		Scripting::ScriptingEngine::GetInstance().Shutdown(); // needs to run after scriptsystem exit()
		s_renderContext.reset();
		s_window.reset();
	}

	void* GetNativeWindowHandle() {
		return s_window->GetNativeWindow();
	}

	bool WindowShouldClose() {
		return s_window->ShouldClose();
	}

	uint32_t GetSceneColorAttachment() {
		return Graphics::GraphicsManager::GetSceneColorAttachment();
	}

	uint32_t GetGameColorAttachment() {
		return Graphics::GraphicsManager::GetGameColorAttachment();
	}

	void UpdateEditorCameraData() {
		Graphics::GraphicsManager::UpdateEditorCameraData();
	}

	void SetEditorCamera(void* camera) {
		Graphics::GraphicsManager::SetEditorCamera(reinterpret_cast<Graphics::EditorCamera*>(camera));
	}

	uint32_t GetPickedEntity(uint32_t x, uint32_t y) {
		return Graphics::GraphicsManager::ReadPixel(x, y);
	}

	std::vector<uint32_t> GetPickedEntities(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
		std::vector<uint32_t> pickedIds;
		Graphics::GraphicsManager::ReadPixelRect(x, y, width, height, pickedIds);
		return pickedIds;
	}

	void CookScene(const std::vector<ECS::Entity>& rootNodes, const std::string& _artifactPath) {
		NE::Serialization::SerializeScene(GetScene().GetECSCoordinator(), rootNodes, _artifactPath);
	}

	bool LoadScene(const std::string& _artifactPath) {
		return gSceneManager.LoadScene(Resource::ComputeArtifactPathFromUUID(_artifactPath, Resource::ResourceType::Scene));
	}

	void CreateSceneFallback(const std::string& _artifactPath) {
		gSceneManager.CreateSceneFallback(_artifactPath);
	}

	void StartSceneFallback() {
		gSceneManager.StartSceneFallback();
	}

	void CookPrefab(const ECS::Entity rootNode, const std::string& _artifactPath) {
		//NE::Serialization::SerializePrefab(GetScene().GetECSCoordinator(), rootNode, _artifactPath);
	}

	uint32_t LoadPrefab(const std::string& _uuid) {
		auto artifactPath = Resource::ComputeArtifactPathFromUUID(_uuid, Resource::ResourceType::Prefab);
		return NE::Deserialization::DeserializePrefab(GetScene().GetECSCoordinator(), artifactPath);
	}

	const std::vector<uint32_t>& GetNumEntities() {
		return gSceneManager.GetActive()->GetECSCoordinator().GetUsedEntities();
	}

	 std::vector<uint32_t> DeserializePrefab(std::string prefabPath) {
		 //auto newEntities = Serialization::JsonSceneSerializer::DeserializePrefab(*gSceneManager.GetActive(), prefabPath);
		 //return newEntities;
		 return std::vector<uint32_t>();
	}

	std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid) {
		//auto newEntities = Serialization::JsonSceneSerializer::DeserializePrefab(*gSceneManager.GetActive(), prefabPath);
		//Prefab::PrefabManager::Instantiate(uuid, newEntities);
		//return newEntities;
		return std::vector<uint32_t>();
	}

	std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid, Math::Vec3 pos) {
		//auto newEntities = Serialization::JsonSceneSerializer::DeserializePrefab(*gSceneManager.GetActive(), prefabPath, pos);
		//Prefab::PrefabManager::Instantiate(uuid, newEntities);
		//return newEntities;
		return std::vector<uint32_t>();
	}

	bool LoadPrefabScene(std::string prefabPath) {
		return gSceneManager.LoadPrefabScene(prefabPath);
	}

	void ReloadAllInstancesOfPrefab(std::string prefabUUID) {
		NE::Prefab::PrefabManager::ReloadAllInstancesOfPrefab(prefabUUID);
	}

	void ClosePrefabScene() {
		gSceneManager.ClosePrefabScene();
	}

	uint32_t DuplicateEntity(uint32_t entity) {
		std::vector<uint8_t> buffer;
		NE::Serialization::SerializeEntitiesToMemory(gSceneManager.GetActive()->GetECSCoordinator(), entity, buffer);

		return NE::Deserialization::DeserializeEntitiesFromMemory(gSceneManager.GetActive()->GetECSCoordinator(), buffer);
	}

	std::vector<uint8_t> CopyEntity(uint32_t entity) {
		std::vector<uint8_t> buffer;
		NE::Serialization::SerializeEntitiesToMemory(gSceneManager.GetActive()->GetECSCoordinator(), entity, buffer);
		return buffer;
	}

	uint32_t PasteEntity(std::vector<uint8_t> clipboard) {
		return NE::Deserialization::DeserializeEntitiesFromMemory(gSceneManager.GetActive()->GetECSCoordinator(), clipboard);
	}

	void CreatePrefabFromEntity(uint32_t entity, std::string& uuid, uint32_t& localID, bool isRoot) {
		auto& coordinator = GetScene().GetECSCoordinator();

		if (isRoot) {
			coordinator.AddComponent<NE::ECS::Component::PrefabInstance>(
				entity, 
				NE::ECS::Component::PrefabInstance{ uuid }
			);
		}

		coordinator.AddComponent<NE::ECS::Component::PrefabLink>(
			entity, 
			NE::ECS::Component::PrefabLink{ uuid, localID });

		auto& hier = coordinator.GetComponent<NE::ECS::Component::Hierarchy>(entity);
		for (auto childID : hier.children) {
			++localID;
			CreatePrefabFromEntity(childID, uuid, localID);
		}
	}

	void UnpackPrefab(uint32_t entity, bool isRoot) {
		auto& coordinator = GetScene().GetECSCoordinator();

		if (isRoot) {
			coordinator.RemoveComponent<NE::ECS::Component::PrefabInstance>(entity);
		}

		coordinator.RemoveComponent<NE::ECS::Component::PrefabLink>(entity);

		auto& hier = coordinator.GetComponent<NE::ECS::Component::Hierarchy>(entity);
		for (auto childID : hier.children) {
			UnpackPrefab(childID);
		}
	}

	// Internal use only
	SceneManagement::Scene& GetScene() {
		return *gSceneManager.GetActive();
	}

	bool CookShader(const std::string& sourcePath, const std::string& outPath, std::unordered_map<unsigned int, std::string>& shaderStages) {
		uint32_t linkedProgram = 0;
		if (!Compile(shaderStages, linkedProgram)) {
			SPD_WARNING("CookShader: compile failed for " << sourcePath);
			return false;
		}

		GLint binLen = 0;
		glGetProgramiv(linkedProgram, GL_PROGRAM_BINARY_LENGTH, &binLen);
		if (binLen <= 0) {
			glDeleteProgram(linkedProgram);
			SPD_WARNING("CookShader: program binary not retrievable (set GL_PROGRAM_BINARY_RETRIEVABLE_HINT?)");
			return false;
		}

		std::vector<uint8_t> blob(binLen);
		GLsizei written = 0;
		GLenum fmt = 0;
		glGetProgramBinary(linkedProgram, binLen, &written, &fmt, blob.data());

		NE::Resource::NanoShdHeader h{};
		h.stagesMask = 
			((shaderStages.count(GL_VERTEX_SHADER) ? 1u : 0u) << 0) | 
			((shaderStages.count(GL_FRAGMENT_SHADER) ? 1u : 0u) << 4) |
			((shaderStages.count(GL_COMPUTE_SHADER) ? 1u : 0u) << 8);
			;
		h.sourceHash = 0; // (optional) fill later
		h.definesHash = 0; // (optional)
		h.permutationKey = 0; // (optional)
		h.programBinaryFormat = static_cast<uint32_t>(fmt);
		h.programOffset = sizeof(NE::Resource::NanoShdHeader);
		h.programSize = static_cast<uint64_t>(written);

		// Strongly recommended for lab PCs / driver changes:
		const bool embedSourceFallback = true;
		if (embedSourceFallback) h.programFlags |= 1u;

		std::ofstream ofs(outPath, std::ios::binary);
		if (!ofs) { glDeleteProgram(linkedProgram); return false; }
		ofs.write(reinterpret_cast<const char*>(&h), sizeof(h));
		ofs.write(reinterpret_cast<const char*>(blob.data()), written);

		/*if (embedSourceFallback) {
			const auto& vs = shaderStages.at(GL_VERTEX_SHADER);
			const auto& fs = shaderStages.at(GL_FRAGMENT_SHADER);
			uint32_t vsLen = static_cast<uint32_t>(vs.size());
			uint32_t fsLen = static_cast<uint32_t>(fs.size());
			ofs.write(reinterpret_cast<const char*>(&vsLen), 4); ofs.write(vs.data(), vsLen);
			ofs.write(reinterpret_cast<const char*>(&fsLen), 4); ofs.write(fs.data(), fsLen);
		}*/

		if (embedSourceFallback) {
			uint32_t stageCount = static_cast<uint32_t>(shaderStages.size());
			ofs.write(reinterpret_cast<const char*>(&stageCount), 4);

			for (auto& [stageEnum, src] : shaderStages) {
				uint32_t stage = static_cast<uint32_t>(stageEnum);
				uint32_t len = static_cast<uint32_t>(src.size());
				ofs.write(reinterpret_cast<const char*>(&stage), 4);
				ofs.write(reinterpret_cast<const char*>(&len), 4);
				ofs.write(src.data(), len);
			}
		}

		glDeleteProgram(linkedProgram);
		return ofs.good();
	}

	void StartRuntime() {
		gSceneManager.LoadRuntime();
	}

	void StopRuntime() {
		gSceneManager.StopRuntime();
	}

	int GetDrawCallCount() {
		return Graphics::GraphicsManager::drawCount;
	}

	void DisplayFinalOutput(int windowWidth, int windowHeight) {
		Graphics::GraphicsManager::DisplayFinalOutput(windowWidth, windowHeight);
	}

	unsigned int LoadCookedThumbnailGL(const std::string& uuid) {
		return Resource::ResourceManager::GetInstance().LoadCookedThumbnailGL(uuid);
	}

	void DestroyGLTexture(unsigned int id) {
		Resource::ResourceManager::GetInstance().DestroyGLTexture(id);
	}

	bool CookMeshCollider(const std::vector<Math::Vec3>& vertices,
		const std::vector<uint32_t>& indices, std::vector<uint8_t>& outBlob) 
	{
		return NE::Physics::PhysicsManager::GetInstance().CookMeshCollider(vertices, indices, outBlob);
	}

	void PreviewAnimation(uint32_t entity, const Animation::AnimationClip& animClip, float timeInSeconds) {
		gSceneManager.GetActive()->GetECSCoordinator().m_animatorSystem->ApplyClipAtTime(entity, animClip, timeInSeconds);
	}
}