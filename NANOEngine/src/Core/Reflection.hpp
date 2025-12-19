#ifndef NANOENGINE_REFLECTION_HPP
#define NANOENGINE_REFLECTION_HPP

#include <tuple>
#include <utility>
#include <string_view>
#include <variant>

namespace NE::Core {
    template <class T>
    concept Reflectable = requires { T::Reflect(); };

    namespace {
        template<typename T, typename F>
        void InvokeField(const T& obj, const auto& desc, F&& f) {
            if constexpr (NE::Core::Reflectable<T>) {
                f(desc, obj.*(desc.member));
            } else if constexpr (requires { std::visit([](auto&&) {}, obj.*(desc.member)); }) {
                f(desc, obj.*(desc.member));
            } else {
                f(desc, obj.*(desc.member));
            }
        }
    }

    enum class FieldFlags : std::uint8_t {
        None = 0,
        HiddenInEditor = 1 << 0,
    };

    constexpr FieldFlags operator|(FieldFlags a, FieldFlags b) {
        return static_cast<FieldFlags>(
            static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
            );
    }

    constexpr FieldFlags operator&(FieldFlags a, FieldFlags b) {
        return static_cast<FieldFlags>(
            static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b)
            );
    }

    constexpr bool HasFlag(FieldFlags value, FieldFlags flag) {
        return (value & flag) != FieldFlags::None;
    }

    template <typename Owner, typename T>
    struct FieldDescriptor {
        std::string_view name;
        T Owner::* member;
        FieldFlags flags;
    };

    template <Reflectable T, class F>
    constexpr void ForEachFieldView(const T& obj, F&& f) {
        constexpr auto fields = T::Reflect();
        std::apply([&](auto&&... d) {
            ((InvokeField(obj, d, f)), ...);
            }, fields);
    }

    template <Reflectable T, class F>
    constexpr void ForEachField(T& obj, F&& f) {
        constexpr auto fields = T::Reflect();
        std::apply([&](auto&&... d) {
            ((f(d, obj.*(d.member))), ...);
            }, fields);
    }

}

#define NE_REFLECT_BEGIN(type) \
    static constexpr auto Reflect() { \
        using Self = type; \
        return std::make_tuple(

#define NE_REFLECT_FIELD(field) \
    NE::Core::FieldDescriptor<Self, decltype(Self::field)>{#field, &Self::field, NE::Core::FieldFlags::None}

#define NE_REFLECT_FIELD_NAMED(field, customName) \
    NE::Core::FieldDescriptor<Self, decltype(Self::field)>{customName, &Self::field, NE::Core::FieldFlags::None}

#define NE_REFLECT_FIELD_HIDDEN(field) \
    NE::Core::FieldDescriptor<Self, decltype(Self::field)>{#field, &Self::field, NE::Core::FieldFlags::HiddenInEditor}

#define NE_REFLECT_FIELD_NAMED_HIDDEN(field, customName) \
    NE::Core::FieldDescriptor<Self, decltype(Self::field)>{customName, &Self::field, NE::Core::FieldFlags::HiddenInEditor}

#define NE_REFLECT_END() \
        ); \
    }

#endif // NANOENGINE_REFLECTION_HPP
