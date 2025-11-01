#include "Engine.hpp"

#include <memory>
#include <fstream>
#include "Graphics/Core/Window.hpp"
#include "Graphics/OpenGL/GLContext.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/OpenGL/GLFrameBuffer.hpp"
#include "Core/Profiler.hpp"
//#include <glad/glad.h>
#include "SceneManagement/Scene.hpp"
#include "../../src/Serialisation/JsonSceneSerializer.hpp"
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
	static std::unique_ptr<Graphics::IFrameBuffer> s_sceneFrameBuffer; // temp
	static std::unique_ptr<Graphics::IFrameBuffer> s_pickingFrameBuffer; // temp

	static SceneManagement::SceneManager gSceneManager;

	void Initialize() {
		NE_PROFILE_FUNCTION();
		Graphics::WindowProperties props;
		props.Title = "NANOEngine";
		props.Width = 1920;
		props.Height = 1080;
		props.VSync = true;

		s_window = std::make_unique<Graphics::Window>(props);
		s_renderContext = std::make_unique<Graphics::OpenGL::GLContext>();
		s_renderContext->Init(s_window->GetNativeWindow());

		s_sceneFrameBuffer = std::make_unique<Graphics::OpenGL::GLFrameBuffer>(1920, 1080);
		s_pickingFrameBuffer = std::make_unique<Graphics::OpenGL::GLFrameBuffer>(1920, 1080);
		Graphics::GraphicsManager::Init();
		Physics::PhysicsManager::Init();
		//Physics::PhysicsManager::TestPhysicsSetup();
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

		s_sceneFrameBuffer->Bind();
		Graphics::GraphicsManager::BeginFrame();
		gSceneManager.Render(NE::SceneManagement::RenderPass::Main);
		TweenManager::Get().Update(static_cast<float>(dt));
		Graphics::GraphicsManager::EndFrame();
		s_sceneFrameBuffer->Unbind();
		
		s_pickingFrameBuffer->Bind();
		Graphics::GraphicsManager::BeginFrame();
		gSceneManager.Render(NE::SceneManagement::RenderPass::Picking);
		Graphics::GraphicsManager::EndFrame();
		s_pickingFrameBuffer->Unbind();

		//s_renderContext->SwapBuffers();
	}

	void Shutdown() {
		NE_PROFILE_FUNCTION();
		SaveCurrentScene("Assets/NewScene.scene");
		Physics::PhysicsManager::Shutdown();
		//Physics::Command::Shutdown();

		gSceneManager.ExitScene();

		s_sceneFrameBuffer.reset();
		s_pickingFrameBuffer.reset();
		s_renderContext->Shutdown();
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

	uint32_t GetSceneFrameBuffer() {
		return s_sceneFrameBuffer->GetColorAttachment();
	}

	void SetEditorCamera(void* camera) {
		Graphics::GraphicsManager::SetCamera(reinterpret_cast<Graphics::Camera*>(camera));
	}

	uint32_t GetPickedEntity(uint32_t x, uint32_t y) {
		return Graphics::GraphicsManager::ReadPixel(s_pickingFrameBuffer.get(), x, y);
	}

	void SaveCurrentScene(std::string path) {
		Serialization::JsonSceneSerializer::Serialize(*gSceneManager.GetActive(), path);
	}

	void LoadTargetScene(std::string targetPath) {
		Serialization::JsonSceneSerializer::Deserialize(*gSceneManager.GetActive(), targetPath);
	}

	//void LoadShader(std::string_view filePath) {
	//	//Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLShader>(filePath.data(), false);
	//}

	//void LoadTexture(std::string_view filePath) {
	//	//Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLTexture>(filePath.data(), false);
	//}

	//std::shared_ptr<Graphics::OpenGL::GLTexture> GetTexture(std::string_view filePath)
	//{
	//	//return Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLTexture>(filePath.data(), false);
	//}

	//std::shared_ptr<Graphics::Material> GetMaterial(std::string_view path) {
	//	//return Asset::AssetManager::GetInstance().Load<NE::Graphics::Material>(path.data(), false);
	//}

	//const std::vector<std::pair<std::string, std::shared_ptr<Graphics::Model>>>& GetAllModels()
	//{
	//	//return Asset::AssetManager::GetInstance().GetAssetsOfType<Graphics::Model>();
	//}

	//const std::vector<std::pair<std::string, std::shared_ptr<Graphics::OpenGL::GLShader>>>& GetAllShaders() {
	//	//return Asset::AssetManager::GetInstance().GetAssetsOfType<Graphics::OpenGL::GLShader>();
	//}
	//
	//const std::vector<std::pair<std::string, std::shared_ptr<Asset::AudioBank>>>& GetAllAudioBanks() {
	//	//return Asset::AssetManager::GetInstance().GetAssetsOfType<Asset::AudioBank>();
	//}

	


	size_t GetNumEntities() {
		return gSceneManager.GetActive()->GetECSCoordinator().GetUsedEntities().size();
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
		Physics::PhysicsManager::ActivateBodies();
		//Physics::Command::ActivateBodies();
		gSceneManager.GetActive()->ScriptStart();
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
		//Physics::Command::DeactivateBodies();
		gSceneManager.GetActive()->ScriptStop();
	}

	int GetDrawCallCount() {
		return Graphics::GraphicsManager::drawCount;
	}
}