#pragma once

#include "IPanel.hpp"
#include <filesystem>
#include <unordered_map>
#include <string>

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
		void CreateNewScene();
		void DeleteAssetWithMeta(const std::filesystem::path& assetPath);
		void MoveAssetWithMeta(const std::filesystem::path& source, const std::filesystem::path& destination);

		void GotoAssetFolder(const std::string& assetPath);

		std::filesystem::path m_rootDirectory;
		std::filesystem::path m_currentDirectory;

		char m_searchBuffer[256] = { 0 };

		bool m_isRenaming = false;
		bool m_triggerRenameNextFrame = false;
		std::filesystem::path m_renamingPath;
		bool m_clickedOnItem = false;
		std::filesystem::path m_selectedPath; // to remove, use EditorScene::selectedAsset
		bool m_renamePopupOpen = false;
		char m_renameBuffer[256] = { 0 };
		bool m_confirmDeletePopupOpen = false;
		bool m_confirmChangeScenePopupOpen = false;
		bool m_selectedItemClickedThisFrame = false;

		// Drag and drop state for moving files
		std::filesystem::path m_draggedAssetPath;
		bool m_isDraggingAsset = false;

		std::filesystem::path m_openMeshPath;
		bool m_openSubmeshPopup = false;
		//std::unordered_map<std::string, bool> m_meshExpanded; // for per directory
	};
}
