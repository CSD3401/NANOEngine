#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <Math/Vec3.hpp>
#include <type_traits>

// Lightweight helper to register exposed fields in game scripts and
// provide string-based get/set that the Editor uses.
struct ExposedFieldRegistry {
    struct Entry {
     std::string typeToken;
   std::function<std::string()> getStr;
        std::function<bool(const std::string&)> setStr;
     
        // Additional metadata for enums and structs
std::vector<std::string> enumOptions;  // For enum dropdown
    std::vector<std::string> structFields;  // For nested struct fields
  };

    // Register helpers - accept any callable for getter/setter and wrap conversions
    template<typename Getter, typename Setter>
    void RegisterFloat(const std::string& name, Getter getter, Setter setter) {
      Entry e;
      e.typeToken = "float";
        e.getStr = [getter]() { return std::to_string(getter()); };
    e.setStr = [setter](const std::string& s) {
            try { setter(std::stof(s)); return true; }
   catch (...) { return false; }
};
      m_entries[name] = std::move(e);
    }

    template<typename Getter, typename Setter>
    void RegisterInt(const std::string& name, Getter getter, Setter setter) {
    Entry e;
        e.typeToken = "int";
   e.getStr = [getter]() { return std::to_string(getter()); };
        e.setStr = [setter](const std::string& s) {
      try { setter(std::stoi(s)); return true; }
            catch (...) { return false; }
  };
 m_entries[name] = std::move(e);
    }

    template<typename Getter, typename Setter>
    void RegisterBool(const std::string& name, Getter getter, Setter setter) {
        Entry e;
        e.typeToken = "bool";
  e.getStr = [getter]() { return getter() ? std::string("1") : std::string("0"); };
  e.setStr = [setter](const std::string& s) {
      if (s == "1" || s == "true") { setter(true); return true; }
       if (s == "0" || s == "false") { setter(false); return true; }
      return false;
     };
   m_entries[name] = std::move(e);
    }

    template<typename Getter, typename Setter>
    void RegisterVec3(const std::string& name, Getter getter, Setter setter) {
        Entry e;
  e.typeToken = "vec3";
    e.getStr = [getter]() {
    auto v = getter();
         std::ostringstream oss; oss << v.x << ' ' << v.y << ' ' << v.z; return oss.str();
       };
        e.setStr = [setter](const std::string& s) {
            try {
    std::istringstream iss(s);
  NE::Math::Vec3 v; if (!(iss >> v.x >> v.y >> v.z)) return false; setter(v); return true;
       }
     catch (...) { return false; }
   };
        m_entries[name] = std::move(e);
  }

    template<typename Getter, typename Setter>
    void RegisterString(const std::string& name, Getter getter, Setter setter) {
     Entry e;
  e.typeToken = "string";
        e.getStr = [getter]() { return getter(); };
  e.setStr = [setter](const std::string& s) { try { setter(s); return true; } catch (...) { return false; } };
        m_entries[name] = std::move(e);
    }

  // Register enum with string options
    template<typename Getter, typename Setter>
    void RegisterEnum(const std::string& name, Getter getter, Setter setter, const std::vector<std::string>& options) {
   Entry e;
      e.typeToken = "enum";
 e.getStr = [getter]() { return std::to_string(static_cast<int>(getter())); };
        e.setStr = [setter, options](const std::string& s) {
            try { 
          int idx = std::stoi(s);
                if (idx >= 0 && idx < static_cast<int>(options.size())) {
    setter(idx); 
           return true;
           }
         return false;
            } 
            catch (...) { return false; }
     };
    m_entries[name] = std::move(e);
        m_enumOptions[name] = options; // Store enum options
    }

    // Convenience overloads: register a member pointer directly (no lambdas required)
    template<typename Owner>
    void RegisterMember(const std::string& name, Owner* obj, float Owner::* member) {
        RegisterFloat(name,
            [obj, member]() -> float { return obj->*member; },
            [obj, member](float v) { obj->*member = v; });
    }

    template<typename Owner>
    void RegisterMember(const std::string& name, Owner* obj, int Owner::* member) {
        RegisterInt(name,
            [obj, member]() -> int { return obj->*member; },
            [obj, member](int v) { obj->*member = v; });
    }

    template<typename Owner>
    void RegisterMember(const std::string& name, Owner* obj, bool Owner::* member) {
        RegisterBool(name,
            [obj, member]() -> bool { return obj->*member; },
            [obj, member](bool v) { obj->*member = v; });
    }

    template<typename Owner>
    void RegisterMember(const std::string& name, Owner* obj, std::string Owner::* member) {
        RegisterString(name,
            [obj, member]() -> std::string { return obj->*member; },
            [obj, member](const std::string& s) { obj->*member = s; });
    }

    template<typename Owner>
    void RegisterMember(const std::string& name, Owner* obj, NE::Math::Vec3 Owner::* member) {
    RegisterVec3(name,
            [obj, member]() -> NE::Math::Vec3 { return obj->*member; },
     [obj, member](NE::Math::Vec3 v) { obj->*member = v; });
    }

    // NEW: Convenience overload for enum members
    template<typename Owner, typename EnumType>
    void RegisterEnumMember(const std::string& name, 
   Owner* obj, 
         EnumType Owner::* member,
   const std::vector<std::string>& options) {
        RegisterEnum<EnumType>(name,
  [obj, member]() -> EnumType { return obj->*member; },
         [obj, member](EnumType v) { obj->*member = v; },
     options);
    }

    std::vector<std::string> GetNames() const {
        std::vector<std::string> out; out.reserve(m_entries.size());
        for (auto const& kv : m_entries) out.push_back(kv.first);
   return out;
    }

    std::string GetType(const std::string& name) const {
        auto it = m_entries.find(name);
        return it != m_entries.end() ? it->second.typeToken : std::string();
    }

    std::string GetValue(const std::string& name) const {
    auto it = m_entries.find(name);
  return it != m_entries.end() ? it->second.getStr() : std::string();
    }

    bool SetValue(const std::string& name, const std::string& s) {
   auto it = m_entries.find(name);
   return it != m_entries.end() ? it->second.setStr(s) : false;
    }

    std::vector<std::string> GetEnumOptions(const std::string& name) const {
        auto it = m_enumOptions.find(name);
  return it != m_enumOptions.end() ? it->second : std::vector<std::string>();
    }

private:
  std::unordered_map<std::string, Entry> m_entries;
    std::unordered_map<std::string, std::vector<std::string>> m_enumOptions;
};

// Macro to auto-register a member within a script's constructor
#ifndef REGISTER_FIELD
#define REGISTER_FIELD(member) m_fields.RegisterMember(#member, this, &std::remove_reference_t<decltype(*this)>::member)
#endif

// NEW: Macro to register enum with options
#ifndef REGISTER_ENUM
#define REGISTER_ENUM(member, ...) \
    m_fields.RegisterEnumMember(#member, this, &std::remove_reference_t<decltype(*this)>::member, {__VA_ARGS__})
#endif
