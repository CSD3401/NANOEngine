#include "AssetBrowserPanel.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>


namespace Editor {
	AssetBrowserPanel::AssetBrowserPanel(const std::filesystem::path& root) 
		: m_rootDirectory(root), m_currentDirectory(root)
	{
        m_thumbnailManager.Init();
        //m_DirectoryIcon = m_thumbnailManager.GetThumbnail("Resources/Icons/icon_folder.png");
        //m_FileIcon = m_thumbnailManager.GetThumbnail("Resources/Icons/icon_file.png");
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
        if (m_RenamePopupOpen) {
            ImGui::OpenPopup("Rename Asset");
            m_RenamePopupOpen = false;
        }
        if (ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("##NewName", m_RenameBuffer, sizeof(m_RenameBuffer));

            if (ImGui::Button("Rename")) {
                std::filesystem::path newPath = m_SelectedPath.parent_path() / m_RenameBuffer;
                std::error_code ec;
                std::filesystem::rename(m_SelectedPath, newPath, ec);
                if (!ec) {
                    m_SelectedPath.clear();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (m_ConfirmDeletePopupOpen) {
            ImGui::OpenPopup("Confirm Delete");
            m_ConfirmDeletePopupOpen = false;
        }
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete this file/folder?");
            ImGui::Separator();

            if (ImGui::Button("Delete")) {
                std::error_code ec;
                if (std::filesystem::is_directory(m_SelectedPath))
                    std::filesystem::remove_all(m_SelectedPath, ec);
                else
                    std::filesystem::remove(m_SelectedPath, ec);

                m_SelectedPath.clear();
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
            ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::TextUnformatted(label.c_str());
            ImVec2 max = ImGui::GetCursorScreenPos();

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
        if (columnCount < 1) columnCount = 1;

        ImGui::Columns(columnCount, 0, false);

        for (auto& entry : std::filesystem::directory_iterator(path)) {
            const auto& name = entry.path().filename().string();

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

            // --- New part using ImGui::Image() ---
            ImGui::ImageButton("##btn",
                (ImTextureID)(intptr_t)iconTexture,
                ImVec2(thumbnailSize, thumbnailSize),
                ImVec2(1, 0), ImVec2(0, 1) // Flip UV if needed
            );

            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    if (entry.is_directory())
                        m_currentDirectory = entry.path();
                    else {
                        // TODO: Open file action
                    }
                }
                else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                    m_SelectedPath = entry.path();
                    m_ClickedOnItem = true;
                    ImGui::OpenPopup("AssetContextMenu");
                }
            }

            ImGui::TextWrapped(name.c_str());

            ImGui::NextColumn();
            ImGui::PopID();
        }

        ImGui::Columns(1);
    }

    void AssetBrowserPanel::RenderPopups() {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            if (!ImGui::IsAnyItemHovered()) {
                m_SelectedPath.clear();
                m_ClickedOnItem = false;
                ImGui::OpenPopup("AssetContextMenu");
            }
        }

        if (ImGui::BeginPopupContextWindow("AssetContextMenu")) {
            if (ImGui::MenuItem("Create New Folder")) {
                //CreateNewFolder();
            }

            if (ImGui::MenuItem("Create New Scene")) {
                //CreateNewFile();
            }

            ImGui::Separator();

            if (m_ClickedOnItem) {
                if (ImGui::MenuItem("Rename")) {
                    m_RenamePopupOpen = true;
                    memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
                    std::string currentName = m_SelectedPath.filename().string();
                    strncpy_s(m_RenameBuffer, currentName.c_str(), sizeof(m_RenameBuffer));
                }

                if (ImGui::MenuItem("Delete")) {
                    m_ConfirmDeletePopupOpen = true;
                }
            } else {
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
}

