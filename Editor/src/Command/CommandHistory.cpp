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
        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateEmptyEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateEmptyEntityEvent&) {
                ExecuteCommand(std::make_unique<CreateEmptyEntityCommand>());
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateCubeEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateCubeEntityEvent&) {
                ExecuteCommand(std::make_unique<CreateCubeEntityCommand>());
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateSphereEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateSphereEntityEvent&) {
                ExecuteCommand(std::make_unique<CreateSphereEntityCommand>());
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateCapsuleEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateCapsuleEntityEvent&) {
                ExecuteCommand(std::make_unique<CreateCapsuleEntityCommand>());
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateCylinderEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateCylinderEntityEvent&) {
                ExecuteCommand(std::make_unique<CreateCylinderEntityCommand>());
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreatePlaneEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreatePlaneEntityEvent&) {
                ExecuteCommand(std::make_unique<CreatePlaneEntityCommand>());
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateUICanvasEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateUICanvasEntityEvent&) {
                ExecuteCommand(std::make_unique<CreateUICanvasEntityCommand>());
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateUIImageEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateUIImageEntityEvent& e) {
                ExecuteCommand(std::make_unique<CreateUIImageEntityCommand>(e.parentCanvas));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::DeleteEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::DeleteEntityEvent& e) {
                ExecuteCommand(std::make_unique<DeleteEntityCommand>(e.entitiesToBeDeleted));
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