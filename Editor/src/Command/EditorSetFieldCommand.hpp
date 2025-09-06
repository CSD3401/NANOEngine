#pragma once
#include <string>
#include <memory>
#include <utility>
#include <typeindex>
#include <type_traits>
#include <functional>
#include "ICommand.hpp"  // :contentReference[oaicite:0]{index=0}
#include <Core/Reflection.hpp>

namespace Editor {

    // Minimal, type-safe command that knows how to set one field on a component.
    template <typename Owner, typename T>
    class SetFieldCommand final : public ICommand {
    public:
        using MemberPtr = T Owner::*;

        SetFieldCommand(uint32_t entity,
            std::string name,
            MemberPtr member,
            T before,
            T after,
            std::function<Owner& (uint32_t)> getter)
            : m_entity(entity),
            m_name(std::move(name)),
            m_member(member),
            m_before(std::move(before)),
            m_after(std::move(after)),
            m_getter(std::move(getter)) {
        }

        void Execute() override {
            auto& c = m_getter(m_entity);
            c.*m_member = m_after;
            MarkDirtyIfPresent(c);
        }

        void Undo() override {
            auto& c = m_getter(m_entity);
            c.*m_member = m_before;
            MarkDirtyIfPresent(c);
        }

        const char* GetName() const override { return m_name.c_str(); }

        bool CanCoalesceWith(const ICommand& next) const override {
            auto* other = dynamic_cast<const SetFieldCommand*>(&next);
            return other &&
                other->m_entity == m_entity &&
                other->m_member == m_member;
        }

        void CoalesceFrom(const ICommand& next) override {
            auto const& other = static_cast<const SetFieldCommand&>(next);
            m_after = other.m_after;
            Execute();
        }

        const T& Before() const { return m_before; }
        const T& After()  const { return m_after; }

    private:
        template <typename C>
        static void MarkDirtyIfPresent(C& comp) {
            if constexpr (requires(C x) { x.isDirty; }) comp.isDirty = true;
        }

        uint32_t m_entity;
        std::string m_name;
        MemberPtr m_member;
        T m_before, m_after;
        std::function<Owner& (uint32_t)> m_getter; // how to get the component at runtime
    };

} // namespace Editor
