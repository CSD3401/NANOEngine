#pragma once

#include "IPanel.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <chrono>

namespace Editor {
	class ScriptsPanel : public IPanel {
	public:
		ScriptsPanel(const std::filesystem::path& scriptsDirectory);
		~ScriptsPanel() override;

		virtual void OnImGuiRender() override;

	private:
		// Rendering methods
		void RenderScriptsList();
		void RenderHotReloadStatus();
		void RenderCompileOutput();
		void RenderPopups();

		// File operations
		void CreateNewScript(const std::string& scriptName);
		void DeleteScript(const std::filesystem::path& scriptPath);
		void RenameScript(const std::filesystem::path& oldPath, const std::string& newName);
		void OpenScriptInEditor(const std::filesystem::path& scriptPath);
		void RefreshScriptsList();

		// Script template generation
		std::string GenerateScriptTemplate(const std::string& className);

		// Helper methods
		bool IsScriptFile(const std::filesystem::path& path) const;
		std::string GetScriptClassName(const std::filesystem::path& path) const;
		void OpenDirectoryInFileExplorer(const std::string& directoryPath);

		// Data members
		std::filesystem::path m_scriptsDirectory;
		std::vector<std::filesystem::path> m_scriptFiles;

		// UI state
		char m_searchBuffer[256] = { 0 };
		char m_newScriptNameBuffer[256] = { 0 };
		char m_renameBuffer[256] = { 0 };

		// Selection state
		std::filesystem::path m_selectedScript;
		bool m_isRenaming = false;
		std::filesystem::path m_renamingScript;

		// Popup state
		bool m_showCreateScriptPopup = false;
		bool m_showDeleteConfirmPopup = false;
		bool m_showRenamePopup = false;

		// Hot reload status tracking
		struct CompileInfo {
			bool isCompiling = false;
			bool lastCompileSuccess = true;
			std::string lastError;
			std::chrono::steady_clock::time_point lastCompileTime;
			int compileCount = 0;
		};
		CompileInfo m_compileInfo;

		// Compile output buffer for displaying errors
		std::vector<std::string> m_compileOutput;
	};
}
