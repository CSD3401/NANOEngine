#pragma once
#include <string>
#include <memory>
#include "ICommand.hpp"
#include <Engine.hpp>  // For MarkSceneDirty
#include <EngineState.hpp>  // For GetEngineState
#include <Core/SpdLogger.hpp>  
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/UIRectTransform.hpp>

namespace Editor {

    class SetTransformCommand final : public ICommand {
    public:
        using Owner = NE::ECS::Component::Transform;
        using Getter = Owner & (*)(uint32_t);

        enum OpMask : uint8_t { Pos = 1 << 0, Rot = 1 << 1, Scl = 1 << 2 };

        SetTransformCommand(uint32_t entity,
            std::string name,
            Owner before,
            Owner after,
            Getter get,
            uint8_t mask)
            : m_entity(entity), m_name(std::move(name)),
            m_before(std::move(before)), m_after(std::move(after)),
            m_get(get), m_mask(mask) {
        }

        void Execute() override { Apply(m_after); }
        void Undo()    override { Apply(m_before); }
        const char* GetName() const override { return m_name.c_str(); }

        void SetAfter(const Owner& v) { m_after = v; Execute(); }

        const Owner& Before() const { return m_before; }
        const Owner& After()  const { return m_after; }
        uint8_t Mask() const { return m_mask; }

    private:
        void Apply(const Owner& v) {
            Owner& t = m_get(m_entity);

            if (m_mask & Pos) t.localPosition = v.localPosition;
            if (m_mask & Rot) t.localRotationEuler = v.localRotationEuler;
            if (m_mask & Scl) t.localScale = v.localScale;

            if constexpr (requires(Owner x) { x.isDirty; }) t.isDirty = true;
     
            // Mark scene dirty for serialization (Edit mode only)
            if (NE::GetEngineState() == NE::EngineState::Edit) {
                NE::MarkSceneDirty();
                //SPD_DEBUG("[DirtyFlag] Transform changed via Gizmo - Scene marked DIRTY");
            }
        }

        uint32_t    m_entity;
        std::string m_name;
        Owner       m_before, m_after;
        Getter      m_get;
        uint8_t     m_mask;
    };

    class SetUITransformCommand final : public ICommand {
    public:
        using Owner = NE::ECS::Component::UIRectTransform;
        using Getter = Owner & (*)(uint32_t);

        enum OpMask : uint8_t {
            Pos = 1 << 0,   // x, y, z
            Rot = 1 << 1,   // rotationX, Y, Z
            Scl = 1 << 2,   // scaleX, Y, Z
            Size = 1 << 3   // width, height
        };

        SetUITransformCommand(uint32_t entity,
            std::string name,
            Owner before,
            Owner after,
            Getter get,
            uint8_t mask = Pos | Rot | Scl | Size)
            : m_entity(entity), m_name(std::move(name)),
            m_before(std::move(before)), m_after(std::move(after)),
            m_get(get), m_mask(mask) {
        }

        void Execute() override { Apply(m_after); }
        void Undo()    override { Apply(m_before); }
        const char* GetName() const override { return m_name.c_str(); }
        void SetAfter(const Owner& v) { m_after = v; Execute(); }

        const Owner& Before() const { return m_before; }
        const Owner& After()  const { return m_after; }
        uint8_t Mask() const { return m_mask; }

    private:
        void Apply(const Owner& v) {
            Owner& rect = m_get(m_entity);

            if (m_mask & Pos) {
                rect.x = v.x;
                rect.y = v.y;
                rect.z = v.z;
            }
            if (m_mask & Rot) {
                rect.rotationX = v.rotationX;
                rect.rotationY = v.rotationY;
                rect.rotationZ = v.rotationZ;
            }
            if (m_mask & Scl) {
                rect.scaleX = v.scaleX;
                rect.scaleY = v.scaleY;
                rect.scaleZ = v.scaleZ;
            }
            if (m_mask & Size) {
                rect.width = v.width;
                rect.height = v.height;
            }

            // Mark scene dirty for serialization (Edit mode only)
            if (NE::GetEngineState() == NE::EngineState::Edit) {
                NE::MarkSceneDirty();
            }
        }

        uint32_t    m_entity;
        std::string m_name;
        Owner       m_before, m_after;
        Getter      m_get;
        uint8_t     m_mask;
    };

} // namespace Editor
