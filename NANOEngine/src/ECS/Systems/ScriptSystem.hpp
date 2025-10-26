#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../src/Scripting/ScriptingEngine.hpp"
#include "FileWatch.hpp"
#include <queue>
#include <mutex>
#include <atomic>


namespace NE::ECS::Systems {

	class ScriptSystem final : public System {
	public:
		explicit ScriptSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override;
		void Exit() override;

		void StartScripts(); //Play
		void PauseScripts(); //Pause
		void StopScripts(); //Stop

		void OnScriptComponentDestroyed(Entity entity);

		Scripting::ScriptingEngine* GetScriptingEngine() const { return scriptingEngine.get(); }

	private:
		ComponentManager* m_componentManager;
		std::unique_ptr<Scripting::ScriptingEngine> scriptingEngine; // Pointer to the scripting engine

		// Hot Reloading
		std::unique_ptr<filewatch::FileWatch<std::string>> m_sourceWatcher;
		std::string m_scriptDLLPath;           // Path to the final DLL (e.g., "GameCode.dll")
		std::string m_scriptSourceDirectory;   // Path to watch for .cpp/hpp (e.g., "../GameCode/src")
		std::string m_scriptBuildCommand;      // The command to run (e.g., "build_scripts.bat")

		// Thread-safe flag to request a compile
		std::atomic<bool> m_compileQueued = false;

		// Counter to create unique filenames for hot-reloading
		int m_hotReloadCounter = 0;
		std::string m_currentLoadedDLLPath; // The path to the DLL *actually* loaded

		/**
		 * @brief Handles the file watcher event from the watcher's thread.
		 */
		void HandleFileWatchEvent(const std::string& path, const filewatch::Event eventType);

		/**
		 * @brief Runs the compile and then triggers the hot reload.
		 */
		void HotCompileAndReload();

		/**
		 * @brief The core hot-reloading logic.
		 */
		void HotReloadDLL(const std::string& oldDllPath, const std::string& newDllPath);
	};

}