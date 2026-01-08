#pragma once
#include <string>
#include <memory>
#include <utility>
#include <typeindex>
#include <type_traits>
#include <functional>
#include "ICommand.hpp"
#include <Core/Reflection.hpp>
#include <Core/SpdLogger.hpp>  // For SPD_DEBUG logging

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
		std::function<Owner& (uint32_t)> m_getter;
	};

    template <typename Alt, typename FieldT>
    class SetColliderVariantFieldCommand final : public ICommand {
    public:
        using MemberPtr = FieldT Alt::*;

        SetColliderVariantFieldCommand(
            uint32_t entity,
            std::string name,
            MemberPtr member,
            FieldT before,
            FieldT after)
            : m_entity(entity)
            , m_name(std::move(name))
            , m_member(member)
            , m_before(std::move(before))
            , m_after(std::move(after))
        {
        }

        void Execute() override { Apply(m_after); }
        void Undo() override { Apply(m_before); }

        const char* GetName() const override { return m_name.c_str(); }

        bool CanCoalesceWith(const ICommand& next) const override
        {
            auto* other = dynamic_cast<const SetColliderVariantFieldCommand*>(&next);
            return other &&
                other->m_entity == m_entity &&
                other->m_member == m_member;
            // Note: we intentionally do NOT require same Alt at runtime here;
            // Apply() will safely no-op if the active alt changed.
        }

        void CoalesceFrom(const ICommand& next) override
        {
            auto const& other = static_cast<const SetColliderVariantFieldCommand&>(next);
            m_after = other.m_after;
            Execute();
        }

        const FieldT& Before() const { return m_before; }
        const FieldT& After()  const { return m_after; }

    private:
        static void MarkDirtyIfPresent(auto& comp)
        {
            if constexpr (requires { comp.isDirty; }) comp.isDirty = true;
        }

        void Apply(const FieldT& v)
        {
            auto& col = NE::ECS::Command::GetEntityCollider(m_entity);

            // Only apply if we're still editing the same active alternative.
            if (auto* alt = std::get_if<Alt>(&col.data)) {
                alt->*m_member = v;
                MarkDirtyIfPresent(col);
            }
        }

    private:
        uint32_t m_entity{};
        std::string m_name;
        MemberPtr m_member{};
        FieldT m_before{}, m_after{};
    };

    template <typename Alt, typename FieldT>
    class SetCanvasRenderModeFieldCommand final : public ICommand {
    public:
        using MemberPtr = FieldT Alt::*;

        SetCanvasRenderModeFieldCommand(
            uint32_t entity,
            std::string name,
            MemberPtr member,
            FieldT before,
            FieldT after)
            : m_entity(entity)
            , m_name(std::move(name))
            , m_member(member)
            , m_before(std::move(before))
            , m_after(std::move(after))
        {
        }

        void Execute() override { Apply(m_after); }
        void Undo() override { Apply(m_before); }

        const char* GetName() const override { return m_name.c_str(); }

        bool CanCoalesceWith(const ICommand& next) const override
        {
            auto* other = dynamic_cast<const SetCanvasRenderModeFieldCommand*>(&next);
            return other &&
                other->m_entity == m_entity &&
                other->m_member == m_member;
            // Note: we intentionally do NOT require same Alt at runtime here;
            // Apply() will safely no-op if the active alt changed.
        }

        void CoalesceFrom(const ICommand& next) override
        {
            auto const& other = static_cast<const SetCanvasRenderModeFieldCommand&>(next);
            m_after = other.m_after;
            Execute();
        }

        const FieldT& Before() const { return m_before; }
        const FieldT& After()  const { return m_after; }

    private:
        static void MarkDirtyIfPresent(auto& comp)
        {
            if constexpr (requires { comp.isDirty; }) comp.isDirty = true;
        }

        void Apply(const FieldT& v)
        {
            auto& col = NE::ECS::Command::GetCanvas(m_entity);

            // Only apply if we're still editing the same active alternative.
            if (auto* alt = std::get_if<Alt>(&col.renderModeData)) {
                alt->*m_member = v;
                MarkDirtyIfPresent(col);
            }
        }

    private:
        uint32_t m_entity{};
        std::string m_name;
        MemberPtr m_member{};
        FieldT m_before{}, m_after{};
    };

    template <typename Alt, typename FieldT>
    class SetCanvasScalarModeFieldCommand final : public ICommand {
    public:
        using MemberPtr = FieldT Alt::*;

        SetCanvasScalarModeFieldCommand(
            uint32_t entity,
            std::string name,
            MemberPtr member,
            FieldT before,
            FieldT after)
            : m_entity(entity)
            , m_name(std::move(name))
            , m_member(member)
            , m_before(std::move(before))
            , m_after(std::move(after))
        {
        }

        void Execute() override { Apply(m_after); }
        void Undo() override { Apply(m_before); }

        const char* GetName() const override { return m_name.c_str(); }

        bool CanCoalesceWith(const ICommand& next) const override
        {
            auto* other = dynamic_cast<const SetCanvasScalarModeFieldCommand*>(&next);
            return other &&
                other->m_entity == m_entity &&
                other->m_member == m_member;
            // Note: we intentionally do NOT require same Alt at runtime here;
            // Apply() will safely no-op if the active alt changed.
        }

        void CoalesceFrom(const ICommand& next) override
        {
            auto const& other = static_cast<const SetCanvasScalarModeFieldCommand&>(next);
            m_after = other.m_after;
            Execute();
        }

        const FieldT& Before() const { return m_before; }
        const FieldT& After()  const { return m_after; }

    private:
        static void MarkDirtyIfPresent(auto& comp)
        {
            if constexpr (requires { comp.isDirty; }) comp.isDirty = true;
        }

        void Apply(const FieldT& v)
        {
            auto& col = NE::ECS::Command::GetCanvas(m_entity);

            // Only apply if we're still editing the same active alternative.
            if (auto* alt = std::get_if<Alt>(&col.scaleModeData)) {
                alt->*m_member = v;
                MarkDirtyIfPresent(col);
            }
        }

    private:
        uint32_t m_entity{};
        std::string m_name;
        MemberPtr m_member{};
        FieldT m_before{}, m_after{};
    };


}