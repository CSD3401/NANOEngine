#pragma once
#include <string>
#include <memory>
#include "ICommand.hpp"

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

            if (m_mask & Pos) t.position = v.position;
            if (m_mask & Rot) t.rotation = v.rotation;
            if (m_mask & Scl) t.scale = v.scale;

            if constexpr (requires(Owner x) { x.isDirty; }) t.isDirty = true;
        }

        uint32_t    m_entity;
        std::string m_name;
        Owner       m_before, m_after;
        Getter      m_get;
        uint8_t     m_mask;
    };

} // namespace Editor
