#include "AssetBrowserPanel.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <Engine.hpp>
#include "../../src/EditorScene.hpp"
#include "../AssetManagement/AssetManager.hpp"
#include <Core/SpdLogger.hpp>
#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <ResourceManagement/ResourcePaths.hpp>

namespace Editor {
    AssetBrowserPanel::AssetBrowserPanel(const std::filesystem::path& root)
        : m_rootDirectory(root), m_currentDirectory(root)
    {
        m_thumbnailManager.Init();
        //m_DirectoryIcon = m_thumbnailManager.GetThumbnail("Resources/Icons/icon_folder.png");
        //m_FileIcon = m_thumbnailManager.GetThumbnail("Resources/Icons/icon_file.png");

        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            std::filesystem::path filePath = entry.path();

            if (filePath.extension() == ".meta") continue;

            Assets::AssetManager::GetInstance().GenerateMetadata(entry.path().string());
        }
    }

    AssetBrowserPanel::~AssetBrowserPanel() {
        m_thumbnailManager.Shutdown();
    }

	void AssetBrowserPanel::OnImGuiRender() {
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

        ImGuiWindow* child = ImGui::GetCurrentWindow();
        ImRect drop_rect = child->InnerRect;      // or child->ContentRegionRect depending on what you want

        if (ImGui::BeginDragDropTargetCustom(drop_rect, child->ID)) {
            auto* draw = child->DrawList;
            draw->AddRect(drop_rect.Min, drop_rect.Max,
                ImGui::GetColorU32(ImVec4(1, 1, 0, 0.3f)),
                0.0f, 0, 2.0f);
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("HIER_DRAG_ID")) {
                if (p->DataSize == sizeof(uint32_t)) {
                    uint32_t dropped = *static_cast<const uint32_t*>(p->Data);
                    
                    auto& meta = NE::ECS::Command::GetEntityMeta(dropped);
                    std::string prefabName = meta.name;
                    if (meta.name.empty())
                        prefabName = "Prefab";

                    std::string filePath = m_currentDirectory.string() + "/" + prefabName + ".nfab";
                    SPD_INFO("Prefab created at: " << filePath);
                    std::ofstream create(filePath, std::ios::binary | std::ios::trunc);
                    Assets::AssetManager::GetInstance().GenerateMetadata(filePath);
                    std::string prefabID = Assets::AssetManager::GetInstance().RetrieveUUID(filePath);
                    meta.prefabID = prefabID;

                    NE::SerializePrefab(dropped, filePath);
                }
            }
            ImGui::EndDragDropTarget();
        }

        RenderPopups();
        ImGui::EndChild();

        ImGui::EndChild();

		ImGui::End();

        if (m_confirmDeletePopupOpen) {
            ImGui::OpenPopup("Confirm Delete");
            m_confirmDeletePopupOpen = false;
        }
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Confirm file/folder deletion?");
            ImGui::Separator();

            if (ImGui::Button("Delete")) {
                DeleteAssetWithMeta(m_selectedPath);
                m_selectedPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (m_confirmChangeScenePopupOpen) {
            ImGui::OpenPopup("Confirm Change Scene");
            m_confirmChangeScenePopupOpen = false;
        }
        if (ImGui::BeginPopupModal("Confirm Change Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Confirm scene switch?");
            ImGui::Separator();

            if (ImGui::Button("Yes")) {
                auto uuid = Assets::AssetManager::GetInstance().RetrieveUUID(m_selectedPath.string());
                NE::LoadScene(uuid);
                EditorScene::BuildRoot();
                EditorScene::s_currentScenePath = m_selectedPath.string();
                EditorScene::s_currentSceneUUID = uuid;

                m_selectedPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void AssetBrowserPanel::RenderBreadcrumbs() {
        std::filesystem::path pathWalk = m_rootDirectory;

        auto drawClickableText = [&](const std::string& label, const std::filesystem::path& pathToSet) {
            ImGui::PushID(label.c_str());

            // Use a button with transparent background
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.7f, 1.0f, 0.5f));

            bool clicked = ImGui::Button(label.c_str());

            ImGui::PopStyleColor(3);

            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            if (clicked) {
                m_currentDirectory = pathToSet;
            }

            // Accept drag-and-drop for moving files into breadcrumb directories
            if (ImGui::BeginDragDropTarget()) {
                // Visual feedback during drag
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 rectMin = ImGui::GetItemRectMin();
                ImVec2 rectMax = ImGui::GetItemRectMax();
                drawList->AddRect(rectMin, rectMax, IM_COL32(100, 200, 255, 255), 0.0f, 0, 2.0f);

                // Accept multiple payload types
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MOVE");
                if (!payload) payload = ImGui::AcceptDragDropPayload("ASSET_MESH_PATH");
                if (!payload) payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH");
                if (!payload) payload = ImGui::AcceptDragDropPayload("TEXTURE_ASSET_PATH");

                if (payload) {
                    const char* pathStr = static_cast<const char*>(payload->Data);
                    std::filesystem::path sourcePath(pathStr);
                    std::filesystem::path destPath = pathToSet / sourcePath.filename();

                    if (sourcePath != destPath && sourcePath.parent_path() != pathToSet) {
                        MoveAssetWithMeta(sourcePath, destPath);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::PopID();
            };

        std::string rootName;
        if (!m_rootDirectory.filename().empty()) {
            rootName = m_rootDirectory.filename().string();
        }
        else if (!m_rootDirectory.has_parent_path()) {
            rootName = m_rootDirectory.string();
        }
        else {
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

                // Accept drag-and-drop for moving files into directories
                if (ImGui::BeginDragDropTarget()) {
                    // Visual feedback during drag
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 rectMin = ImGui::GetItemRectMin();
                    ImVec2 rectMax = ImGui::GetItemRectMax();
                    drawList->AddRect(rectMin, rectMax, IM_COL32(100, 200, 255, 255), 0.0f, 0, 2.0f);

                    // Accept multiple payload types
                    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MOVE");
                    if (!payload) payload = ImGui::AcceptDragDropPayload("ASSET_MESH_PATH");
                    if (!payload) payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH");
                    if (!payload) payload = ImGui::AcceptDragDropPayload("TEXTURE_ASSET_PATH");

                    if (payload) {
                        const char* pathStr = static_cast<const char*>(payload->Data);
                        std::filesystem::path sourcePath(pathStr);
                        std::filesystem::path destPath = entry.path() / sourcePath.filename();

                        if (sourcePath != destPath && sourcePath.parent_path() != entry.path()) {
                            MoveAssetWithMeta(sourcePath, destPath);
                        }
                    }
                    ImGui::EndDragDropTarget();
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

            // Universal drag source for moving files/folders
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                // Store path in member variable to ensure it persists
                m_draggedAssetPath = entry.path();
                std::string dragPathStr = m_draggedAssetPath.string();

                // Set the appropriate payload based on file type
                const auto& entryPath = entry.path();
                bool hasSpecialPayload = false;

                if (!entry.is_directory()) {
                    if (entryPath.extension() == ".obj" || entryPath.extension() == ".fbx") {
                        ImGui::SetDragDropPayload("ASSET_MESH_PATH", dragPathStr.c_str(), dragPathStr.size() + 1);
                        hasSpecialPayload = true;
                    }
                    else if (entryPath.extension() == ".nanomat") {
                        ImGui::SetDragDropPayload("MATERIAL_PATH", dragPathStr.c_str(), dragPathStr.size() + 1);
                        hasSpecialPayload = true;
                    }
                    else if (entryPath.extension() == ".jpg" || entryPath.extension() == ".png") {
                        ImGui::SetDragDropPayload("TEXTURE_ASSET_PATH", dragPathStr.c_str(), dragPathStr.size() + 1);
                        hasSpecialPayload = true;
                    }
                    else if (entryPath.extension() == ".nfab") {
                        ImGui::SetDragDropPayload("PREFAB_ASSET_PATH", dragPathStr.c_str(), dragPathStr.size() + 1);
                        hasSpecialPayload = true;
                    }
                }

                // If no special payload was set, use the generic ASSET_MOVE
                if (!hasSpecialPayload) {
                    ImGui::SetDragDropPayload("ASSET_MOVE", dragPathStr.c_str(), dragPathStr.size() + 1);
                }

                ImGui::Text("Move: %s", name.c_str());
                ImGui::EndDragDropSource();
            }

            // Handle double-click for file types
            if (!entry.is_directory()) {
                const auto& entryPath = entry.path();
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (entryPath.extension() == ".obj" || entryPath.extension() == ".fbx" ||
                        entryPath.extension() == ".nanomat" ||
                        entryPath.extension() == ".jpg" || entryPath.extension() == ".png") {
                        EditorScene::s_selection.Clear();
                        EditorScene::selectedAsset = entryPath.string();
                    } else if (entryPath.extension() == ".scene") {
                        m_selectedPath = entryPath;
                        m_confirmChangeScenePopupOpen = true;
                        // NE::LoadTargetScene(entryPath.string());
                        // for (const auto& entt : NE::GetEntities()) {
                        //     EditorScene::s_entities.push_back({ entt });
                        // }

                    } else if (entryPath.extension() == ".nfab") {
                        EditorScene::s_selection.Clear();
                        EditorScene::selectedAsset = "";
                        NE::LoadPrefabScene(entryPath.string());

                        Editor::EditorScene::selectedPrefab = entryPath.string();
                    }
                }
            }

            // Accept drops onto folders
            if (entry.is_directory()) {
                if (ImGui::BeginDragDropTarget()) {
                    // Accept multiple payload types
                    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MOVE");
                    if (!payload) payload = ImGui::AcceptDragDropPayload("ASSET_MESH_PATH");
                    if (!payload) payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH");
                    if (!payload) payload = ImGui::AcceptDragDropPayload("TEXTURE_ASSET_PATH");

                    if (payload) {
                        const char* pathStr = static_cast<const char*>(payload->Data);
                        std::filesystem::path sourcePath(pathStr);
                        std::filesystem::path destPath = entry.path() / sourcePath.filename();

                        if (sourcePath != destPath && sourcePath.parent_path() != entry.path()) {
                            MoveAssetWithMeta(sourcePath, destPath);
                        }
                    }
                    ImGui::EndDragDropTarget();
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
                    std::error_code ec;

                    // Old/new asset paths
                    std::filesystem::path oldAssetPath = entry.path();
                    std::filesystem::path newAssetPath = oldAssetPath.parent_path() / m_renameBuffer;

                    // Use the move function to handle both asset and meta
                    MoveAssetWithMeta(oldAssetPath, newAssetPath);

                    // Keep selection pointing at the new asset path
                    m_selectedPath = newAssetPath;
                    m_renamingPath = newAssetPath;

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

        ImGui::Columns();

        // Accept drops onto empty space in current directory
        // Draw an invisible button covering the remaining space
        ImVec2 availSpace = ImGui::GetContentRegionAvail();
        if (availSpace.y > 0) {
            ImGui::InvisibleButton("##DragDropArea", availSpace);

            if (ImGui::BeginDragDropTarget()) {
                // Accept multiple payload types
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MOVE");
                if (!payload) payload = ImGui::AcceptDragDropPayload("ASSET_MESH_PATH");
                if (!payload) payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH");
                if (!payload) payload = ImGui::AcceptDragDropPayload("TEXTURE_ASSET_PATH");

                if (payload) {
                    const char* pathStr = static_cast<const char*>(payload->Data);
                    std::filesystem::path sourcePath(pathStr);
                    std::filesystem::path destPath = m_currentDirectory / sourcePath.filename();

                    if (sourcePath != destPath && sourcePath.parent_path() != m_currentDirectory) {
                        MoveAssetWithMeta(sourcePath, destPath);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

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
            if (ImGui::BeginMenu("Create")) {

                if (ImGui::MenuItem("Folder")) {
                    CreateNewFolder();
                }
                if (ImGui::MenuItem("Material")) {
                    CreateNewMaterial();
                }
                if (ImGui::MenuItem("Script", "", false, false)) {
                    //CreateNewFolder();
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Rendering")) {

                    if (ImGui::MenuItem("Material")) {
                        CreateNewMaterial();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Scene")) {

                    if (ImGui::MenuItem("Scene")) {
                        CreateNewScene();
                    }
                    if (ImGui::MenuItem("Prefab")) {
                        //CreateNewFolder();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Shader")) {

                    if (ImGui::MenuItem("Shader")) {
                        //CreateNewFolder();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Show in Explorer")) {
                OpenDirectoryInFileExplorer(m_currentDirectory.relative_path().string());
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

                if (ImGui::MenuItem("Reimport")) {
                    Assets::AssetManager::GetInstance().ReimportAsset(m_selectedPath.string());
                }
            }
            else {
                ImGui::BeginDisabled();
                ImGui::MenuItem("Rename");
                ImGui::MenuItem("Delete");
                ImGui::EndDisabled();
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

    void AssetBrowserPanel::CreateNewMaterial() {
        namespace fs = std::filesystem;

        fs::path targetDir = m_currentDirectory;
        if (!fs::exists(targetDir))
            return;

        static int s_MatCounter = 1;
        fs::path matPath;
        do {
            matPath = targetDir / ("NewMaterial_" + std::to_string(s_MatCounter++) + ".nanomat");
        } while (fs::exists(matPath));

        rapidjson::Document doc;
        doc.SetObject();
        auto& alloc = doc.GetAllocator();

        doc.AddMember("Shader", rapidjson::Value("Unlit", alloc), alloc);

        doc.AddMember("DepthTest", true, alloc);
        doc.AddMember("BlendMode", true, alloc);

        doc.AddMember("CullMode", 1029, alloc);
        doc.AddMember("PolygonMode", 6914, alloc);

        rapidjson::Value props(rapidjson::kObjectType);
        doc.AddMember("Properties", props, alloc);

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::ofstream out(matPath);
        if (out.is_open()) {
            out << buffer.GetString();
            out.close();
        }

        Assets::AssetManager::GetInstance().GenerateMetadata(matPath.string());
    }

    void AssetBrowserPanel::CreateNewScene() {
        namespace fs = std::filesystem;

        fs::path targetDir = m_currentDirectory;
        if (!fs::exists(targetDir))
            return;

        static int s_sceneCounter = 1;
        fs::path scenePath;
        do {
            scenePath = targetDir / ("NewScene_" + std::to_string(s_sceneCounter++) + ".scene");
        } while (fs::exists(scenePath));

        std::ofstream out(scenePath);
        out.close();

        Assets::AssetManager::GetInstance().GenerateMetadata(scenePath.string());
    }

    void AssetBrowserPanel::DeleteAssetWithMeta(const std::filesystem::path& assetPath) {
        std::error_code ec;

        // Delete the main asset
        if (std::filesystem::is_directory(assetPath)) {
            std::filesystem::remove_all(assetPath, ec);
        }
        else {
            std::filesystem::remove(assetPath, ec);
        }

        if (ec) {
            // Log error if deletion failed
            return;
        }

        // Delete the .meta file if it exists
        std::filesystem::path metaPath = assetPath;
        metaPath += ".meta";

        if (std::filesystem::exists(metaPath)) {
            std::filesystem::remove(metaPath, ec);
        }
    }

    void AssetBrowserPanel::MoveAssetWithMeta(const std::filesystem::path& source, const std::filesystem::path& destination) {
        std::error_code ec;

        // Move the main asset
        std::filesystem::rename(source, destination, ec);
        if (ec) {
            // Log error if move failed
            return;
        }

        // Move the .meta file if it exists
        std::filesystem::path sourceMetaPath = source;
        sourceMetaPath += ".meta";

        if (std::filesystem::exists(sourceMetaPath)) {
            std::filesystem::path destMetaPath = destination;
            destMetaPath += ".meta";

            std::filesystem::rename(sourceMetaPath, destMetaPath, ec);
            // Optional: handle ec here if you care about meta file move failures
        }
    }

}