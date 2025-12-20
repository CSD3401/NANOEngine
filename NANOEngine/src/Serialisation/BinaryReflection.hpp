#ifndef BINARY_REFLECTION_HPP
#define BINARY_REFLECTION_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <variant>

#include "Math/Vec2.hpp"
#include "Math/Vec3.hpp"
#include "Math/Vec4.hpp"
#include "Core/Reflection.hpp"

namespace NE {

	using ByteBuffer = std::vector<uint8_t>;

    namespace Serialization {
        // low-level helpers
        inline void AppendBytes(ByteBuffer& out, const void* data, size_t size) {
            const auto* p = static_cast<const std::uint8_t*>(data);
            out.insert(out.end(), p, p + size);
        }

        // uint64 to little-endian
        inline void AppendU64LE(ByteBuffer& out, std::uint64_t v) {
            std::uint8_t b[8];
            b[0] = (std::uint8_t)((v >> 0) & 0xFF);
            b[1] = (std::uint8_t)((v >> 8) & 0xFF);
            b[2] = (std::uint8_t)((v >> 16) & 0xFF);
            b[3] = (std::uint8_t)((v >> 24) & 0xFF);
            b[4] = (std::uint8_t)((v >> 32) & 0xFF);
            b[5] = (std::uint8_t)((v >> 40) & 0xFF);
            b[6] = (std::uint8_t)((v >> 48) & 0xFF);
            b[7] = (std::uint8_t)((v >> 56) & 0xFF);
            AppendBytes(out, b, sizeof(b));
        }

        template <NE::Core::Reflectable T>
        inline size_t ToBinary(ByteBuffer& out, const T& obj);

	    // Primitives and std types
        inline size_t ToBinary(ByteBuffer& out, const std::uint64_t& v) {
            const size_t before = out.size();
            AppendU64LE(out, v);
            return out.size() - before;
        }

        inline size_t ToBinary(ByteBuffer& out, std::string_view s) {
            const size_t before = out.size();

            AppendU64LE(out, static_cast<std::uint64_t>(s.size()));

            if (!s.empty())
                AppendBytes(out, s.data(), s.size());

            return out.size() - before;
        }

        inline size_t ToBinary(ByteBuffer& out, const std::string& s) {
            return ToBinary(out, std::string_view{ s });
        }

        inline size_t ToBinary(ByteBuffer& out, const char* s) {
            return ToBinary(out, s ? std::string_view{ s } : std::string_view{});
        }

        inline size_t ToBinary(ByteBuffer& out, const std::filesystem::path& p) {
            const auto u8 = p.u8string();
            std::string_view bytes(reinterpret_cast<const char*>(u8.data()), u8.size());
            return ToBinary(out, bytes);
        }

        template <typename T>
        inline size_t ToBinary(ByteBuffer& out, const T& v) requires std::is_arithmetic_v<T> {
            const size_t before = out.size();

            if constexpr (std::is_floating_point_v<T>) {
                const double d = static_cast<double>(v);
                const std::uint64_t bits = std::bit_cast<std::uint64_t>(d);
                AppendU64LE(out, bits);
            } else {
                const std::int64_t i = static_cast<std::int64_t>(v);
                AppendU64LE(out, static_cast<std::uint64_t>(i));
            }

            return out.size() - before;
        }

        template <typename T>
        inline size_t ToBinary(ByteBuffer& out, const std::vector<T>& vec) {
            const size_t before = out.size();

            AppendU64LE(out, static_cast<std::uint64_t>(vec.size()));

            if constexpr (std::is_trivially_copyable_v<T>) {
                if (!vec.empty())
                    AppendBytes(out, vec.data(), vec.size() * sizeof(T));
            } else {
                for (const auto& e : vec)
                    ToBinary(out, e);
            }

            return out.size() - before;
        }

        template <typename... Ts>
        inline size_t ToBinary(ByteBuffer& out, const std::variant<Ts...>& v) {
            const size_t before = out.size();

            uint32_t index = static_cast<uint32_t>(v.index());
            ToBinary(out, index);

            std::visit([&](auto&& alt) {
                using T = std::remove_cvref_t<decltype(alt)>;

                if constexpr (NE::Core::Reflectable<T>) {
                    ToBinary(out, alt);
                }
            }, v);

            return out.size() - before;
        }

        template <typename E>
        inline size_t ToBinary(ByteBuffer& out, E e) requires std::is_enum_v<E> {
            using U = std::underlying_type_t<E>;
            return ToBinary(out, static_cast<U>(e));
        }

        // Math types
        inline size_t ToBinary(ByteBuffer& out, const NE::Math::Vec2& v) {
            const size_t before = out.size();
            ToBinary(out, v.x);
            ToBinary(out, v.y);
            return out.size() - before;
        }

        inline size_t ToBinary(ByteBuffer& out, const NE::Math::Vec3& v) {
            const size_t before = out.size();
            ToBinary(out, v.x);
            ToBinary(out, v.y);
            ToBinary(out, v.z);
            return out.size() - before;
        }

        inline size_t ToBinary(ByteBuffer& out, const NE::Math::Vec4& v) {
            const size_t before = out.size();
            ToBinary(out, v.x);
            ToBinary(out, v.y);
            ToBinary(out, v.z);
            ToBinary(out, v.w);
            return out.size() - before;
        }

        template <NE::Core::Reflectable T>
        inline size_t ToBinary(ByteBuffer& out, const T& obj) {
            const size_t before = out.size();

            NE::Core::ForEachFieldView(obj, [&](auto&& /*desc*/, auto&& field) {
                ToBinary(out, field);
                });

            return out.size() - before;
        }
    }

    namespace Deserialization {
        inline bool ReadBytes(const uint8_t*& it, const uint8_t* end, void* out, size_t size) {
            if (it + size > end)
                return false;

            std::memcpy(out, it, size);
            it += size;
            return true;
        }

        inline bool ReadU64LE(const uint8_t*& it, const uint8_t* end, std::uint64_t& out) {
            if (it + 8 > end)
                return false;

            out =
                (std::uint64_t(it[0]) << 0) |
                (std::uint64_t(it[1]) << 8) |
                (std::uint64_t(it[2]) << 16) |
                (std::uint64_t(it[3]) << 24) |
                (std::uint64_t(it[4]) << 32) |
                (std::uint64_t(it[5]) << 40) |
                (std::uint64_t(it[6]) << 48) |
                (std::uint64_t(it[7]) << 56);

            it += 8;
            return true;
        }

        template <NE::Core::Reflectable T>
        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, T& obj);

        // primitives and std types
        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, std::uint64_t& v) {
            return ReadU64LE(it, end, v);
        }

        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, std::string& s) {
            std::uint64_t size = 0;
            if (!ReadU64LE(it, end, size))
                return false;

            if (it + size > end)
                return false;

            s.assign(reinterpret_cast<const char*>(it), size);
            it += size;
            return true;
        }

        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, std::filesystem::path& p) {
            std::string s;
            if (!FromBinary(it, end, s))
                return false;

            p = std::filesystem::path(std::u8string(
                reinterpret_cast<const char8_t*>(s.data()),
                reinterpret_cast<const char8_t*>(s.data() + s.size())
            ));
            return true;
        }

        // arithmetic
        template <typename T>
        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, T& v) requires std::is_arithmetic_v<T> {
            std::uint64_t raw = 0;
            if (!ReadU64LE(it, end, raw))
                return false;

            if constexpr (std::is_floating_point_v<T>) {
                const double d = std::bit_cast<double>(raw);
                v = static_cast<T>(d);
            } else {
                const std::int64_t i = static_cast<std::int64_t>(raw);
                v = static_cast<T>(i);
            }

            return true;
        }

        // std::vector
        template <typename T>
        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, std::vector<T>& vec) {
            std::uint64_t count = 0;
            if (!ReadU64LE(it, end, count))
                return false;

            vec.clear();
            vec.resize(static_cast<size_t>(count));

            if constexpr (std::is_trivially_copyable_v<T>) {
                const size_t bytes = vec.size() * sizeof(T);
                return ReadBytes(it, end, vec.data(), bytes);
            } else {
                for (auto& e : vec)
                    if (!FromBinary(it, end, e))
                        return false;
                return true;
            }
        }

        // std::variant
        template <typename... Ts>
        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, std::variant<Ts...>& v) {
            uint32_t index = 0;
            if (!FromBinary(it, end, index) || index >= sizeof...(Ts))
                return false;

            bool success = false;

            [&] <std::size_t... Is>(std::index_sequence<Is...>) {
                ((index == Is ? (v.template emplace<Is>(), success = true, false) : false) || ...);
            }(std::make_index_sequence<sizeof...(Ts)>{});

            if (!success)
                return false;

            return std::visit([&](auto&& value) -> bool {
                using T = std::remove_cvref_t<decltype(value)>;

                if constexpr (NE::Core::Reflectable<T>) {
                    return FromBinary(it, end, value);
                }

            }, v);
        }

        // enums
        template <typename E>
        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, E& e) requires std::is_enum_v<E> {
            using U = std::underlying_type_t<E>;
            U v{};
            if (!FromBinary(it, end, v))
                return false;

            e = static_cast<E>(v);
            return true;
        }

        // math
        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, NE::Math::Vec2& v) {
            return FromBinary(it, end, v.x)
                && FromBinary(it, end, v.y);
        }

        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, NE::Math::Vec3& v) {
            return FromBinary(it, end, v.x)
                && FromBinary(it, end, v.y)
                && FromBinary(it, end, v.z);
        }

        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, NE::Math::Vec4& v) {
            return FromBinary(it, end, v.x)
                && FromBinary(it, end, v.y)
                && FromBinary(it, end, v.z)
                && FromBinary(it, end, v.w);
        }

        template <NE::Core::Reflectable T>
        inline bool FromBinary(const uint8_t*& it, const uint8_t* end, T& obj) {
            NE::Core::ForEachField(obj, [&](auto&& /*desc*/, auto&& field) {
                FromBinary(it, end, field);
                });
            return true;
        }
    }

}

#endif // !BINARY_REFLECTION_HPP
