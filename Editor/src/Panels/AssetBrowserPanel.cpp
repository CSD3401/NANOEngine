#include "AssetBrowserPanel.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <Engine.hpp>
#include <ECSInternals.hpp>
#include "../../src/EditorScene.hpp"
#include <Utility/MetadataHandler.hpp>

namespace Editor {
	AssetBrowserPanel::AssetBrowserPanel(const std::filesystem::path& root) 
		: m_rootDirectory(root), m_currentDirectory(root)
	{
        m_thumbnailManager.Init();
        //m_DirectoryIcon = m_thumbnailManager.GetThumbnail("Resources/Icons/icon_folder.png");
        //m_FileIcon = m_thumbnailManager.GetThumbnail("Resources/Icons/icon_file.png");

        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            std::filesystem::path filePath = entry.path();

            // Skip meta files themselves
            if (filePath.extension() == ".meta") continue;

            if (!NE::Utility::MetadataHandler::MetaFileExists(entry.path().string())) {
                NE::Utility::MetadataHandler::GenerateMetaFile(entry.path().string());
            }

            if (filePath.extension() == ".nanoshader") {
                NE::LoadShader(filePath.string());
            }
        }
	}

    AssetBrowserPanel::~AssetBrowserPanel() {
        m_thumbnailManager.Shutdown();
    }

	void AssetBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin("Asset Browser", nullptr, ImGuiWindowFlags_MenuBar);

        // Search bar
        ImGui::BeginMenuBar();
        ImGui::InputTextWithHint("##Search", "Search...", m_searchBuffer, sizeof(m_searchBuffer));
        ImGui::EndMenuBar();

        // Directory Tree
		ImGui::BeginChild("DirectoryTree", ImVec2(200, 0), false);
		RenderDirectoryTree(m_rootDirectory);
		ImGui::EndChild();

		ImGui::SameLine();

        // Directory Contents
        ImGui::BeginChild("Breadcrumbs", ImVec2(0, 0), true);
        RenderBreadcrumbs();

        ImGui::Separator();

        ImGui::BeginChild("DirectoryContents", ImVec2(0, 0), false);
        RenderDirectoryContents(m_currentDirectory);
        RenderPopups();
        ImGui::EndChild();

        ImGui::EndChild();

		ImGui::End();


        // Popup utils
        //if (m_renamePopupOpen) {
        //    ImGui::OpenPopup("Rename Asset");
        //    m_renamePopupOpen = false;
        //}
        //if (ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        //    ImGui::InputText("##NewName", m_renameBuffer, sizeof(m_renameBuffer));

        //    if (ImGui::Button("Rename")) {
        //        std::filesystem::path newPath = m_selectedPath.parent_path() / m_renameBuffer;
        //        std::error_code ec;
        //        std::filesystem::rename(m_selectedPath, newPath, ec);
        //        if (!ec) {
        //            m_selectedPath.clear();
        //        }
        //        ImGui::CloseCurrentPopup();
        //    }
        //    ImGui::SameLine();
        //    if (ImGui::Button("Cancel")) {
        //        ImGui::CloseCurrentPopup();
        //    }

        //    ImGui::EndPopup();
        //}

        if (m_confirmDeletePopupOpen) {
            ImGui::OpenPopup("Confirm Delete");
            m_confirmDeletePopupOpen = false;
        }
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete this file/folder?");
            ImGui::Separator();

            if (ImGui::Button("Delete")) {
                std::error_code ec;
                if (std::filesystem::is_directory(m_selectedPath))
                    std::filesystem::remove_all(m_selectedPath, ec);
                else
                    std::filesystem::remove(m_selectedPath, ec);

                m_selectedPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
	}

    void AssetBrowserPanel::RenderBreadcrumbs() {
        std::filesystem::path pathWalk = m_rootDirectory;

        auto drawClickableText = [&](const std::string& label, const std::filesystem::path& pathToSet) {
            ImGui::PushID(label.c_str());

            // Get mouse hover
            //ImVec2 min = ImGui::GetCursorScreenPos(); // warning unused var - RF
            ImGui::TextUnformatted(label.c_str());
            //ImVec2 max = ImGui::GetCursorScreenPos(); // warning unused var - RF

            ImVec2 textMin = ImGui::GetItemRectMin();
            ImVec2 textMax = ImGui::GetItemRectMax();

            bool hovered = ImGui::IsMouseHoveringRect(textMin, textMax);

            if (hovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImU32 color = hovered
                ? ImGui::GetColorU32(ImVec4(0.3f, 0.7f, 1.0f, 1.0f))
                : ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddText(textMin, color, label.c_str());

            if (hovered && ImGui::IsMouseClicked(0)) {
                m_currentDirectory = pathToSet;
            }

            ImGui::PopID();
        };

        std::string rootName;
        if (!m_rootDirectory.filename().empty()) {
            rootName = m_rootDirectory.filename().string();
        } else if (!m_rootDirectory.has_parent_path()) {
            rootName = m_rootDirectory.string();
        } else {
            rootName = m_rootDirectory.parent_path().filename().string();
        }

        if (rootName.empty()) {
            rootName = "Root";
        }

        drawClickableText(rootName, m_rootDirectory);

        std::filesystem::path relativePath = m_currentDirectory.lexically_relative(m_rootDirectory);
        if (relativePath == "." || relativePath.empty())
            return;

        for (const auto& part : m_currentDirectory.lexically_relative(m_rootDirectory)) {
            pathWalk /= part;

            ImGui::SameLine();
            ImGui::TextUnformatted(">");
            ImGui::SameLine();

            drawClickableText(part.string(), pathWalk);
        }
    }

    void AssetBrowserPanel::RenderDirectoryTree(const std::filesystem::path& path) {
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory()) {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
                bool opened = ImGui::TreeNodeEx(entry.path().filename().string().c_str(), flags);

                if (ImGui::IsItemClicked()) {
                    m_currentDirectory = entry.path();
                }

                if (opened) {
                    RenderDirectoryTree(entry.path());
                    ImGui::TreePop();
                }
            }
        }
    }

    void AssetBrowserPanel::RenderDirectoryContents(const std::filesystem::path& path) {
        float padding = 16.0f;
        float thumbnailSize = 64.0f;
        float cellSize = thumbnailSize + padding;

        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1) columnCount = 1; // NOLINT 

        ImGui::Columns(columnCount, 0, false);

        for (auto& entry : std::filesystem::directory_iterator(path)) {
            const auto& name = entry.path().filename().string();

			if (entry.path().extension() == ".meta") continue; // Skip metadata files

            // Search filter
            if (strlen(m_searchBuffer) > 0) {
                std::string lowerName = name;
                std::string lowerSearch = m_searchBuffer;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                if (lowerName.find(lowerSearch) == std::string::npos)
                    continue;
            }

            ImGui::PushID(name.c_str());

            GLuint iconTexture = entry.is_directory() ? m_thumbnailManager.m_DirectoryIcon : m_thumbnailManager.m_FileIcon;

            // Draw icon button
            ImGui::ImageButton("##btn",
                (ImTextureID)(intptr_t)iconTexture,
                ImVec2(thumbnailSize, thumbnailSize),
                ImVec2(1, 0), ImVec2(0, 1)
            );

            // Drag and drop assets
            if (!entry.is_directory()) {
                const auto& entryPath = entry.path();
                if (entryPath.extension() == ".obj" || entryPath.extension() == ".fbx") {
                    if (ImGui::BeginDragDropSource()) {
                        std::string assetPath = entry.path().string();
                        ImGui::SetDragDropPayload("ASSET_PATH", assetPath.c_str(), assetPath.size() + 1);
                        ImGui::TextUnformatted(name.c_str());
                        ImGui::EndDragDropSource();
                    }
                } else if (entryPath.extension() == ".scene") {    
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        NE::LoadTargetScene(entryPath.string());

                        for (const auto& entt : NE::GetEntities()) {
                            EditorScene::s_entities.push_back({ entt });
                        }
                    }
                } else if (entryPath.extension() == ".nanomat") {
                    if (ImGui::BeginDragDropSource()) {
                        std::string assetPath = entry.path().string();
                        ImGui::SetDragDropPayload("MATERIAL_PATH", assetPath.c_str(), assetPath.size() + 1);
                        ImGui::TextUnformatted(name.c_str());
                        ImGui::EndDragDropSource();
                    } else if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        EditorScene::s_selectedEntity = nullptr;
                        EditorScene::selectedMaterial = entryPath.string();
                    }
                }
            }

            // Handle double click and right click
            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (entry.is_directory())
                        m_currentDirectory = entry.path();
                    else {
                        // TODO: Open file action
                    }
                }
                else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                    m_selectedPath = entry.path();
                    m_clickedOnItem = true;
                    ImGui::OpenPopup("AssetContextMenu");
                }
            }

            // Renaming
            if (m_isRenaming && m_renamingPath == entry.path()) {
                ImGui::SetNextItemWidth(thumbnailSize * 1.5f);
                ImGui::SetKeyboardFocusHere();

                if (ImGui::InputText("##RenameInput", m_renameBuffer, sizeof(m_renameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                    
                    std::filesystem::path newPath = entry.path().parent_path() / m_renameBuffer;
                    std::error_code ec;
                    std::filesystem::rename(entry.path(), newPath, ec);
                    m_isRenaming = false;
                }

                // Cancel if clicked away or lost focus
                if (!ImGui::IsItemActive() && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))) {
                    m_isRenaming = false;
                }
            }
            else {
                ImGui::TextWrapped("%s", name.c_str());

                if (m_triggerRenameNextFrame && m_selectedPath == entry.path()) {
                    m_isRenaming = true;
                    m_renamingPath = entry.path();
                    m_triggerRenameNextFrame = false;
                    strncpy_s(m_renameBuffer, name.c_str(), sizeof(m_renameBuffer));
                }
            }

            ImGui::NextColumn();
            ImGui::PopID();
        }

        ImGui::Columns(1);

        if (m_triggerRenameNextFrame) {
            m_triggerRenameNextFrame = false;
        }
    }

    void AssetBrowserPanel::RenderPopups() {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            if (!ImGui::IsAnyItemHovered()) {
                m_selectedPath.clear();
                m_clickedOnItem = false;
                ImGui::OpenPopup("AssetContextMenu");
            }
        }

        if (ImGui::BeginPopupContextWindow("AssetContextMenu")) {
            if (ImGui::MenuItem("Create New Folder")) {
                CreateNewFolder();
            }

            if (ImGui::MenuItem("Create New Scene")) {
                //CreateNewFile();
            }

            ImGui::Separator();

            if (m_clickedOnItem) {
                if (ImGui::MenuItem("Rename")) {
                    m_triggerRenameNextFrame = true;
                    m_renamingPath = m_selectedPath;
                    strncpy_s(m_renameBuffer, m_selectedPath.filename().string().c_str(), sizeof(m_renameBuffer));
                }

                if (ImGui::MenuItem("Delete")) {
                    m_confirmDeletePopupOpen = true;
                }
            }
            else {
                ImGui::BeginDisabled();
                ImGui::MenuItem("Rename");
                ImGui::MenuItem("Delete");
                ImGui::EndDisabled();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Open in File Explorer")) {
                OpenDirectoryInFileExplorer(m_currentDirectory.relative_path().string());
            }

            ImGui::EndPopup();
        }
    }

    void AssetBrowserPanel::OpenDirectoryInFileExplorer(const std::string& directoryPath) {
        std::filesystem::path fullPath = std::filesystem::absolute(directoryPath);
        std::string command = "explorer \"" + fullPath.string() + "\"";
        std::system(command.c_str());
    }

    void AssetBrowserPanel::CreateNewFolder() {
        std::filesystem::path newFolderPath = m_currentDirectory / "New Folder";

        int counter = 1;
        while (std::filesystem::exists(newFolderPath)) {
            newFolderPath = m_currentDirectory / ("New Folder (" + std::to_string(counter) + ")");
            counter++;
        }

        std::filesystem::create_directory(newFolderPath);
    }
}

