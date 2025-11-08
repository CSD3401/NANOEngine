/*!
\file       LoggerPanel.hpp
\author     Anson Teng
\date       9/9/2025
\brief      This file contains declarations for ImGui Logger Panel.
            Provides visual interface for viewing, filtering, and managing log messages
            captured by SpdLogger with real-time display and search functionality.
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/


#pragma once

#include "IPanel.hpp"
#include "Core/SpdLogger.hpp"
#include <vector>
#include <string>

namespace Editor {
    /*!
    \class LoggerPanel
    \brief ImGui panel for displaying and managing log messages from SpdLogger
           Provides filtering, searching, and visual display of log entries with color coding
    */
    class LoggerPanel : public IPanel {
    public:
        /*!
        \brief Constructor - initializes the logger panel with default settings
        */
        LoggerPanel();

        /*!
        \brief Renders the ImGui interface for the logger panel
               Called every frame to display the logging interface
        */
        void OnImGuiRender() override;

    private:
        // Filter settings for different log levels
        bool m_showDebug = true;        
        bool m_showInfo = true;         
        bool m_showWarning = true;      
        bool m_showError = true;        
        bool m_showCritical = true;     

        // UI settings
        bool m_autoScroll = true;                 
        char m_searchBuffer[256] = { 0 };          
        
        // Selection tracking
        std::vector<int> m_selectedIndices;
        int m_lastClickedIndex = -1;
        
        // Drag selection state
        bool m_isDragging = false;
        int m_dragStartIndex = -1;
        int m_dragEndIndex = -1;
        
        /*!
        \brief Determines if a log entry should be displayed based on current filters
        \param entry The log entry to check
        \return True if the entry should be shown, false otherwise
        */
        bool ShouldShowEntry(const SpdLogEntry& entry) const;

        /*!
        \brief Converts log level enum to display string
        \param level The log level to convert
        \return String representation of the log level
        */
        const char* GetLevelString(SpdLogLevel level) const;

        /*!
        \brief Renders a single log entry with appropriate styling and context menu
        \param entry The log entry to render
        \param index The index of this entry (for unique ID generation)
        */
        void RenderLogEntry(const SpdLogEntry& entry, int index);

        /*!
        \brief Renders the filter checkboxes for different log levels
        */
        void RenderFilterButtons();

        /*!
        \brief Renders control buttons (Clear, All, None, Errors Only)
        */
        void RenderControlButtons();
        
        /*!
        \brief Formats a single log entry to string
        \param entry The log entry to format
        \return Formatted string representation
        */
        std::string FormatLogEntry(const SpdLogEntry& entry) const;
        
        /*!
        \brief Copies all visible/filtered logs to clipboard
        \param filteredEntries The entries currently visible
        */
        void CopyAllVisibleLogs(const std::vector<const SpdLogEntry*>& filteredEntries);
        
        /*!
        \brief Copies selected logs to clipboard
        \param filteredEntries The entries currently visible
        */
        void CopySelectedLogs(const std::vector<const SpdLogEntry*>& filteredEntries);
        
        /*!
        \brief Checks if an index is selected
        \param index The index to check
        \return True if selected
        */
        bool IsIndexSelected(int index) const;
        
        /*!
        \brief Toggles selection of an index
        \param index The index to toggle
        \param isMultiSelect Whether this is a multi-select operation (Ctrl held)
        */
        void ToggleSelection(int index, bool isMultiSelect);
        
        /*!
        \brief Handles drag selection logic
        \param index The current log index being hovered
        \param isItemHovered Whether the mouse is over this item
        */
        void HandleDragSelection(int index, bool isItemHovered);
        
        /*!
        \brief Selects a range of indices (for drag selection)
        \param startIndex The start of the range
        \param endIndex The end of the range
        */
        void SelectRange(int startIndex, int endIndex);
    };
}