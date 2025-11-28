#include "Engine.hpp"

#include <memory>
#include <fstream>
#include <filesystem>
#include "Graphics/Core/Window.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/OpenGL/GLFrameBuffer.hpp"
#include "Core/Profiler.hpp"
//#include <glad/glad.h>
#include "SceneManagement/Scene.hpp"
#include "../../src/Serialisation/JsonSceneSerializer.hpp"
#include "ECS/Components/Transform.hpp"
//#include "AssetManager.hpp"
#include "Physics/PhysicsManager.hpp"
#include "Physics/JoltDebugRenderer.hpp"
//#include "EditorInterface/PhysicsExports.hpp"
#include "EngineState.hpp"
#include "SceneManagement/SceneManager.hpp"
#include "Tween/TweenManager.hpp"
#include "Core/SpdLogger.hpp"
#include <glad/glad.h>
#include "ResourceManagement/BinaryHeaders/NanoShdHeader.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "Input/InputManager.hpp"
#include "Graphics/OpenGL/GLTexture.hpp"
#include "Audio/AudioBank.hpp"
#include "Scripting/ScriptingEngine.hpp"
#include "PrefabManagement/PrefabManager.hpp"
#include <glfw/glfw3.h>

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
		//NE_PROFILE_FUNCTION();
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
		Physics::PhysicsManager::Init();
		Scripting::ScriptingEngine::GetInstance().Initialize();
		//Physics::PhysicsManager::TestPhysicsSetup();
		//glDisable(GL_FRAMEBUFFER_SRGB);
	}

	void LoadStartupScene() {
		gSceneManager.LoadScene("Assets/NewScene.scene");
	}

	void Run(double dt) {
		NE_PROFILE_FUNCTION();
		//s_window->PollEvents();

		Physics::PhysicsManager::Update(static_cast<float>(dt));
		//Physics::Command::Update(static_cast<float>(dt));

		gSceneManager.Update(dt);

		Graphics::GraphicsManager::SubmitSkybox(); // Submit skybox once per frame

		gSceneManager.Render();

		Graphics::GraphicsManager::Clear(); // Clear draw commands after rendering

		TweenManager::Get().Update(static_cast<float>(dt));
		//Graphics::GraphicsManager::EndFrame();

		if (InputManager::WasKeyPressed('L')) {
			glfwSetInputMode(static_cast<GLFWwindow*>(s_window->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}

		//s_renderContext->SwapBuffers();
	}

	void Shutdown() {
		NE_PROFILE_FUNCTION();
		SaveCurrentScene("Assets/NewScene.scene");
		Physics::PhysicsManager::Shutdown();
		Graphics::GraphicsManager::Shutdown();
		//Physics::Command::Shutdown();
		

		gSceneManager.ExitScene();

		s_renderContext->Shutdown();
		Scripting::ScriptingEngine::GetInstance().Shutdown(); // needs to run after scriptsystem exit()
		s_renderContext.reset();
		s_window.reset();
	}

	void* GetNativeWindowHandle() {
		return s_window->GetNativeWindow();
	}

	bool WindowShouldClose()
	{
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

	void SaveCurrentScene(std::string path) {
		auto* editorScene = gSceneManager.GetEditorScene();
		if (!editorScene) return;
		
		SPD_INFO("[DirtyFlag] SaveCurrentScene called - Saving to: {}", path);

		// CRITICAL: Capture current field values from script instances before serializing
		// This ensures any changes made in the editor inspector are persisted
		auto& entities = editorScene->GetECSCoordinator().GetComponentManager().GetEntitiesWithComponent<ECS::Component::NativeScript>();
		for (NE::ECS::Entity entity : entities) {
			auto& nsc = editorScene->GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
			if (nsc.Instance) {
				Scripting::ScriptingEngine::GetInstance().SaveSerializedFields(nsc);
			}
		}

		Serialization::JsonSceneSerializer::Serialize(*editorScene, path);
		editorScene->ClearDirty();
		SPD_INFO("[DirtyFlag] Scene saved and marked as CLEAN");
	}

	void SaveSceneIfDirty(std::string path) {
		gSceneManager.SaveSceneIfDirty(path);
	}

	bool IsSceneDirty() {
		auto* editorScene = gSceneManager.GetEditorScene();
		return editorScene ? editorScene->IsDirty() : false;
	}

	void MarkSceneDirty() {
		auto* editorScene = gSceneManager.GetEditorScene();
		if (editorScene) {
			editorScene->MarkDirty();
		}
	}

	void LoadTargetScene(std::string targetPath) {
		Serialization::JsonSceneSerializer::Deserialize(*gSceneManager.GetActive(), targetPath);
	}

	const std::vector<uint32_t>& GetNumEntities() {
		return gSceneManager.GetActive()->GetECSCoordinator().GetUsedEntities();
	}

	std::string SerializePrefab(uint32_t entt, std::string targetPath) {
		return Serialization::JsonSceneSerializer::SerializePrefab(*gSceneManager.GetActive(), entt, targetPath);
	}

	 std::vector<uint32_t> DeserializePrefab(std::string prefabPath) {
		 auto newEntities = Serialization::JsonSceneSerializer::DeserializePrefab(*gSceneManager.GetActive(), prefabPath);
		 return newEntities;
	}

	std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid) {
		auto newEntities = Serialization::JsonSceneSerializer::DeserializePrefab(*gSceneManager.GetActive(), prefabPath);
		Prefab::PrefabManager::Instantiate(uuid, newEntities);
		return newEntities;
	}

	std::vector<uint32_t> DeserializePrefab(std::string prefabPath, std::string uuid, Math::Vec3 pos) {
		auto newEntities = Serialization::JsonSceneSerializer::DeserializePrefab(*gSceneManager.GetActive(), prefabPath, pos);
		Prefab::PrefabManager::Instantiate(uuid, newEntities);
		return newEntities;
	}

	void LoadPrefabScene(std::string prefabPath) {
		gSceneManager.LoadPrefabScene(prefabPath);
	}

	void SavePrefabScene(std::string prefabPath) {
		Serialization::JsonSceneSerializer::Serialize(*gSceneManager.GetActive(), prefabPath);
	}

	void ReloadAllInstancesOfPrefab(std::string prefabUUID, std::string prefabPath) {
		NE::Prefab::PrefabManager::ReloadAllInstancesOfPrefab(prefabUUID, prefabPath);
	}

	void ClosePrefabScene() {
		gSceneManager.ClosePrefabScene();
	}

	std::vector<uint32_t> DuplicateEntity(uint32_t entity) {
		std::vector<uint8_t> buffer;
		NE::Serialization::JsonSceneSerializer::SerializePrefabToMemory(*gSceneManager.GetActive(), entity, buffer);

		if (buffer.empty())
			return std::vector<uint32_t>{};

		auto newEntities =
			NE::Serialization::JsonSceneSerializer::DeserializePrefabFromMemory(*gSceneManager.GetActive(), buffer);

		if (newEntities.empty())
			return std::vector<uint32_t>{};

		auto& transform = gSceneManager.
			GetActive()->GetECSCoordinator().
			GetComponent<ECS::Component::Transform>(newEntities[0]);

		transform.localPosition.x += 0.5f;
		transform.localPosition.y += 0.5f;
		transform.localPosition.z += 0.5f;
		transform.isDirty = true;

		return newEntities;
	}

	std::vector<uint8_t> CopyEntity(uint32_t entity) {
		std::vector<uint8_t> buffer;
		NE::Serialization::JsonSceneSerializer::SerializePrefabToMemory(*gSceneManager.GetActive(), entity, buffer);
		return buffer;
	}

	std::vector<uint32_t> PasteEntity(std::vector<uint8_t> clipboard) {
		auto newEntities =
			NE::Serialization::JsonSceneSerializer::DeserializePrefabFromMemory(*gSceneManager.GetActive(), clipboard);

		//auto& transform = gSceneManager.
		//	GetActive()->GetECSCoordinator().
		//	GetComponent<ECS::Component::Transform>(newEntities[0]);

		//transform.localPosition = pos;
		//transform.isDirty = true;

		return newEntities;
	}

	// Internal use only
	SceneManagement::Scene& GetScene() {
		return *gSceneManager.GetActive();
	}

	std::shared_ptr<NE::Graphics::Material> LoadMaterial(std::string uuid) {
		return Resource::ResourceManager::GetInstance().LoadResource<NE::Graphics::Material>(uuid);
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
		h.stagesMask = ((shaderStages.count(GL_VERTEX_SHADER) ? 1 : 0) << 0)
			| ((shaderStages.count(GL_FRAGMENT_SHADER) ? 1 : 0) << 4);
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

		if (embedSourceFallback) {
			const auto& vs = shaderStages.at(GL_VERTEX_SHADER);
			const auto& fs = shaderStages.at(GL_FRAGMENT_SHADER);
			uint32_t vsLen = static_cast<uint32_t>(vs.size());
			uint32_t fsLen = static_cast<uint32_t>(fs.size());
			ofs.write(reinterpret_cast<const char*>(&vsLen), 4); ofs.write(vs.data(), vsLen);
			ofs.write(reinterpret_cast<const char*>(&fsLen), 4); ofs.write(fs.data(), fsLen);
		}

		glDeleteProgram(linkedProgram);
		return ofs.good();
	}

	void EditorPlay() {
		g_EngineState = EngineState::Play;
		NE::Physics::PhysicsManager::ClearAllBodies();
		gSceneManager.BeginPlay();
		Physics::PhysicsManager::ActivateBodies();
		glfwSetInputMode(static_cast<GLFWwindow*>(s_window->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	void EditorPause() {
		g_EngineState = EngineState::Play;
		Physics::PhysicsManager::DeactivateBodies();
		//Physics::Command::DeactivateBodies();
		gSceneManager.GetActive()->ScriptPause();
	}

	void EditorEdit() {
		g_EngineState = EngineState::Edit;
		Physics::PhysicsManager::DeactivateBodies(); 
		NE::Physics::PhysicsManager::ClearAllBodies(); // to change to create body on play and clear on stop once
		gSceneManager.StopPlay();
	}

	int GetDrawCallCount() {
		return Graphics::GraphicsManager::drawCount;
	}
}