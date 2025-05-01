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

		std::filesystem::path m_rootDirectory;
		std::filesystem::path m_currentDirectory;

		char m_searchBuffer[256] = { 0 };

		bool m_ClickedOnItem = false;
		std::filesystem::path m_SelectedPath;
		bool m_RenamePopupOpen = false;
		char m_RenameBuffer[256] = { 0 };
		bool m_ConfirmDeletePopupOpen = false;

		ThumbnailManager m_thumbnailManager;
		//GLuint m_DirectoryIcon = 0;
		//GLuint m_FileIcon = 0;
	};
}
