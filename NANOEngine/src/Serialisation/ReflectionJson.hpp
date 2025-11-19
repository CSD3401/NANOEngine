#pragma once
#include <string>
#include <type_traits>
#include <filesystem>
#include <rapidjson/document.h>
#include "../../src/Math/Vec3.hpp"
#include "../Core/Reflection.hpp"
#include "ECS/Components/Light.hpp" //temp
#include "ECS/Components/NativeScript.hpp"

namespace NE::Serialization {

    using Alloc = rapidjson::Document::AllocatorType;
    using RJson = rapidjson::Value;

    // ----------- Primitives & std types -----------
    template <typename T>
    RJson to_json(const T& v, Alloc&) requires std::is_arithmetic_v<T> {
        RJson out;
        if constexpr (std::is_floating_point_v<T>) out.SetDouble(static_cast<double>(v));
        else                                       out.SetInt64(static_cast<int64_t>(v));
        return out;
    }
    inline RJson to_json(const std::string& s, Alloc& a) { return RJson(s.c_str(), a); }
    inline RJson to_json(std::string_view s, Alloc& a) { return RJson(s.data(), a); }
    inline RJson to_json(const char* s, Alloc& a) { return RJson(s, a); }
    inline RJson to_json(const std::filesystem::path& p, Alloc& a) { return RJson(p.string().c_str(), a); }
    inline RJson to_json(const uint64_t& v, Alloc&) {
        RJson out;
        out.SetUint64(v);
        return out;
    }

    template <typename T>
    void from_json(const RJson& v, T& out) requires std::is_arithmetic_v<T> {
        if constexpr (std::is_floating_point_v<T>) out = static_cast<T>(v.GetDouble());
        else                                       out = static_cast<T>(v.GetInt64());
    }
    inline void from_json(const RJson& v, std::string& out) { out = v.GetString(); }
    inline void from_json(const RJson& v, std::filesystem::path& out) { out = v.GetString(); }
    inline void from_json(const RJson& v, uint64_t& out) { out = v.GetUint64(); }

    // ----------- Enums -----------
    template <typename E>
    RJson to_json(E e, Alloc&) requires std::is_enum_v<E> {
        return RJson(static_cast<int>(e));
    }
    template <typename E>
    void from_json(const RJson& v, E& e) requires std::is_enum_v<E> {
        e = static_cast<E>(v.GetInt());
    }

    // ----------- Example: Math::Vec3 -----------
    // Add similar tiny adapters for other math types you use.
    inline RJson to_json(const NE::Math::Vec3& v, Alloc& a) {
        RJson obj(rapidjson::kObjectType);
        obj.AddMember("x", v.x, a);
        obj.AddMember("y", v.y, a);
        obj.AddMember("z", v.z, a);
        return obj;
    }
    inline void from_json(const RJson& v, NE::Math::Vec3& out) {
        if (v.HasMember("x")) out.x = v["x"].GetFloat();
        if (v.HasMember("y")) out.y = v["y"].GetFloat();
        if (v.HasMember("z")) out.z = v["z"].GetFloat();
    }

    // ----------- Reflectable objects -----------
    template <NE::Core::Reflectable T>
    RJson to_json(const T& obj, Alloc& a) {
        RJson o(rapidjson::kObjectType);
        NE::Core::ForEachFieldView(obj, [&](auto&& desc, auto&& field) {
            // field name is a std::string_view per FieldDescriptor
            RJson key(desc.name.data(), a);
            o.AddMember(key, to_json(field, a), a);
            });
        return o;
    }

    template <NE::Core::Reflectable T>
    void from_json(const RJson& v, T& out) {
        NE::Core::ForEachField(out, [&](auto&& desc, auto& field) {
            const auto key = desc.name; // std::string_view
            if (v.HasMember(rapidjson::StringRef(key.data(), static_cast<rapidjson::SizeType>(key.size())))) {
                const auto& sub = v[key.data()];
                from_json(sub, field);
            }
            });

        // set isDirty if present on the type
        if constexpr (requires (T t) { t.isDirty; }) {
            out.isDirty = true;
        }
    }

    // quick hack stuff

    inline NE::Serialization::RJson to_json(const NE::ECS::Component::Light& l, NE::Serialization::Alloc& a) {
        using namespace NE::Serialization;
        auto obj = to_json<NE::ECS::Component::Light>(static_cast<const NE::ECS::Component::Light&>(l), a);
        obj.AddMember("type", to_json(l.type, a), a);
        return obj;
    }

    inline void from_json(const NE::Serialization::RJson& v, NE::ECS::Component::Light& out) {
        using namespace NE::Serialization;
        from_json<NE::ECS::Component::Light>(v, out);
        if (v.HasMember("type")) from_json(v["type"], out.type);
    }

    // ----------- NativeScript serialization -----------
    // Custom serialization to handle ScriptName and SerializedFields
    inline RJson to_json(const NE::ECS::Component::NativeScript& script, Alloc& a) {
        RJson obj(rapidjson::kObjectType);
  
     // Serialize script name
        obj.AddMember("ScriptName", to_json(script.ScriptName, a), a);
     
        // Serialize field values as a nested object
        if (!script.SerializedFields.empty()) {
          RJson fieldsObj(rapidjson::kObjectType);
       for (const auto& [name, value] : script.SerializedFields) {
   RJson keyJson(name.c_str(), a);
     fieldsObj.AddMember(keyJson, to_json(value, a), a);
            }
   obj.AddMember("SerializedFields", fieldsObj, a);
        }
        
        return obj;
    }

    inline void from_json(const RJson& v, NE::ECS::Component::NativeScript& out) {
   // Deserialize script name
     if (v.HasMember("ScriptName")) {
   from_json(v["ScriptName"], out.ScriptName);
     }

        // Deserialize field values
   if (v.HasMember("SerializedFields")) {
 const auto& fieldsObj = v["SerializedFields"];
            for (auto it = fieldsObj.MemberBegin(); it != fieldsObj.MemberEnd(); ++it) {
     std::string fieldName = it->name.GetString();
    std::string fieldValue;
                from_json(it->value, fieldValue);
     out.SerializedFields[fieldName] = fieldValue;
      }
    }
    }

}