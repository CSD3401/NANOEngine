#include "Engine.hpp"

#include <memory>
#include <fstream>
#include <filesystem>
#include "Graphics/Core/Window.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/OpenGL/GLFrameBuffer.hpp"
#include "Core/Profiler.hpp"
#include "SceneManagement/Scene.hpp"
//#include "../../src/Serialisation/JsonSceneSerializer.hpp"
#include "ECS/Components/Transform.hpp"
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
#include "Serialisation/Serializer.hpp"
#include "ResourceManagement/ResourcePaths.hpp"
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

		if (InputManager::WasKeyPressed(GLFW_KEY_ESCAPE)) {
			glfwSetInputMode(static_cast<GLFWwindow*>(s_window->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

	void Shutdown() {
		NE_PROFILE_FUNCTION();
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

	void LoadScene(const std::string& _artifactPath) {
		gSceneManager.LoadScene(Resource::ComputeArtifactPathFromUUID(_artifactPath, Resource::ResourceType::Scene));
	}

	const std::vector<uint32_t>& GetNumEntities() {
		return gSceneManager.GetActive()->GetECSCoordinator().GetUsedEntities();
	}

	std::string SerializePrefab(uint32_t entt, std::string targetPath) {
		//return Serialization::JsonSceneSerializer::SerializePrefab(*gSceneManager.GetActive(), entt, targetPath);
		return "";
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

	void LoadPrefabScene(std::string prefabPath) {
		gSceneManager.LoadPrefabScene(prefabPath);
	}

	void SavePrefabScene(std::string prefabPath) {
		//Serialization::JsonSceneSerializer::Serialize(*gSceneManager.GetActive(), prefabPath);
	}

	void ReloadAllInstancesOfPrefab(std::string prefabUUID, std::string prefabPath) {
		NE::Prefab::PrefabManager::ReloadAllInstancesOfPrefab(prefabUUID, prefabPath);
	}

	void ClosePrefabScene() {
		gSceneManager.ClosePrefabScene();
	}

	std::vector<uint32_t> DuplicateEntity(uint32_t entity) {
		//std::vector<uint8_t> buffer;
		//NE::Serialization::JsonSceneSerializer::SerializePrefabToMemory(*gSceneManager.GetActive(), entity, buffer);

		//if (buffer.empty())
		//	return std::vector<uint32_t>{};

		//auto newEntities =
		//	NE::Serialization::JsonSceneSerializer::DeserializePrefabFromMemory(*gSceneManager.GetActive(), buffer);

		//if (newEntities.empty())
		//	return std::vector<uint32_t>{};

		//auto& transform = gSceneManager.
		//	GetActive()->GetECSCoordinator().
		//	GetComponent<ECS::Component::Transform>(newEntities[0]);

		//transform.localPosition.x += 0.5f;
		//transform.localPosition.y += 0.5f;
		//transform.localPosition.z += 0.5f;
		//transform.isDirty = true;

		//return newEntities;
		return std::vector<uint32_t>();
	}

	std::vector<uint8_t> CopyEntity(uint32_t entity) {
		//std::vector<uint8_t> buffer;
		//NE::Serialization::JsonSceneSerializer::SerializePrefabToMemory(*gSceneManager.GetActive(), entity, buffer);
		//return buffer;
		return std::vector<uint8_t>();
	}

	std::vector<uint32_t> PasteEntity(std::vector<uint8_t> clipboard) {
		//auto newEntities =
		//	NE::Serialization::JsonSceneSerializer::DeserializePrefabFromMemory(*gSceneManager.GetActive(), clipboard);

		//return newEntities;
		return std::vector<uint32_t>();
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

	//void EditorPlay() {
	//	g_EngineState = EngineState::Play;
	//	gSceneManager.BeginPlay();
	//	glfwSetInputMode(static_cast<GLFWwindow*>(s_window->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	//}

	//void EditorPause() {
	//	g_EngineState = EngineState::Play;
	//	gSceneManager.GetActive()->ScriptPause();
	//}

	//void EditorEdit() {
	//	g_EngineState = EngineState::Edit;
	//	gSceneManager.StopPlay();
	//}

	int GetDrawCallCount() {
		return Graphics::GraphicsManager::drawCount;
	}

	void DisplayFinalOutput(int windowWidth, int windowHeight) {
		Graphics::GraphicsManager::DisplayFinalOutput(windowWidth, windowHeight);
	}
}