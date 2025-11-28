#include "CommandHistory.hpp"

#include <memory>

#include <Events/EventBus.hpp>
#include "EditorCommands.hpp"
#include "../EditorEvents.hpp"

namespace Editor {
    CommandHistory& CommandHistory::GetInstance() {
        static CommandHistory instance;
        return instance;
    }

    CommandHistory::CommandHistory() {
        NANOEngine::Events::EventBus::Get().Subscribe<CreateEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const CreateEntityEvent&) {
                ExecuteCommand(std::make_unique<CreateEntityCommand>());
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<CreateUICanvasEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const CreateUICanvasEntityEvent&) {
                ExecuteCommand(std::make_unique<CreateUICanvasEntityCommand>());
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<CreateUIImageEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const CreateUIImageEntityEvent& e) {
                ExecuteCommand(std::make_unique<CreateUIImageEntityCommand>(e.parentCanvas));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<DeleteEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const DeleteEntityEvent& e) {
                ExecuteCommand(std::make_unique<DeleteEntityCommand>(e.deletedEntity));
            }
        );
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