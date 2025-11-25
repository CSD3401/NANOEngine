#pragma once
#include <string>
#include <sstream>
#include <type_traits>
#include <filesystem>
#include <vector>
#include <rapidjson/document.h>
#include "../../src/Math/Vec3.hpp"
#include "../Core/Reflection.hpp"
#include "../Core/SpdLogger.hpp"
#include "ECS/Components/Light.hpp" //temp
#include "ECS/Components/NativeScript.hpp"
#include "ECS/Components/EntityMeta.hpp"
#include "ECS/Core/Entity.hpp"
#include "ECS/Core/ComponentManager.hpp"
#include "ECS/Core/EntityManager.hpp"

// Forward declare to avoid circular dependency
namespace NE::ECS::Component { struct EntityMeta; }

namespace NE::Serialization {

    using Alloc = rapidjson::Document::AllocatorType;
    using RJson = rapidjson::Value;

    // Serialization context for Entity <-> LUID conversion
    struct SerializationContext {
        NE::ECS::ComponentManager* componentManager = nullptr;
        NE::ECS::EntityManager* entityManager = nullptr;
    };

    // Thread-local context for serialization
    inline thread_local SerializationContext g_serializationContext;

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

    // ----------- Entity serialization (Entity ID <-> LUID conversion) -----------
   

    // Get LUID from Entity ID
    inline uint64_t GetLUIDFromEntity(NE::ECS::Entity entity);

    // Get Entity ID from LUID
    inline NE::ECS::Entity GetEntityFromLUID(uint64_t luid);

    // Serialize Entity as LUID
    inline RJson to_json(const NE::ECS::Entity& entity, Alloc&) {
        RJson out;

        // Special case: NO_ENTITY serializes as 0
        if (entity == NE::ECS::NO_ENTITY) {
            out.SetUint64(0);
            return out;
        }

        uint64_t luid = GetLUIDFromEntity(entity);

        // Warning: Entity without valid LUID will serialize as 0 and fail to deserialize!
        // All scene entities should have valid LUIDs assigned in their EntityMeta component
        out.SetUint64(luid);
        return out;
    }

    // Deserialize LUID to Entity
    inline void from_json(const RJson& v, NE::ECS::Entity& out) {
        uint64_t luid = v.GetUint64();

        // LUID of 0 means NO_ENTITY
        if (luid == 0) {
            out = NE::ECS::NO_ENTITY;
            return;
        }

        out = GetEntityFromLUID(luid);
    }

    // ----------- std::vector serialization -----------
    template <typename T>
    RJson to_json(const std::vector<T>& vec, Alloc& a) {
        RJson arr(rapidjson::kArrayType);
        for (const auto& item : vec) {
            arr.PushBack(to_json(item, a), a);
        }
        return arr;
    }

    template <typename T>
    void from_json(const RJson& v, std::vector<T>& out) {
        out.clear();
        if (v.IsArray()) {
            for (const auto& item : v.GetArray()) {
                T value;
                from_json(item, value);
                out.push_back(value);
            }
        }
    }

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
    // Converts entity ID <-> LUID for entity reference fields during scene save/load
    inline RJson to_json(const NE::ECS::Component::NativeScript& script, Alloc& a) {
        RJson obj(rapidjson::kObjectType);

        // Serialize script name
        obj.AddMember("ScriptName", to_json(script.ScriptName, a), a);

        // Serialize field values as a nested object
        if (!script.SerializedFields.empty()) {
            RJson fieldsObj(rapidjson::kObjectType);
            for (const auto& [name, value] : script.SerializedFields) {
                RJson keyJson(name.c_str(), a);

                // Check if this field contains entity references (needs LUID conversion)
                if (script.EntityReferenceFields.count(name) > 0) {
                    // This is an entity reference field - convert entity ID(s) to LUID(s)

                    // Check if it's a vector (contains commas) or single entity
                    if (value.find(',') != std::string::npos) {
                        // Vector<Entity> - parse comma-separated entity IDs
                        RJson luidsArray(rapidjson::kArrayType);
                        std::stringstream ss(value);
                        std::string entityIdStr;

                        while (std::getline(ss, entityIdStr, ',')) {
                            try {
                                NE::ECS::Entity entityId = static_cast<NE::ECS::Entity>(std::stoul(entityIdStr));
                                uint64_t luid = GetLUIDFromEntity(entityId);
                                luidsArray.PushBack(luid, a);
                            } catch (...) {
                                // Failed to parse - skip this entry
                            }
                        }

                        fieldsObj.AddMember(keyJson, luidsArray, a);
                    } else {
                        // Single entity reference
                        try {
                            NE::ECS::Entity entityId = static_cast<NE::ECS::Entity>(std::stoul(value));
                            uint64_t luid = GetLUIDFromEntity(entityId);

                            // DEBUG: Log the conversion
                            if (name == "tref0") {
                                SPD_DEBUG("[LUID Serialize] Field " << name << ": Entity ID " << entityId << " -> LUID " << luid);
                            }

                            RJson luidJson;
                            luidJson.SetUint64(luid);
                            fieldsObj.AddMember(keyJson, luidJson, a);
                        } catch (...) {
                            // Failed to parse - store as string
                            fieldsObj.AddMember(keyJson, to_json(value, a), a);
                        }
                    }
                } else {
                    // Normal field - store as string
                    fieldsObj.AddMember(keyJson, to_json(value, a), a);
                }
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

                // Check the JSON value type to determine if it's an entity reference
                if (it->value.IsUint64()) {
                    // Single entity LUID - convert to entity ID string
                    uint64_t luid = it->value.GetUint64();
                    NE::ECS::Entity entityId = GetEntityFromLUID(luid);

                    // DEBUG: Log the conversion
                    if (fieldName == "tref0") {
                        SPD_DEBUG("[LUID Deserialize] Field "<< fieldName << ": LUID " << luid << " ->Entity ID " << entityId);
                    }

                    out.SerializedFields[fieldName] = std::to_string(entityId);
                } else if (it->value.IsArray()) {
                    // Vector<Entity> LUIDs - convert to comma-separated entity IDs
                    std::stringstream ss;
                    bool first = true;
                    for (const auto& luidValue : it->value.GetArray()) {
                        if (!first) ss << ",";
                        first = false;

                        uint64_t luid = luidValue.GetUint64();
                        NE::ECS::Entity entityId = GetEntityFromLUID(luid);
                        ss << entityId;
                    }
                    out.SerializedFields[fieldName] = ss.str();
                } else if (it->value.IsString()) {
                    // Normal string field
                    std::string fieldValue;
                    from_json(it->value, fieldValue);
                    out.SerializedFields[fieldName] = fieldValue;
                }
            }
        }
    }

    // ----------- Entity <-> LUID conversion implementation -----------

    inline uint64_t GetLUIDFromEntity(NE::ECS::Entity entity) {
        if (!g_serializationContext.componentManager || entity == NE::ECS::NO_ENTITY) {
            return 0;
        }

        if (!g_serializationContext.componentManager->HasComponent<NE::ECS::Component::EntityMeta>(entity)) {
            return 0;
        }

        return g_serializationContext.componentManager->GetComponent<NE::ECS::Component::EntityMeta>(entity).luid;
    }

    inline NE::ECS::Entity GetEntityFromLUID(uint64_t luid) {
        if (!g_serializationContext.componentManager ||
            !g_serializationContext.entityManager ||
            luid == 0) {
            return NE::ECS::NO_ENTITY;
        }

        const auto& usedEntities = g_serializationContext.entityManager->GetUsedEntities();
        for (NE::ECS::Entity entity : usedEntities) {
            if (g_serializationContext.componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
                const auto& meta = g_serializationContext.componentManager->GetComponent<NE::ECS::Component::EntityMeta>(entity);
                if (meta.luid == luid) {
                    return entity;
                }
            }
        }

        return NE::ECS::NO_ENTITY;
    }

}