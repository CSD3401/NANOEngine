#pragma once

#include "IPanel.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include "../ThumbnailManager.hpp"

namespace Editor {
	class AssetBrowserPanel : public IPanel {
	public:
		AssetBrowserPanel(const std::filesystem::path& root);
		~AssetBrowserPanel() override;

		virtual void OnImGuiRender() override;

	private:
		void RenderBreadcrumbs();
		void RenderDirectoryTree(const std::filesystem::path& path);
		void RenderDirectoryContents(const std::filesystem::path& path);
		void RenderPopups();
		void OpenDirectoryInFileExplorer(const std::string& directoryPath);

		void CreateNewFolder();
		void CreateNewMaterial();

		std::filesystem::path m_rootDirectory;
		std::filesystem::path m_currentDirectory;

		char m_searchBuffer[256] = { 0 };

		bool m_isRenaming = false;
		bool m_triggerRenameNextFrame = false;
		std::filesystem::path m_renamingPath;
		bool m_clickedOnItem = false;
		std::filesystem::path m_selectedPath;
		bool m_renamePopupOpen = false;
		char m_renameBuffer[256] = { 0 };
		bool m_confirmDeletePopupOpen = false;

		ThumbnailManager m_thumbnailManager;
	};
}
