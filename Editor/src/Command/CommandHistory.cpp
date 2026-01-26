#include "CommandHistory.hpp"

#include <memory>

#include <Events/EventBus.hpp>
#include "EditorCommands.hpp"
#include "../EditorEvents.hpp"
#include "../EditorScene.hpp"

namespace Editor {
    CommandHistory& CommandHistory::GetInstance() {
        static CommandHistory instance;
        return instance;
    }

    CommandHistory::CommandHistory() {
        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateEmptyEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateEmptyEntityEvent& e) {
                ExecuteCommand(std::make_unique<CreateEmptyEntityCommand>(e.parentEntity));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateCubeEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateCubeEntityEvent& e) {
                ExecuteCommand(std::make_unique<CreateCubeEntityCommand>(e.parentEntity));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateSphereEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateSphereEntityEvent& e) {
                ExecuteCommand(std::make_unique<CreateSphereEntityCommand>(e.parentEntity));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateCapsuleEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateCapsuleEntityEvent& e) {
                ExecuteCommand(std::make_unique<CreateCapsuleEntityCommand>(e.parentEntity));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateCylinderEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateCylinderEntityEvent& e) {
                ExecuteCommand(std::make_unique<CreateCylinderEntityCommand>(e.parentEntity));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreatePlaneEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreatePlaneEntityEvent& e) {
                ExecuteCommand(std::make_unique<CreatePlaneEntityCommand>(e.parentEntity));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateQuadEntityEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateQuadEntityEvent& e) {
                ExecuteCommand(std::make_unique<CreateQuadEntityCommand>(e.parentEntity));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateDirectionalLightEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateDirectionalLightEvent& e) {
                ExecuteCommand(std::make_unique<CreateDirectionalLightCommand>(e.parentEntity));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreatePointLightEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreatePointLightEvent& e) {
                ExecuteCommand(std::make_unique<CreatePointLightCommand>(e.parentEntity));
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::CreateSpotLightEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::CreateSpotLightEvent& e) {
                ExecuteCommand(std::make_unique<CreateSpotLightCommand>(e.parentEntity));
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

        NANOEngine::Events::EventBus::Get().Subscribe<Events::SceneChangedEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::SceneChangedEvent& e) {
                ClearHistory();
            }
        );

        NANOEngine::Events::EventBus::Get().Subscribe<Events::HierarchyChangeEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::HierarchyChangeEvent& e) {
                ExecuteCommand(std::make_unique<HierarchyChangeCommand>(e.childEntity, e.newParentEntity, e.insertIndex));
            }
        );
    }

    void CommandHistory::ClearHistory() {
        m_undoList.clear();
		m_redoList.clear();
    }

    void CommandHistory::ExecuteCommand(std::unique_ptr<ICommand> command) {
        command->Execute();
        m_undoList.push_back(std::move(command));
        m_redoList.clear();
        EditorScene::isDirty = true;
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