#pragma once

#include <rapidjson/document.h>
#include <filesystem>

#include <Math/Vec2.hpp>
#include <Math/Vec3.hpp>
#include <Math/Vec4.hpp>

namespace Editor {
	using Alloc = rapidjson::Document::AllocatorType;
	using rapidjson::Value;

    namespace Serialization {
        // Primitives and stdlib
        template <typename T>
        Value ToJSON(const T& v, Alloc&) requires std::is_arithmetic_v<T> {
            Value out;
            if constexpr (std::is_floating_point_v<T>) out.SetDouble(static_cast<double>(v));
            else                                       out.SetInt64(static_cast<int64_t>(v));
            return out;
        }

        inline Value ToJSON(const std::string& s, Alloc& a) { return Value(s.c_str(), a); }
        inline Value ToJSON(std::string_view s, Alloc& a) { return Value(s.data(), a); }
        inline Value ToJSON(const char* s, Alloc& a) { return Value(s, a); }
        inline Value ToJSON(const std::filesystem::path& p, Alloc& a) { return Value(p.string().c_str(), a); }
        inline Value ToJSON(const uint64_t& v, Alloc&) {
            Value out;
            out.SetUint64(v);
            return out;
        }

        // std::vector
        template <typename T>
        Value ToJSON(const std::vector<T>& vec, Alloc& a) {
            Value arr(rapidjson::kArrayType);
            for (const auto& item : vec) {
                arr.PushBack(ToJSON(item, a), a);
            }
            return arr;
        }

        // enums
        template <typename E>
        Value ToJSON(E e, Alloc&) requires std::is_enum_v<E> {
            return Value(static_cast<int>(e));
        }

        // Math::Vec2
        inline Value ToJSON(const NE::Math::Vec2& v, Alloc& a) {
            Value obj(rapidjson::kObjectType);
            obj.AddMember("x", v.x, a);
            obj.AddMember("y", v.y, a);
            return obj;
        }

        // Math::Vec3
        inline Value ToJSON(const NE::Math::Vec3& v, Alloc& a) {
            Value obj(rapidjson::kObjectType);
            obj.AddMember("x", v.x, a);
            obj.AddMember("y", v.y, a);
            obj.AddMember("z", v.z, a);
            return obj;
        }

        // Math::Vec4
        inline Value ToJSON(const NE::Math::Vec4& v, Alloc& a) {
            Value obj(rapidjson::kObjectType);
            obj.AddMember("x", v.x, a);
            obj.AddMember("y", v.y, a);
            obj.AddMember("z", v.z, a);
            obj.AddMember("w", v.w, a);
            return obj;
        }

        // Reflectable Objects
        template <NE::Core::Reflectable T>
        Value ToJSON(const T& obj, Alloc& a) {
            Value o(rapidjson::kObjectType);
            NE::Core::ForEachFieldView(obj, [&](auto&& desc, auto&& field) {
                Value key(desc.name.data(), a);
                o.AddMember(key, ToJSON(field, a), a);
                });
            return o;
        }
    }

    namespace Deserialization {
        template <typename T>
        void FromJSON(const Value& v, T& out) requires std::is_arithmetic_v<T> {
            if constexpr (std::is_floating_point_v<T>) out = static_cast<T>(v.GetDouble());
            else                                       out = static_cast<T>(v.GetInt64());
        }
        inline void FromJSON(const Value& v, std::string& out) { out = v.GetString(); }
        inline void FromJSON(const Value& v, std::filesystem::path& out) { out = v.GetString(); }
        inline void FromJSON(const Value& v, uint64_t& out) { out = v.GetUint64(); }

        // std::vector
        template <typename T>
        void FromJSON(const Value& v, std::vector<T>& out) {
            out.clear();
            if (v.IsArray()) {
                for (const auto& item : v.GetArray()) {
                    T value;
                    FromJSON(item, value);
                    out.push_back(value);
                }
            }
        }

        // enums
        template <typename E>
        void FromJSON(const Value& v, E& e) requires std::is_enum_v<E> {
            e = static_cast<E>(v.GetInt());
        }

        // Math::Vec2
        inline void FromJSON(const Value& v, NE::Math::Vec2& out) {
            if (v.HasMember("x")) out.x = v["x"].GetFloat();
            if (v.HasMember("y")) out.y = v["y"].GetFloat();
        }

        // Math::Vec3
        inline void FromJSON(const Value& v, NE::Math::Vec3& out) {
            if (v.HasMember("x")) out.x = v["x"].GetFloat();
            if (v.HasMember("y")) out.y = v["y"].GetFloat();
            if (v.HasMember("z")) out.z = v["z"].GetFloat();
        }

        // Math::Vec4
        inline void FromJSON(const Value& v, NE::Math::Vec4& out) {
            if (v.HasMember("x")) out.x = v["x"].GetFloat();
            if (v.HasMember("y")) out.y = v["y"].GetFloat();
            if (v.HasMember("z")) out.z = v["z"].GetFloat();
            if (v.HasMember("w")) out.w = v["w"].GetFloat();
        }

        // Reflectable Objects
        template <NE::Core::Reflectable T>
        void FromJSON(const Value& v, T& out) {
            NE::Core::ForEachField(out, [&](auto&& desc, auto& field) {
                const auto key = desc.name;
                if (v.HasMember(rapidjson::StringRef(key.data(), static_cast<rapidjson::SizeType>(key.size())))) {
                    const auto& sub = v[key.data()];
                    FromJSON(sub, field);
                }
                });

            // set isDirty if present on the type
            //if constexpr (requires (T t) { t.isDirty; }) {
            //    out.isDirty = true;
            //}
        }
    }
}
