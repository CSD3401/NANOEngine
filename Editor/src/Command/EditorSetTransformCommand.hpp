#pragma once
#include <string>
#include <memory>
#include "ICommand.hpp"
#include <Engine.hpp>
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
        }

        uint32_t    m_entity;
        std::string m_name;
        Owner       m_before, m_after;
        Getter      m_get;
        uint8_t     m_mask;
    };

    class SetUIRectTransformCommand final : public ICommand {
    public:
        using Owner = NE::ECS::Component::UIRectTransform;
        using Getter = Owner & (*)(uint32_t);

        enum OpMask : uint8_t {
            Pos = 1 << 0,  // x, y, z
            Rot = 1 << 1,  // rotationX, rotationY, rotationZ
            Scl = 1 << 2,  // scaleX, scaleY, scaleZ
            Size = 1 << 3,  // width, height
            Pivot = 1 << 4,  // pivotX, pivotY
            All = Pos | Rot | Scl | Size | Pivot
        };

        SetUIRectTransformCommand(uint32_t entity,
            std::string name,
            Owner before,
            Owner after,
            Getter get,
            uint8_t mask)
            : m_entity(entity)
            , m_name(std::move(name))
            , m_before(std::move(before))
            , m_after(std::move(after))
            , m_get(get)
            , m_mask(mask)
        {
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

            if (m_mask & Pos) {
                t.x = v.x;
                t.y = v.y;
                t.z = v.z;
            }
            if (m_mask & Rot) {
                t.rotationX = v.rotationX;
                t.rotationY = v.rotationY;
                t.rotationZ = v.rotationZ;
            }
            if (m_mask & Scl) {
                t.scaleX = v.scaleX;
                t.scaleY = v.scaleY;
                t.scaleZ = v.scaleZ;
            }
            if (m_mask & Size) {
                t.width = v.width;
                t.height = v.height;
            }
            if (m_mask & Pivot) {
                t.pivotX = v.pivotX;
                t.pivotY = v.pivotY;
            }
        }

        uint32_t    m_entity;
        std::string m_name;
        Owner       m_before;
        Owner       m_after;
        Getter      m_get;
        uint8_t     m_mask;
    };

} // namespace Editor
