#include "pch.h"
/*!
\fileLoggerPanel.cpp
\author     Anson Teng
\date       9/9/2025
\brief      This file contains implementation for ImGui Logger Panel.
		Provides visual interface for viewing, filtering, and managing log messages
	  captured by SpdLogger with real-time display and search functionality.
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/

#include "LoggerPanel.hpp"
#include <imgui/imgui.h>
#include <algorithm>
#include <cctype>
#include <functional>

namespace Editor {
	/*!
		\brief Constructor - initializes the logger panel with default settings
		*/

	LoggerPanel::LoggerPanel() {
	}

	/*!
	\brief Renders the ImGui interface for the logger panel
  Called every frame to display the logging interface with filters, search, and log entries
	*/
	void LoggerPanel::OnImGuiRender() {
		ImGui::Begin("Logger", nullptr, ImGuiWindowFlags_MenuBar);

		// Render menu bar with filtering and control options
		if (ImGui::BeginMenuBar()) {
			RenderFilterButtons();
			ImGui::Separator();
			RenderControlButtons();
			ImGui::EndMenuBar();
		}

		// Search functionality
		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputTextWithHint("##Search", "Search logs...", m_searchBuffer, sizeof(m_searchBuffer));
		ImGui::SameLine();

		// Auto-scroll toggle
		ImGui::Checkbox("Auto-scroll", &m_autoScroll);

		// Show selection help text
		if (!m_selectedIndices.empty()) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
				"(%zu selected)", m_selectedIndices.size());
		} else {
			ImGui::SameLine();
			ImGui::TextDisabled("(Click to select, Ctrl+Click multi, Shift+Click range, drag to select)");
		}

		ImGui::Separator();

		// log display area
		if (ImGui::BeginChild("LogScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
			auto logEntries = SpdLogger::GetInstance().GetLogEntries();

			// filter and search entries
			std::vector<const SpdLogEntry*> filteredEntries;
			for (const auto& entry : logEntries) {
				if (ShouldShowEntry(entry)) {
					// Apply search filter if search text is provided
					if (strlen(m_searchBuffer) > 0) {
						std::string lowerMessage = entry.message;
						std::string lowerSearch = m_searchBuffer;

						// Convert to lowercase for case-insensitive search
						std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(),
							[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
						std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(),
							[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

						// Skip entry if search text not found
						if (lowerMessage.find(lowerSearch) == std::string::npos) {
							continue;
						}
					}
					filteredEntries.push_back(&entry);
				}
			}

			// Keyboard shortcuts for selection and copying
			if (ImGui::IsWindowFocused()) {
				// Ctrl+A: Select all visible logs
				if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
					m_selectedIndices.clear();
					for (size_t i = 0; i < filteredEntries.size(); ++i) {
						m_selectedIndices.push_back(static_cast<int>(i));
					}
				}

				// Ctrl+C: Copy selected logs (or all if none selected)
				if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
					if (!m_selectedIndices.empty()) {
						CopySelectedLogs(filteredEntries);
					} else {
						CopyAllVisibleLogs(filteredEntries);
					}
				}

				// Escape: Clear selection
				if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
					m_selectedIndices.clear();
				}
			}

			// Render all filtered entries
			for (size_t i = 0; i < filteredEntries.size(); ++i) {
				RenderLogEntry(*filteredEntries[i], static_cast<int>(i));
			}

			// Auto-scroll to bottom when new logs arrive
			if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}
		}
		ImGui::EndChild();

		ImGui::End();
	}

	/*!
	\brief Determines if a log entry should be displayed based on current filter settings
\param entry The log entry to check against current filters
	\return True if the entry should be shown based on its log level, false otherwise
	*/
	bool LoggerPanel::ShouldShowEntry(const SpdLogEntry& entry) const {
		switch (entry.level) {
		case SpdLogLevel::Debug:    return m_showDebug;
		case SpdLogLevel::Info:     return m_showInfo;
		case SpdLogLevel::Warning:  return m_showWarning;
		case SpdLogLevel::Error:    return m_showError;
		case SpdLogLevel::Critical: return m_showCritical;
		default:           return true;
		}
	}

	/*!
 \brief Converts log level enum to human-readable display string
	\param level The log level to convert
	\return String representation of the log level for display purposes
	*/
	const char* LoggerPanel::GetLevelString(SpdLogLevel level) const {
		switch (level) {
		case SpdLogLevel::Debug:    return "DEBUG";
		case SpdLogLevel::Info:  return "INFO";
		case SpdLogLevel::Warning:  return "WARNING";
		case SpdLogLevel::Error:    return "ERROR";
		case SpdLogLevel::Critical: return "CRITICAL";
		default:        return "UNKNOWN";
		}
	}

	/*!
	  \brief Renders a single log entry with color coding, timestamp, selection, and context menu
	  \param entry The log entry to render with all its information
	  \param index The index of this entry used for generating unique ImGui IDs
	  */
	void LoggerPanel::RenderLogEntry(const SpdLogEntry& entry, int index) {
		// Check if this entry is selected
		bool isSelected = IsIndexSelected(index);

		// Color based on log level
		ImVec4 color;
		switch (entry.level) {
		case SpdLogLevel::Debug:    color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f); break; // Green
		case SpdLogLevel::Info:     color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break; // White
		case SpdLogLevel::Warning:  color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break; // Yellow
		case SpdLogLevel::Error:    color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break; // Red
		case SpdLogLevel::Critical: color = ImVec4(1.0f, 0.0f, 1.0f, 1.0f); break; // Magenta
		default:          color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;
		}

		// Apply selection background with transparency to keep text visible
		if (isSelected) {
			// Draw a subtle background rectangle
			ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			ImVec2 textSize = ImGui::CalcTextSize("X"); // Get line height
			ImVec2 rectMin = cursorPos;
			ImVec2 rectMax = ImVec2(cursorPos.x + ImGui::GetContentRegionAvail().x, cursorPos.y + textSize.y);

			// Draw semi-transparent selection background
			ImGui::GetWindowDrawList()->AddRectFilled(
				rectMin,
				rectMax,
				ImGui::GetColorU32(ImVec4(0.2f, 0.4f, 0.7f, 0.3f)) // Lighter blue with more transparency
			);
		}

		ImGui::PushStyleColor(ImGuiCol_Text, color);

		// Format timestamp
		auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
		std::tm tm;
		localtime_s(&tm, &time);

		char timeBuffer[32];
		std::strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M:%S]", &tm);

		// Format file and line information
		std::string fileInfo;
		if (!entry.file.empty() && entry.line != -1) {
			size_t lastSlash = entry.file.find_last_of("/\\");
			std::string filename = (lastSlash == std::string::npos)
				? entry.file
				: entry.file.substr(lastSlash + 1);
			fileInfo = "[" + filename + ":" + std::to_string(entry.line) + "]";
		}

		// Display the complete log entry
		ImGui::Text("%s [%s] %s %s",
			timeBuffer,
			GetLevelString(entry.level),
			fileInfo.c_str(),
			entry.message.c_str());

		// Check if item is hovered (for drag selection)
		bool isItemHovered = ImGui::IsItemHovered();

		// Handle click selection (left mouse button)
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			bool isMultiSelect = ImGui::GetIO().KeyCtrl;
			bool isRangeSelect = ImGui::GetIO().KeyShift;

			if (isRangeSelect && m_lastClickedIndex != -1) {
				// Shift+Click: Select range from last clicked to current
				SelectRange(m_lastClickedIndex, index);
				m_lastClickedIndex = index;
			} else if (!isMultiSelect) {
				// Start potential drag selection
				m_isDragging = false; // Will be set to true if mouse is dragged
				m_dragStartIndex = index;
				m_dragEndIndex = index;

				ToggleSelection(index, isMultiSelect);
			} else {
				// Ctrl+Click: Toggle individual selection
				ToggleSelection(index, isMultiSelect);
			}
		}

		// Handle drag selection
		HandleDragSelection(index, isItemHovered);

		ImGui::PopStyleColor();

		// Right-click context menu for copying functionality
		if (ImGui::BeginPopupContextItem(("LogEntry" + std::to_string(index)).c_str())) {
			if (ImGui::MenuItem("Copy Message")) {
				ImGui::SetClipboardText(entry.message.c_str());
			}
			if (ImGui::MenuItem("Copy Full Entry")) {
				std::string fullEntry = FormatLogEntry(entry);
				ImGui::SetClipboardText(fullEntry.c_str());
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Select This Entry")) {
				m_selectedIndices.clear();
				m_selectedIndices.push_back(index);
			}
			ImGui::EndPopup();
		}
	}

	/*!
	\brief Renders filter checkboxes for each log level in the menu bar
		   Allows users to toggle visibility of different log level categories
	*/
	void LoggerPanel::RenderFilterButtons() {
		// Log level filter buttons
		if (ImGui::Checkbox("Debug", &m_showDebug)) {}
		ImGui::SameLine();
		if (ImGui::Checkbox("Info", &m_showInfo)) {}
		ImGui::SameLine();
		if (ImGui::Checkbox("Warning", &m_showWarning)) {}
		ImGui::SameLine();
		if (ImGui::Checkbox("Error", &m_showError)) {}
		ImGui::SameLine();
		if (ImGui::Checkbox("Critical", &m_showCritical)) {}
	}

	/*!
	\brief Renders control buttons for log management and quick filter presets
		   Provides Clear Logs, All, None, and Errors Only functionality plus copy options
	*/
	void LoggerPanel::RenderControlButtons() {
		// Control buttons
		if (ImGui::Button("Clear Logs")) {
			SpdLogger::GetInstance().ClearLogEntries();
			m_selectedIndices.clear();
		}

		ImGui::SameLine();
		// Show all log levels
		if (ImGui::Button("All")) {
			m_showDebug = m_showInfo = m_showWarning = m_showError = m_showCritical = true;
		}

		ImGui::SameLine();
		// Hide all log levels
		if (ImGui::Button("None")) {
			m_showDebug = m_showInfo = m_showWarning = m_showError = m_showCritical = false;
		}

		ImGui::SameLine();
		// Show only error and critical messages
		if (ImGui::Button("Errors Only")) {
			m_showDebug = m_showInfo = m_showWarning = false;
			m_showError = m_showCritical = true;
		}

		ImGui::SameLine();
		ImGui::Separator();

		ImGui::SameLine();
		// Copy all visible logs
		if (ImGui::Button("Copy All Visible")) {
			auto logEntries = SpdLogger::GetInstance().GetLogEntries();
			std::vector<const SpdLogEntry*> filteredEntries;
			for (const auto& entry : logEntries) {
				if (ShouldShowEntry(entry)) {
					filteredEntries.push_back(&entry);
				}
			}
			CopyAllVisibleLogs(filteredEntries);
		}

		ImGui::SameLine();
		// Copy selected logs
		if (ImGui::Button("Copy Selected") && !m_selectedIndices.empty()) {
			auto logEntries = SpdLogger::GetInstance().GetLogEntries();
			std::vector<const SpdLogEntry*> filteredEntries;
			for (const auto& entry : logEntries) {
				if (ShouldShowEntry(entry)) {
					filteredEntries.push_back(&entry);
				}
			}
			CopySelectedLogs(filteredEntries);
		}

		// Show selection count
		if (!m_selectedIndices.empty()) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
				"(%zu selected)", m_selectedIndices.size());
		}

		// Crash simulation buttons (for testing only - remove in production!)
		//ImGui::SameLine();
		//if (ImGui::Button("[Crash: Access Violation")) {
		//	SPD_CRASH_LOG("Simulating access violation crash");
		//	// Force access violation
		//	int* null_ptr = nullptr;
		//	*null_ptr = 42;  // This will crash with access violation
		//}

		//ImGui::SameLine();
		//if (ImGui::Button("Crash: Stack Overflow")) {
		//	SPD_CRASH_LOG("Simulating stack overflow crash");
		//	// Force stack overflow with infinite recursion
		//	std::function<void()> infiniteRecursion = [&]() {
		//		infiniteRecursion();
		//		};
		//	infiniteRecursion();
		//}

		//ImGui::SameLine();
		//if (ImGui::Button("Crash: Abort")) {
		//	SPD_FATAL_CRASH("Simulating controlled crash via abort()");
		//	// This will call SaveCrashLog() then abort()
		//}
	}

	/*!
	\brief Formats a single log entry to a readable string
	\param entry The log entry to format
	\return Formatted string representation with timestamp, level, file, and message
	*/
	std::string LoggerPanel::FormatLogEntry(const SpdLogEntry& entry) const {
		auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
		std::tm tm;
		localtime_s(&tm, &time);

		char timeBuffer[32];
		std::strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M:%S]", &tm);

		std::string fileInfo;
		if (!entry.file.empty() && entry.line != -1) {
			size_t lastSlash = entry.file.find_last_of("/\\");
			std::string filename = (lastSlash == std::string::npos)
				? entry.file
				: entry.file.substr(lastSlash + 1);
			fileInfo = "[" + filename + ":" + std::to_string(entry.line) + "] ";
		}

		std::string fullEntry = timeBuffer;
		fullEntry += " [" + std::string(GetLevelString(entry.level)) + "] ";
		fullEntry += fileInfo + entry.message;

		return fullEntry;
	}

	/*!
		\brief Copies all visible/filtered logs to the system clipboard
		\param filteredEntries Vector of log entries currently visible after filtering
		*/
	void LoggerPanel::CopyAllVisibleLogs(const std::vector<const SpdLogEntry*>& filteredEntries) {
		if (filteredEntries.empty()) {
			return;
		}

		std::string allLogs;
		for (const auto* entry : filteredEntries) {
			allLogs += FormatLogEntry(*entry) + "\n";
		}

		ImGui::SetClipboardText(allLogs.c_str());
		SPD_INFO("Copied " << filteredEntries.size() << " log entries to clipboard");
	}

	/*!
	\brief Copies selected logs to the system clipboard
 \param filteredEntries Vector of log entries currently visible after filtering
	*/
	void LoggerPanel::CopySelectedLogs(const std::vector<const SpdLogEntry*>& filteredEntries) {
		if (m_selectedIndices.empty()) {
			return;
		}

		std::string selectedLogs;
		for (int index : m_selectedIndices) {
			if (index >= 0 && index < static_cast<int>(filteredEntries.size())) {
				selectedLogs += FormatLogEntry(*filteredEntries[index]) + "\n";
			}
		}

		ImGui::SetClipboardText(selectedLogs.c_str());
		SPD_INFO("Copied " << m_selectedIndices.size() << " selected log entries to clipboard");
	}

	/*!
	\brief Checks if a log entry index is currently selected
	\param index The index to check
	\return True if the index is in the selection list
	*/
	bool LoggerPanel::IsIndexSelected(int index) const {
		return std::find(m_selectedIndices.begin(), m_selectedIndices.end(), index)
			!= m_selectedIndices.end();
	}

	/*!
	\brief Toggles selection state of a log entry
	\param index The index to toggle
	\param isMultiSelect True if Ctrl is held (add to selection), false to replace selection
	*/
	void LoggerPanel::ToggleSelection(int index, bool isMultiSelect) {
		auto it = std::find(m_selectedIndices.begin(), m_selectedIndices.end(), index);

		if (isMultiSelect) {
			// Multi-select: toggle this index
			if (it != m_selectedIndices.end()) {
				m_selectedIndices.erase(it);
			} else {
				m_selectedIndices.push_back(index);
			}
		} else {
			// Single select: clear others and select this one
			if (it != m_selectedIndices.end() && m_selectedIndices.size() == 1) {
				// Clicking same item again - deselect it
				m_selectedIndices.clear();
			} else {
				m_selectedIndices.clear();
				m_selectedIndices.push_back(index);
			}
		}

		m_lastClickedIndex = index;
	}

	/*!
	\brief Handles drag selection - detects mouse drag and selects range of logs
	\param index The current log entry index
	\param isItemHovered Whether mouse is hovering over this item
	*/
	void LoggerPanel::HandleDragSelection(int index, bool isItemHovered) {
		// Check if left mouse button is being held down
		bool isMouseDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f);

		// Start dragging if mouse moves while button is held
		if (isMouseDragging && !m_isDragging && m_dragStartIndex != -1) {
			m_isDragging = true;
		}

		// Update drag selection range while dragging
		if (m_isDragging && isItemHovered) {
			m_dragEndIndex = index;

			// Select range from start to current index
			SelectRange(m_dragStartIndex, m_dragEndIndex);
		}

		// End dragging when mouse button is released
		if (m_isDragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			m_isDragging = false;
			m_dragStartIndex = -1;
			m_dragEndIndex = -1;
		}
	}

	/*!
	  \brief Selects a range of log entries (inclusive)
	  \param startIndex The starting index of the range
	  \param endIndex The ending index of the range
	  */
	void LoggerPanel::SelectRange(int startIndex, int endIndex) {
		// Ensure start is less than end
		int rangeStart = std::min(startIndex, endIndex);
		int rangeEnd = std::max(startIndex, endIndex);

		// Clear current selection if not holding Ctrl
		if (!ImGui::GetIO().KeyCtrl) {
			m_selectedIndices.clear();
		}

		// Add all indices in range
		for (int i = rangeStart; i <= rangeEnd; ++i) {
			// Only add if not already selected
			if (!IsIndexSelected(i)) {
				m_selectedIndices.push_back(i);
			}
		}
	}
}