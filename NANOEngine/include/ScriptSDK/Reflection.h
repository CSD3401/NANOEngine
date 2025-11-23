#ifndef NANOENGINE_REFLECTION_HPP
#define NANOENGINE_REFLECTION_HPP

#include <tuple>
#include <utility>
#include <string_view>

namespace NE::Core {

    enum class FieldFlags : std::uint8_t {
        None = 0,
        HiddenInEditor = 1 << 0,
        // You can add more later, e.g. ReadOnly = 1 << 1, etc.
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

    template <class T>
    concept Reflectable = requires { T::Reflect(); };

    //template <typename T, typename F>
    //constexpr void ForEachField(F&& f) {
    //    constexpr auto fields = T::Reflect();
    //    std::apply([&](auto&&... desc) { (f(desc), ...); }, fields);
    //}

    //template <typename T, typename Obj, typename F>
    //constexpr void ForEachField(Obj&& obj, F&& f) {
    //    constexpr auto fields = T::Reflect();
    //    std::apply([&](auto&&... desc) { (f(desc, obj.*(desc.member)), ...); }, fields);
    //}
    template <Reflectable T, class F>
    constexpr void ForEachFieldView(const T& obj, F&& f) {
        constexpr auto fields = T::Reflect();
        std::apply([&](auto&&... d) {
            ((f(d, std::as_const(obj).*(d.member))), ...);
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
