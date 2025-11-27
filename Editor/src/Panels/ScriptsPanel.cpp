#include "ScriptsPanel.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <Scripting/ScriptingEngine.hpp>
#include <Core/SpdLogger.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>

namespace Editor {

	ScriptsPanel::ScriptsPanel(const std::filesystem::path& scriptsDirectory)
		: m_scriptsDirectory(scriptsDirectory)
	{
		// Ensure the scripts directory exists
		if (!std::filesystem::exists(m_scriptsDirectory)) {
			SPD_ERROR("Scripts directory does not exist: " << m_scriptsDirectory.string());
			std::filesystem::create_directories(m_scriptsDirectory);
		}

		RefreshScriptsList();
	}

	ScriptsPanel::~ScriptsPanel() {
	}

	void ScriptsPanel::OnImGuiRender() {
		ImGui::Begin("Scripts", nullptr, ImGuiWindowFlags_MenuBar);

		// Menu bar with hot reload status and actions
		ImGui::BeginMenuBar();
		{
			// Search bar
			ImGui::SetNextItemWidth(200.0f);
			ImGui::InputTextWithHint("##ScriptSearch", "Search scripts...", m_searchBuffer, sizeof(m_searchBuffer));

			ImGui::SameLine();

			// Refresh button
			if (ImGui::Button("Refresh")) {
				RefreshScriptsList();
			}

			ImGui::SameLine();

			// Create new script button
			if (ImGui::Button("+ New Script")) {
				m_showCreateScriptPopup = true;
				memset(m_newScriptNameBuffer, 0, sizeof(m_newScriptNameBuffer));
			}

			ImGui::SameLine();

			// Open folder button
			if (ImGui::Button("Open Folder")) {
				OpenDirectoryInFileExplorer(m_scriptsDirectory.string());
			}

			// Spacer
			ImGui::SameLine();
			ImGui::Dummy(ImVec2(20, 0));

			// Hot reload status indicator
			RenderHotReloadStatus();
		}
		ImGui::EndMenuBar();

		// Main content area - split vertically
		ImGui::BeginChild("MainContent", ImVec2(0, -200), false);
		RenderScriptsList();
		ImGui::EndChild();

		ImGui::Separator();

		// Bottom panel - compile output
		ImGui::BeginChild("CompileOutput", ImVec2(0, 0), true);
		RenderCompileOutput();
		ImGui::EndChild();

		// Render popups
		RenderPopups();

		ImGui::End();
	}

	void ScriptsPanel::RenderScriptsList() {
		// Display script count
		ImGui::Text("Scripts: %zu", m_scriptFiles.size());
		ImGui::Separator();

		// Filter scripts based on search
		std::string searchLower = m_searchBuffer;
		std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
			[](unsigned char c) { return static_cast<char>(::tolower(c)); });

		// Display scripts in a list
		for (const auto& scriptPath : m_scriptFiles) {
			std::string filename = scriptPath.filename().string();
			std::string filenameLower = filename;
			std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(),
				[](unsigned char c) { return static_cast<char>(::tolower(c)); });

			// Skip if doesn't match search
			if (!searchLower.empty() && filenameLower.find(searchLower) == std::string::npos) {
				continue;
			}

			// Get script class name
			std::string className = GetScriptClassName(scriptPath);

			bool isSelected = (scriptPath == m_selectedScript);
			ImGui::PushID(scriptPath.string().c_str());

			// Selectable item
			if (ImGui::Selectable(filename.c_str(), isSelected)) {
				m_selectedScript = scriptPath;
			}

			// Double-click to open
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
				OpenScriptInEditor(scriptPath);
			}

			// Context menu
			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Open")) {
					OpenScriptInEditor(scriptPath);
				}

				if (ImGui::MenuItem("Rename")) {
					m_renamingScript = scriptPath;
					m_showRenamePopup = true;
					strncpy_s(m_renameBuffer, scriptPath.stem().string().c_str(), sizeof(m_renameBuffer) - 1);
				}

				if (ImGui::MenuItem("Delete")) {
					m_selectedScript = scriptPath;
					m_showDeleteConfirmPopup = true;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Show in Explorer")) {
					OpenDirectoryInFileExplorer(scriptPath.parent_path().string());
				}

				if (ImGui::MenuItem("Copy Path")) {
					ImGui::SetClipboardText(scriptPath.string().c_str());
				}

				ImGui::EndPopup();
			}

			ImGui::PopID();
		}
	}

	void ScriptsPanel::RenderHotReloadStatus() {
		auto& scriptEngine = NE::Scripting::ScriptingEngine::GetInstance();

		// Check if compile is queued
		bool compileQueued = scriptEngine.m_compileQueued.load();

		if (compileQueued || m_compileInfo.isCompiling) {
			// Show compiling status
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Yellow
			ImGui::Text("Compiling...");
			ImGui::PopStyleColor();

			// Spinner animation
			ImGui::SameLine();
			const char* spinner[] = { "|", "/", "-", "\\" };
			static int spinnerIndex = 0;
			spinnerIndex = (spinnerIndex + 1) % 4;
			ImGui::Text("%s", spinner[spinnerIndex]);

			m_compileInfo.isCompiling = true;
		}
		else {
			if (m_compileInfo.isCompiling) {
				// Just finished compiling
				m_compileInfo.isCompiling = false;
				m_compileInfo.lastCompileTime = std::chrono::steady_clock::now();
				m_compileInfo.compileCount++;
			}

			// Show last compile status
			if (m_compileInfo.lastCompileSuccess) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green
				ImGui::Text("Ready");
				ImGui::PopStyleColor();
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red
				ImGui::Text("Build Failed");
				ImGui::PopStyleColor();
			}

			// Show compile count
			if (m_compileInfo.compileCount > 0) {
				ImGui::SameLine();
				ImGui::TextDisabled("(Reloaded: %d)", m_compileInfo.compileCount);
			}
		}
	}

	void ScriptsPanel::RenderCompileOutput() {
		ImGui::Text("Compile Output");
		ImGui::Separator();

		// Get last error from scripting engine
		auto& scriptEngine = NE::Scripting::ScriptingEngine::GetInstance();
		const std::string& lastError = scriptEngine.GetLastError();

		if (!lastError.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // Light red
			ImGui::TextWrapped("%s", lastError.c_str());
			ImGui::PopStyleColor();
		}
		else {
			ImGui::TextDisabled("No errors. Scripts compiled successfully.");
		}

		// Display compile output log
		if (!m_compileOutput.empty()) {
			ImGui::Separator();
			ImGui::Text("Build Log:");

			ImGui::BeginChild("BuildLog", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
			for (const auto& line : m_compileOutput) {
				// Color code error/warning lines
				if (line.find("error") != std::string::npos || line.find("Error") != std::string::npos) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
					ImGui::TextUnformatted(line.c_str());
					ImGui::PopStyleColor();
				}
				else if (line.find("warning") != std::string::npos || line.find("Warning") != std::string::npos) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
					ImGui::TextUnformatted(line.c_str());
					ImGui::PopStyleColor();
				}
				else {
					ImGui::TextUnformatted(line.c_str());
				}
			}
			ImGui::EndChild();
		}
	}

	void ScriptsPanel::RenderPopups() {
		// Create new script popup
		if (m_showCreateScriptPopup) {
			ImGui::OpenPopup("Create New Script");
			m_showCreateScriptPopup = false;
		}

		if (ImGui::BeginPopupModal("Create New Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Enter script class name:");
			ImGui::SetNextItemWidth(300.0f);
			ImGui::InputTextWithHint("##ScriptName", "e.g., MyNewScript", m_newScriptNameBuffer, sizeof(m_newScriptNameBuffer));

			ImGui::Separator();

			// Validate name
			bool validName = strlen(m_newScriptNameBuffer) > 0;
			std::string nameStr = m_newScriptNameBuffer;

			// Check if name is valid C++ identifier
			if (validName && !nameStr.empty()) {
				if (!std::isalpha(nameStr[0]) && nameStr[0] != '_') {
					validName = false;
				}
				for (char c : nameStr) {
					if (!std::isalnum(c) && c != '_') {
						validName = false;
						break;
					}
				}
			}

			if (!validName && strlen(m_newScriptNameBuffer) > 0) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
				ImGui::Text("Invalid class name. Use only letters, numbers, and underscores.");
				ImGui::PopStyleColor();
			}

			ImGui::BeginDisabled(!validName);
			if (ImGui::Button("Create", ImVec2(120, 0))) {
				CreateNewScript(m_newScriptNameBuffer);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// Delete confirmation popup
		if (m_showDeleteConfirmPopup) {
			ImGui::OpenPopup("Confirm Delete");
			m_showDeleteConfirmPopup = false;
		}

		if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure you want to delete this script?");
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s", m_selectedScript.filename().string().c_str());
			ImGui::Separator();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("Delete", ImVec2(120, 0))) {
				DeleteScript(m_selectedScript);
				m_selectedScript.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// Rename popup
		if (m_showRenamePopup) {
			ImGui::OpenPopup("Rename Script");
			m_showRenamePopup = false;
		}

		if (ImGui::BeginPopupModal("Rename Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Enter new name:");
			ImGui::SetNextItemWidth(300.0f);
			ImGui::InputText("##RenameName", m_renameBuffer, sizeof(m_renameBuffer));

			ImGui::Separator();

			bool validName = strlen(m_renameBuffer) > 0;

			ImGui::BeginDisabled(!validName);
			if (ImGui::Button("Rename", ImVec2(120, 0))) {
				RenameScript(m_renamingScript, m_renameBuffer);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ScriptsPanel::CreateNewScript(const std::string& scriptName) {
		// Generate file path
		std::filesystem::path scriptPath = m_scriptsDirectory / (scriptName + ".hpp");

		// Check if file already exists
		if (std::filesystem::exists(scriptPath)) {
			SPD_ERROR("Script already exists: " << scriptPath.string());
			return;
		}

		// Generate script content from template
		std::string scriptContent = GenerateScriptTemplate(scriptName);

		// Write to file
		std::ofstream file(scriptPath);
		if (!file.is_open()) {
			SPD_ERROR("Failed to create script file: " << scriptPath.string());
			return;
		}

		file << scriptContent;
		file.close();

		SPD_INFO("Created new script: " << scriptPath.string());

		// Refresh list
		RefreshScriptsList();

		// Select the new script
		m_selectedScript = scriptPath;

		// Small delay to let file system settle before opening editor
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// Open in editor
		OpenScriptInEditor(scriptPath);
	}

	void ScriptsPanel::DeleteScript(const std::filesystem::path& scriptPath) {
		if (!std::filesystem::exists(scriptPath)) {
			SPD_ERROR("Script does not exist: " << scriptPath.string());
			return;
		}

		try {
			std::filesystem::remove(scriptPath);
			SPD_INFO("Deleted script: " << scriptPath.string());
			RefreshScriptsList();
		}
		catch (const std::exception& e) {
			SPD_ERROR("Failed to delete script: " << e.what());
		}
	}

	void ScriptsPanel::RenameScript(const std::filesystem::path& oldPath, const std::string& newName) {
		if (!std::filesystem::exists(oldPath)) {
			SPD_ERROR("Script does not exist: " << oldPath.string());
			return;
		}

		// Generate new path with extension
		std::filesystem::path newPath = oldPath.parent_path() / (newName + oldPath.extension().string());

		// Check if new path already exists
		if (std::filesystem::exists(newPath)) {
			SPD_ERROR("A script with that name already exists: " << newPath.string());
			return;
		}

		try {
			std::filesystem::rename(oldPath, newPath);
			SPD_INFO("Renamed script: " << oldPath.filename().string() << " -> " << newPath.filename().string());
			RefreshScriptsList();
			m_selectedScript = newPath;
		}
		catch (const std::exception& e) {
			SPD_ERROR("Failed to rename script: " << e.what());
		}
	}

	void ScriptsPanel::OpenScriptInEditor(const std::filesystem::path& scriptPath) {
		if (!std::filesystem::exists(scriptPath)) {
			SPD_ERROR("Script does not exist: " << scriptPath.string());
			return;
		}

		// Use the same method as AssetBrowserPanel - just use system command
		std::filesystem::path fullPath = std::filesystem::absolute(scriptPath);
		std::string command = "\"" + fullPath.string() + "\"";
		std::system(command.c_str());
	}

	void ScriptsPanel::RefreshScriptsList() {
		m_scriptFiles.clear();

		if (!std::filesystem::exists(m_scriptsDirectory)) {
			return;
		}

		// Scan for .hpp and .h files
		for (const auto& entry : std::filesystem::directory_iterator(m_scriptsDirectory)) {
			if (entry.is_regular_file() && IsScriptFile(entry.path())) {
				m_scriptFiles.push_back(entry.path());
			}
		}

		// Sort alphabetically
		std::sort(m_scriptFiles.begin(), m_scriptFiles.end(),
			[](const std::filesystem::path& a, const std::filesystem::path& b) {
				return a.filename().string() < b.filename().string();
			}
		);
	}

	std::string ScriptsPanel::GenerateScriptTemplate(const std::string& className) {
		std::stringstream ss;

		ss << "#pragma once\n";
		ss << "#include \"ScriptBase.hpp\"\n";
		ss << "\n";
		ss << "/**\n";
		ss << " * " << className << " - Auto-generated script template\n";
		ss << " * Implement your game logic in the lifecycle methods below.\n";
		ss << " */\n";
		ss << "class " << className << " : public IScript" << " {\n";
		ss << "public:\n";
		ss << "\t" << className << "() {\n";
		ss << "\t\t// Register any editable fields here\n";
		ss << "\t\t// Example: SCRIPT_FIELD(speed, float);\n";
		ss << "\t\t// Example: SCRIPT_FIELD_VECTOR(blingstring, String);;\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\t~" << className << "() override = default;\n";
		ss << "\n";
		ss << "\t// === Lifecycle Methods ===\n";
		ss << "\n";
		ss << "\tvoid Awake() override {\n";
		ss << "\t\t// Called when the script component is first created\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tvoid Initialize(Entity entity) override {\n";
		ss << "\t\t// Called to initialize the script with its entity\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tvoid Start() override {\n";
		ss << "\t\t// Called when the script is enabled and play mode starts\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tvoid Update(double deltaTime) override {\n";
		ss << "\t\t// Called every frame while the script is enabled\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tvoid OnDestroy() override {\n";
		ss << "\t\t// Called when the script is about to be destroyed\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\t// === Optional Callbacks ===\n";
		ss << "\n";
		ss << "\tvoid OnEnable() override {\n";
		ss << "\t\t// Called when the script is enabled\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tvoid OnDisable() override {\n";
		ss << "\t\t// Called when the script is disabled\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tvoid OnValidate() override {\n";
		ss << "\t\t// Called when a field value is changed in the editor\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tconst char* GetTypeName() const override {\n";
		ss << "\t\treturn \"" << className << "\";\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\t// === Collision Callbacks ===\n";
		ss << "\n";
		ss << "\tvoid OnCollisionEnter(Entity other) override {\n";
		ss << "\t\t// Called when this entity starts colliding with another\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tvoid OnCollisionExit(Entity other) override {\n";
		ss << "\t\t// Called when this entity stops colliding with another\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tvoid OnTriggerEnter(Entity other) override {\n";
		ss << "\t\t// Called when this entity enters a trigger\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "\tvoid OnTriggerExit(Entity other) override {\n";
		ss << "\t\t// Called when this entity exits a trigger\n";
		ss << "\t}\n";
		ss << "\n";
		ss << "private:\n";
		ss << "\t// Add your private member variables here\n";
		ss << "\t// Example: float speed = 5.0f;\n";
		ss << "\n";
		ss << "};\n";

		return ss.str();
	}

	bool ScriptsPanel::IsScriptFile(const std::filesystem::path& path) const {
		std::string ext = path.extension().string();
		return (ext == ".hpp" || ext == ".h");
	}

	std::string ScriptsPanel::GetScriptClassName(const std::filesystem::path& path) const {
		// Simple heuristic: class name is likely the filename without extension
		return path.stem().string();
	}

	void ScriptsPanel::OpenDirectoryInFileExplorer(const std::string& directoryPath) {
		// Use the same method as AssetBrowserPanel
		std::filesystem::path fullPath = std::filesystem::absolute(directoryPath);
		std::string command = "explorer \"" + fullPath.string() + "\"";
		std::system(command.c_str());
	}

} // namespace Editor
