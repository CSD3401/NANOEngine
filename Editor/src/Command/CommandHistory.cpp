#include "CommandHistory.hpp"

namespace Editor {
    CommandHistory& CommandHistory::GetInstance() {
        static CommandHistory instance;
        return instance;
    }

    void CommandHistory::ExecuteCommand(std::unique_ptr<ICommand> command) {
        command->Execute();
        m_undoList.push_back(std::move(command));
        m_redoList.clear();
    }

    void CommandHistory::Undo() {
        if (m_undoList.empty()) return;
        auto command = std::move(m_undoList.back());
        m_undoList.pop_back();
        command->Undo();
        m_redoList.push_back(std::move(command));
    }

    void CommandHistory::Redo() {
        if (m_redoList.empty()) return;
        auto command = std::move(m_redoList.back());
        m_redoList.pop_back();
        command->Execute();
        m_undoList.push_back(std::move(command));
    }

    std::vector<std::unique_ptr<ICommand>>& CommandHistory::GetUndoList()
    {
        return m_undoList;
    }

    std::vector<std::unique_ptr<ICommand>>& CommandHistory::GetRedoList()
    {
        return m_redoList;
    }

}