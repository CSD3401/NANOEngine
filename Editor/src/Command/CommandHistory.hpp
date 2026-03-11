#pragma once
#include <vector>
#include <memory>
#include "ICommand.hpp"

namespace Editor {

    class CommandHistory {
    public:
        static CommandHistory& GetInstance();
        CommandHistory(const CommandHistory&) = delete;
        CommandHistory& operator=(const CommandHistory&) = delete;

        void ExecuteCommand(std::unique_ptr<ICommand> command);
        void Undo();
        void Redo();

        std::vector<std::unique_ptr<ICommand>>& GetUndoList();
        std::vector<std::unique_ptr<ICommand>>& GetRedoList();

    private:
        CommandHistory();
        ~CommandHistory() = default;

        void ClearHistory();
        std::vector<std::unique_ptr<ICommand>> m_undoList;
        std::vector<std::unique_ptr<ICommand>> m_redoList;
    };

}