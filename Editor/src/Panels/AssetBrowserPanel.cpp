#include "AssetBrowserPanel.hpp"
#include <fstream>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include <Engine.hpp>
#include <Core/SpdLogger.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <ResourceManagement/ResourcePaths.hpp>
#include <Events/EventBus.hpp>

#include "../EditorScene.hpp"
#include "../AssetManagement/AssetManager.hpp"
#include "../EditorEvents.hpp"
#include "../ThumbnailManager.hpp"

namespace Editor {
    AssetBrowserPanel::AssetBrowserPanel(const std::filesystem::path& root)
        : m_rootDirectory(root), m_currentDirectory(root)
    {

        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            std::filesystem::path filePath = entry.path();

            if (filePath.extension() == ".meta") continue;

            Assets::AssetManager::GetInstance().GenerateMetadata(entry.path().string());
            Assets::ThumbnailManager::GetInstance().GenerateThumbnail(
                entry.path(),
                Assets::AssetManager::GetInstance().RetrieveUUID(entry.path().string())
			);
        }

        NANOEngine::Events::EventBus::Get().Subscribe<Events::GotoAssetPathEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::GotoAssetPathEvent& e) {
                GotoAssetFolder(e.assetPath);
            }
        );
    }

    AssetBrowserPanel::~AssetBrowserPanel() {
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
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ENTITY_DRAG")) {
                if (p->DataSize >= sizeof(uint32_t)) {
                    const uint32_t* entities = static_cast<const uint32_t*>(p->Data);
                    uint32_t dropped = entities[0]; // First entity
                    
					EditorScene::s_selection.SetDropped(dropped);

                    std::string uuid = Assets::GenerateUUID();

                    auto& meta = NE::ECS::Command::GetEntityMeta(dropped);
                    std::string prefabName = meta.name;
                    if (meta.name.empty())
                        prefabName = "Prefab";

                    std::string filePath = m_currentDirectory.string() + "/" + prefabName + ".nfab";
                    Assets::AssetManager::GetInstance().GenerateMetadata(filePath, uuid);
                    EditorScene::s_selection.Clear();
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
                EditorScene::isDirty = false;

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
        float thumbnailSize = 64.0f;
        float cellPaddingX = 20.0f;
        float cellPaddingY = 18.0f;

        float cellWidth = thumbnailSize + cellPaddingX;
        float textLineH = ImGui::GetTextLineHeight();
        //float cellHeight = thumbnailSize + 4.0f + textLineH + cellPaddingY;

        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = (int)(panelWidth / cellWidth);
        if (columnCount < 1) columnCount = 1;

        ImGui::Columns(columnCount, nullptr, false);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));

        for (auto& entry : std::filesystem::directory_iterator(path)) {
            const auto& name = entry.path().filename().string();

            if (entry.path().extension() == ".meta") continue;

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

			unsigned int iconTexture = Assets::ThumbnailManager::GetInstance().GetThumbnail(entry.path());

            ImGui::ImageButton("##btn",
                (ImTextureID)(intptr_t)iconTexture,
                ImVec2(thumbnailSize, thumbnailSize),
                ImVec2(0, 1), ImVec2(1, 0)
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
                    } else if (entryPath.extension() == ".nfab") {
                        EditorScene::s_selection.Clear();
                        EditorScene::selectedAsset = "";
						std::string prefabUUID = Assets::AssetManager::GetInstance().RetrieveUUID(entryPath.string());
                        if (NE::LoadPrefabScene(NE::Resource::ComputeArtifactPathFromUUID(prefabUUID, NE::Resource::ResourceType::Prefab))) {
                            EditorScene::BuildRoot();
                            Editor::EditorScene::selectedPrefab = prefabUUID;
                        }
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
                ImDrawList* dl = ImGui::GetWindowDrawList();

                float lineH = ImGui::GetTextLineHeight();
                ImVec2 bbMin = ImGui::GetCursorScreenPos();
                ImVec2 bbMax = bbMin + ImVec2(cellWidth, lineH);

                ImVec2 full = ImGui::CalcTextSize(name.c_str());
                float startX = bbMin.x + (cellWidth - full.x) * 0.5f;
                if (startX < bbMin.x) startX = bbMin.x;

                ImGui::RenderTextEllipsis(
                    dl,
                    ImVec2(startX, bbMin.y),
                    bbMax,
                    bbMax.x,
                    name.c_str(),
                    nullptr,
                    &full
                );

                ImGui::Dummy(ImVec2(0, lineH));

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
        ImGui::PopStyleColor(3);

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

		doc.AddMember("RenderQueueBase", rapidjson::Value("Geometry", alloc), alloc);
		doc.AddMember("RenderQueueOffset", 0, alloc);

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

    void AssetBrowserPanel::GotoAssetFolder(const std::string& assetPath) {
        std::filesystem::path fullPath = std::filesystem::absolute(assetPath);
		m_currentDirectory = fullPath.parent_path();

		ImGui::SetWindowFocus("Asset Browser");
    }

}