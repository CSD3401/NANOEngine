#pragma once

#include "ICommand.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Editor {

    class CreateEntityCommand final : public ICommand {
    public:
        CreateEntityCommand();

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Entity"; }

    private:
        uint32_t m_entity;
    };

    class CreateUICanvasEntityCommand final : public ICommand {
    public:
        CreateUICanvasEntityCommand();
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Canvas"; }
    private:
        uint32_t m_entity;
    };

    class CreateUIImageEntityCommand final : public ICommand {
    public:
        CreateUIImageEntityCommand(uint32_t parentCanvas);
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Image"; }
    private:
        uint32_t m_entity;
        uint32_t m_parentCanvas;
    };

    class CreateUIButtonEntityCommand final : public ICommand {
    public:
        CreateUIButtonEntityCommand(uint32_t parentCanvas);
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Button"; }
    private:
        uint32_t m_entity;
        uint32_t m_parentCanvas;
    };

    class DeleteEntityCommand final : public ICommand {
    public:
        DeleteEntityCommand(uint32_t deletedEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Delete Entity"; }

    private:
        uint32_t m_entity;

        struct DeletedUIEntityInfo {
            uint32_t id;
            bool wasCanvas;
            bool wasUIImage;
            uint32_t parentId;  // For UI images
        };
        std::vector<DeletedUIEntityInfo> m_deletedEntities;
    };

    class RenameEntityCommand : public ICommand {
    public:
        RenameEntityCommand(uint32_t entity, const std::string& newName)
            : m_Entity(entity), m_NewName(newName) {}

        void Execute() override {
            //auto& name = ECS::GetComponent<NameComponent>(m_Entity);
            //m_OldName = name.text;
            //name.text = m_NewName;
        }

        void Undo() override {
            //ECS::GetComponent<NameComponent>(m_Entity).text = m_OldName;
        }

    private:
        uint32_t m_Entity;
        std::string m_OldName, m_NewName;
    };
}