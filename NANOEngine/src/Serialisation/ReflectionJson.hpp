#pragma once

#include <type_traits>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <rapidjson/document.h>
#include "../Core/Reflection.hpp"

namespace NE::Serialization {
    using Alloc = rapidjson::Document::AllocatorType;

    // ------------------------------
    // 1) Detect reflectable types
    // ------------------------------
    template<class T, class = void>
    struct is_reflectable : std::false_type {};

    template<class T>
    struct is_reflectable<T, std::void_t<decltype(T::Reflect())>> : std::true_type {};

    template<class T>
    inline constexpr bool is_reflectable_v =
        is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>::value;

    // ------------------------------
    // 2) Leaf adapters (extend as needed)
    // ------------------------------
    // Arithmetic
    template<class T>
        requires std::is_arithmetic_v<T>
    inline rapidjson::Value to_json(const T& x, Alloc&) { return rapidjson::Value(x); }

    inline void from_json(const rapidjson::Value& v, float& x) { if (v.IsNumber()) x = v.GetFloat(); }
    inline void from_json(const rapidjson::Value& v, double& x) { if (v.IsNumber()) x = v.GetDouble(); }
    inline void from_json(const rapidjson::Value& v, int& x) { if (v.IsInt())    x = v.GetInt(); }
    inline void from_json(const rapidjson::Value& v, unsigned x) { if (v.IsUint())   x = v.GetUint(); }
    inline void from_json(const rapidjson::Value& v, int64_t& x) { if (v.IsInt64())  x = v.GetInt64(); }
    inline void from_json(const rapidjson::Value& v, uint64_t& x) { if (v.IsUint64()) x = v.GetUint64(); }
    inline void from_json(const rapidjson::Value& v, bool& x) { if (v.IsBool())    x = v.GetBool(); }

    // std::string
    inline rapidjson::Value to_json(const std::string& s, Alloc& a) { return rapidjson::Value(s.c_str(), a); }
    inline void from_json(const rapidjson::Value& v, std::string& s) { if (v.IsString()) s = v.GetString(); }

    // std::filesystem::path
    inline rapidjson::Value to_json(const std::filesystem::path& p, Alloc& a) {
        return rapidjson::Value(p.string().c_str(), a);
    }
    inline void from_json(const rapidjson::Value& v, std::filesystem::path& p) {
        if (v.IsString()) p = v.GetString();
    }

    // Example math type: Math::Vec3 (adjust if your namespace/type differs)
    namespace Math { struct Vec3 { float x{}, y{}, z{}; }; } // If you already have this, this forward is harmless

    inline rapidjson::Value to_json(const Math::Vec3& v, Alloc& a) {
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("x", v.x, a);
        obj.AddMember("y", v.y, a);
        obj.AddMember("z", v.z, a);
        return obj;
    }
    inline void from_json(const rapidjson::Value& v, Math::Vec3& out) {
        if (!v.IsObject()) return;
        if (v.HasMember("x")) out.x = v["x"].GetFloat();
        if (v.HasMember("y")) out.y = v["y"].GetFloat();
        if (v.HasMember("z")) out.z = v["z"].GetFloat();
    }

    // std::vector<T>
    template<class T>
    rapidjson::Value to_json(const std::vector<T>& arr, Alloc& a) {
        rapidjson::Value v(rapidjson::kArrayType);
        for (auto& e : arr) v.PushBack(to_json(e, a), a);
        return v;
    }
    template<class T>
    void from_json(const rapidjson::Value& v, std::vector<T>& out) {
        if (!v.IsArray()) return;
        out.clear(); out.reserve(v.Size());
        for (auto& e : v.GetArray()) {
            T elem{}; from_json(e, elem); out.emplace_back(std::move(elem));
        }
    }

    // ------------------------------
    // 3) Generic reflective object (de)serializer
    // ------------------------------
    //template<class T>
    //    requires is_reflectable_v<T>
    //rapidjson::Value to_json(const T& obj, Alloc& a) {
    //    using namespace NANOEngine::Core; // For ForEachField / FieldDescriptor
    //    rapidjson::Value out(rapidjson::kObjectType);
    //    ForEachField<T>([&](auto desc) {
    //        const auto& field = obj.*(desc.member);
    //        rapidjson::Value name(desc.name.data(), static_cast<rapidjson::SizeType>(desc.name.size()), a);
    //        out.AddMember(name, to_json(field, a), a);
    //        });
    //    return out;
    //}

    //template<class T>
    //    requires is_reflectable_v<T>
    //void from_json(const rapidjson::Value& in, T& obj) {
    //    using namespace NANOEngine::Core;
    //    if (!in.IsObject()) return;
    //    ForEachField<T>(obj, [&](auto desc, auto& field) {
    //        const char* n = desc.name.data();
    //        if (in.HasMember(n)) from_json(in[n], field);
    //        });
    //}
}