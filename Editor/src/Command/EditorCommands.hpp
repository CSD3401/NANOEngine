#pragma once

#include "ICommand.hpp"
#include <cstdint>
#include <string>
#include <vector>

#include <ECS/Core/Entity.hpp>

namespace Editor {

    class CreateEmptyEntityCommand final : public ICommand {
    public:
        CreateEmptyEntityCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Empty"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    class CreateCubeEntityCommand final : public ICommand {
    public:
        CreateCubeEntityCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Cube"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    class CreateSphereEntityCommand final : public ICommand {
    public:
        CreateSphereEntityCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Sphere"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    class CreateCapsuleEntityCommand final : public ICommand {
    public:
        CreateCapsuleEntityCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Capsule"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    class CreateCylinderEntityCommand final : public ICommand {
    public:
        CreateCylinderEntityCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Cylinder"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    class CreatePlaneEntityCommand final : public ICommand {
    public:
        CreatePlaneEntityCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Plane"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    class CreateQuadEntityCommand final : public ICommand {
    public:
        CreateQuadEntityCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Quad"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    class CreateDirectionalLightCommand final : public ICommand {
    public:
        CreateDirectionalLightCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Directional Light"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    class CreatePointLightCommand final : public ICommand {
    public:
        CreatePointLightCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Point Light"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    class CreateSpotLightCommand final : public ICommand {
    public:
        CreateSpotLightCommand(uint32_t parentEntity);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create Spot Light"; }

    private:
        uint32_t m_entity;
        uint32_t m_parentEntity;
    };

    // Helper function to find or create a canvas
    uint32_t FindOrCreateCanvas();

    class CreateUICanvasCommand final : public ICommand {
    public:
        CreateUICanvasCommand();
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Canvas"; }
    private:
        uint32_t m_entity = NE::ECS::NO_ENTITY;
    };

    class CreateUITextCommand final : public ICommand {
    public:
        CreateUITextCommand(uint32_t parentEntity);
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Text"; }
    private:
        uint32_t m_entity = NE::ECS::NO_ENTITY;
        uint32_t m_parentEntity;
        uint32_t m_canvasEntity = NE::ECS::NO_ENTITY;
        bool m_createdCanvas = false;
    };

    class CreateUIImageCommand final : public ICommand {
    public:
        CreateUIImageCommand(uint32_t parentEntity);
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Image"; }
    private:
        uint32_t m_entity = NE::ECS::NO_ENTITY;
        uint32_t m_parentEntity;
        uint32_t m_canvasEntity = NE::ECS::NO_ENTITY;
        bool m_createdCanvas = false;
    };

    class CreateUIButtonCommand final : public ICommand {
    public:
        CreateUIButtonCommand(uint32_t parentEntity);
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Button"; }
    private:
        uint32_t m_entity = NE::ECS::NO_ENTITY;
        uint32_t m_textEntity = NE::ECS::NO_ENTITY;
        uint32_t m_parentEntity;
        uint32_t m_canvasEntity = NE::ECS::NO_ENTITY;
        bool m_createdCanvas = false;
    };

    class CreateUIPanelCommand final : public ICommand {
    public:
        CreateUIPanelCommand(uint32_t parentEntity);
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Panel"; }
    private:
        uint32_t m_entity = NE::ECS::NO_ENTITY;
        uint32_t m_parentEntity;
        uint32_t m_canvasEntity = NE::ECS::NO_ENTITY;
        bool m_createdCanvas = false;
    };

    class CreateUISliderCommand final : public ICommand {
    public:
        CreateUISliderCommand(uint32_t parentEntity);
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Slider"; }
    private:
        uint32_t m_entity = NE::ECS::NO_ENTITY;            // Slider root
        uint32_t m_backgroundEntity = NE::ECS::NO_ENTITY;  // Background image
        uint32_t m_fillAreaEntity = NE::ECS::NO_ENTITY;    // Fill area container
        uint32_t m_fillEntity = NE::ECS::NO_ENTITY;        // Fill image
        uint32_t m_handleAreaEntity = NE::ECS::NO_ENTITY;  // Handle slide area container
        uint32_t m_handleEntity = NE::ECS::NO_ENTITY;      // Handle image
        uint32_t m_parentEntity;
        uint32_t m_canvasEntity = NE::ECS::NO_ENTITY;
        bool m_createdCanvas = false;
    };

    class CreateUIToggleCommand final : public ICommand {
    public:
        CreateUIToggleCommand(uint32_t parentEntity);
        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Create UI Toggle"; }
    private:
        uint32_t m_entity = NE::ECS::NO_ENTITY;           // Toggle root
        uint32_t m_backgroundEntity = NE::ECS::NO_ENTITY; // Background image
        uint32_t m_checkmarkEntity = NE::ECS::NO_ENTITY;  // Checkmark image
        uint32_t m_labelEntity = NE::ECS::NO_ENTITY;      // Label text
        uint32_t m_parentEntity;
        uint32_t m_canvasEntity = NE::ECS::NO_ENTITY;
        bool m_createdCanvas = false;
    };

    class DeleteEntityCommand final : public ICommand {
    public:
        explicit DeleteEntityCommand(std::vector<uint32_t> rootEntitiesToDelete);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Delete Entity"; }

    private:
        struct DeletedRootSnapshot {
            std::vector<uint8_t> blob;
            uint32_t oldParent = NE::ECS::NO_ENTITY;
            int oldIndex = -1;
            bool wasRoot = false;
            uint32_t liveEntityId = NE::ECS::NO_ENTITY;
        };

        std::vector<uint32_t> m_initialRootEntities;
        std::vector<DeletedRootSnapshot> m_snapshots;

        //struct DeletedUIEntityInfo {
        //    uint32_t id;
        //    bool wasCanvas;
        //    bool wasUIImage;
        //    uint32_t parentId;  // For UI images
        //};
        //std::vector<DeletedUIEntityInfo> m_deletedEntities;
    };

    class HierarchyChangeCommand final : public ICommand {
    public:
        HierarchyChangeCommand(uint32_t child, uint32_t newParent, int newInsertIndex);

        void Execute() override;
        void Undo() override;
        const char* GetName() const override { return "Hierarchy Change"; }

    private:
        uint32_t childEntity = NE::ECS::NO_ENTITY;

        uint32_t oldParentEntity = NE::ECS::NO_ENTITY;
        int      oldInsertIndex = -1;

        uint32_t newParentEntity = NE::ECS::NO_ENTITY;
        int      newInsertIndex = -1;
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
