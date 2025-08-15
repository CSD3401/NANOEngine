#ifndef NANOENGINE_REFLECTION_HPP
#define NANOENGINE_REFLECTION_HPP

#include <tuple>
#include <utility>
#include <string_view>

namespace NE::Core {

    template <typename Owner, typename T>
    struct FieldDescriptor {
        std::string_view name;
        T Owner::* member;
    };

    template <typename T, typename F>
    constexpr void ForEachField(F&& f) {
        constexpr auto fields = T::Reflect();
        std::apply([&](auto&&... desc) { (f(desc), ...); }, fields);
    }

    template <typename T, typename Obj, typename F>
    constexpr void ForEachField(Obj&& obj, F&& f) {
        constexpr auto fields = T::Reflect();
        std::apply([&](auto&&... desc) { (f(desc, obj.*(desc.member)), ...); }, fields);
    }
}

#define NE_REFLECT_BEGIN(type) \
    static constexpr auto Reflect() { \
        using Self = type; \
        return std::make_tuple(

#define NE_REFLECT_FIELD(field) \
            NE::Core::FieldDescriptor<Self, decltype(Self::field)>{#field, &Self::field}

#define NE_REFLECT_END() \
        ); \
    }

#endif // NANOENGINE_REFLECTION_HPP
