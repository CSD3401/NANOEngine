/*!
\file       LoggerPanel.cpp
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
        default:                    return true;
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
        case SpdLogLevel::Info:     return "INFO";
        case SpdLogLevel::Warning:  return "WARNING";
        case SpdLogLevel::Error:    return "ERROR";
        case SpdLogLevel::Critical: return "CRITICAL";
        default:                    return "UNKNOWN";
        }
    }

    /*!
    \brief Renders a single log entry with color coding, timestamp, and context menu
    \param entry The log entry to render with all its information
    \param index The index of this entry used for generating unique ImGui IDs
    */
    void LoggerPanel::RenderLogEntry(const SpdLogEntry& entry, int index) {
        // Color based on log level
        ImVec4 color;
        switch (entry.level) {
        case SpdLogLevel::Debug:    color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); break; // Gray
        case SpdLogLevel::Info:     color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break; // White
        case SpdLogLevel::Warning:  color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break; // Yellow
        case SpdLogLevel::Error:    color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); break; // Red
        case SpdLogLevel::Critical: color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break; // Bright Red
        default:                    color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;
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

        ImGui::PopStyleColor();

        // Right-click context menu for copying functionality
        if (ImGui::BeginPopupContextItem(("LogEntry" + std::to_string(index)).c_str())) {
            if (ImGui::MenuItem("Copy Message")) {
                ImGui::SetClipboardText(entry.message.c_str());
            }
            if (ImGui::MenuItem("Copy Full Entry")) {
                std::string fullEntry = timeBuffer;
                fullEntry += " [" + std::string(GetLevelString(entry.level)) + "] ";
                fullEntry += fileInfo + " " + entry.message;
                ImGui::SetClipboardText(fullEntry.c_str());
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
           Provides Clear Logs, All, None, and Errors Only functionality plus crash logging
    */
    void LoggerPanel::RenderControlButtons() {
        // Control buttons
        if (ImGui::Button("Clear Logs")) {
            SpdLogger::GetInstance().ClearLogEntries();
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

        // Crash simulation buttons (for testing only - remove in production!)
        ImGui::SameLine();
        if (ImGui::Button("[Crash: Access Violation")) {
            SPD_CRASH_LOG("Simulating access violation crash");
            // Force access violation
            int* null_ptr = nullptr;
            *null_ptr = 42;  // This will crash with access violation
        }

        ImGui::SameLine();
        if (ImGui::Button("Crash: Stack Overflow")) {
            SPD_CRASH_LOG("Simulating stack overflow crash");
            // Force stack overflow with infinite recursion
            std::function<void()> infiniteRecursion = [&]() {
                infiniteRecursion();
            };
            infiniteRecursion();
        }

        ImGui::SameLine();
        if (ImGui::Button("Crash: Abort")) {
            SPD_FATAL_CRASH("Simulating controlled crash via abort()");
            // This will call SaveCrashLog() then abort()
        }
    }
}