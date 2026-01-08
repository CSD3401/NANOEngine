#pragma once

#include "ICommand.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Editor {

    class CreateEmptyEntityCommand final : public ICommand {
    public:
        CreateEmptyEntityCommand();

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Empty"; }

    private:
        uint32_t m_entity;
    };

    class CreateCubeEntityCommand final : public ICommand {
    public:
        CreateCubeEntityCommand();

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Cube"; }

    private:
        uint32_t m_entity;
    };

    class CreateSphereEntityCommand final : public ICommand {
    public:
        CreateSphereEntityCommand();

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Sphere"; }

    private:
        uint32_t m_entity;
    };

    class CreateCapsuleEntityCommand final : public ICommand {
    public:
        CreateCapsuleEntityCommand();

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Capsule"; }

    private:
        uint32_t m_entity;
    };

    class CreateCylinderEntityCommand final : public ICommand {
    public:
        CreateCylinderEntityCommand();

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Cylinder"; }

    private:
        uint32_t m_entity;
    };

    class CreatePlaneEntityCommand final : public ICommand {
    public:
        CreatePlaneEntityCommand();

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Plane"; }

    private:
        uint32_t m_entity;
    };

    class CreateCanvasEntityCommand final : public ICommand {
    public:
        CreateCanvasEntityCommand();
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Canvas"; }
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

    class DeleteEntityCommand final : public ICommand {
    public:
        DeleteEntityCommand(std::vector<uint32_t> deletedEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Delete Entity"; }

    private:
        std::vector<uint32_t> m_entities;

        //struct DeletedUIEntityInfo {
        //    uint32_t id;
        //    bool wasCanvas;
        //    bool wasUIImage;
        //    uint32_t parentId;  // For UI images
        //};
        //std::vector<DeletedUIEntityInfo> m_deletedEntities;
    };

    class RenameEntityCommand final : public ICommand {
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

    class SetEntityLayerCommand final : public ICommand {
    public:
        SetEntityLayerCommand(uint32_t entity, uint8_t before, uint8_t after);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Change Layer"; }
    private:
        uint32_t m_entity;
        uint8_t m_before, m_after;
    };
}