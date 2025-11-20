#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <type_traits>
#include <ScriptSDK/Math.h>
#include <ScriptSDK/ScriptTypes.h>
#include <ScriptSDK/Reflection.h>

// Lightweight helper to register exposed fields in game scripts and
// provide string-based get/set that the Editor uses.
struct ExposedFieldRegistry {
	struct Entry {
		std::string typeToken;
		std::function<std::string()> getStr;
		std::function<bool(const std::string&)> setStr;

		// Additional metadata for enums, structs, and arrays
		std::vector<std::string> enumOptions; // For enum dropdown
		std::vector<std::string> structFields;     // For nested struct fields

		// Array/Vector metadata
		std::function<size_t()> getSize;         // Get array size
		std::function<std::string(size_t)> getElement;  // Get element at index
		std::function<bool(size_t, const std::string&)> setElement;  // Set element at index
		std::function<void()> addElement;          // Add new element
		std::function<void(size_t)> removeElement; // Remove element at index
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

	// SDK Vec3 overload - converts between SDK and engine Vec3
	template<typename Owner>
	void RegisterMember(const std::string& name, Owner* obj, NE::Scripting::Vec3 Owner::* member) {
		RegisterVec3(name,
			[obj, member]() -> NE::Math::Vec3 {
				auto sdkVec = obj->*member;
				return NE::Math::Vec3(sdkVec.x, sdkVec.y, sdkVec.z);
			},
			[obj, member](NE::Math::Vec3 v) {
				obj->*member = NE::Scripting::Vec3(v.x, v.y, v.z);
			});
	}

	// NEW: Convenience overload for enum members
	template<typename Owner, typename EnumType>
	void RegisterEnumMember(const std::string& name,
		Owner* obj,
		EnumType Owner::* member,
		const std::vector<std::string>& options) {
		RegisterEnum(name,
			[obj, member]() -> EnumType { return obj->*member; },
			[obj, member](int idx) { obj->*member = static_cast<EnumType>(idx); },
			options);
	}

	// ============ NEW: STRUCT FIELD HELPERS ============

   // Template helper to register nested struct fields
	template<typename Owner, typename StructType, typename FieldType>
	void RegisterStructField(const std::string& structName,
		const std::string& fieldName,
		Owner* owner,
		StructType Owner::* structMember,
		FieldType StructType::* field) {
		std::string fullName = structName + "." + fieldName;

		if constexpr (std::is_same_v<FieldType, int>) {
			RegisterInt(fullName,
				[owner, structMember, field]() -> int { return (owner->*structMember).*field; },
				[owner, structMember, field](int v) { (owner->*structMember).*field = v; });
		}
		else if constexpr (std::is_same_v<FieldType, float>) {
			RegisterFloat(fullName,
				[owner, structMember, field]() -> float { return (owner->*structMember).*field; },
				[owner, structMember, field](float v) { (owner->*structMember).*field = v; });
		}
		else if constexpr (std::is_same_v<FieldType, bool>) {
			RegisterBool(fullName,
				[owner, structMember, field]() -> bool { return (owner->*structMember).*field; },
				[owner, structMember, field](bool v) { (owner->*structMember).*field = v; });
		}
		else if constexpr (std::is_same_v<FieldType, std::string>) {
			RegisterString(fullName,
				[owner, structMember, field]() -> std::string { return (owner->*structMember).*field; },
				[owner, structMember, field](const std::string& v) { (owner->*structMember).*field = v; });
		}
		else if constexpr (std::is_same_v<FieldType, NE::Math::Vec3>) {
			RegisterVec3(fullName,
				[owner, structMember, field]() -> NE::Math::Vec3 { return (owner->*structMember).*field; },
				[owner, structMember, field](const NE::Math::Vec3& v) { (owner->*structMember).*field = v; });
		}
		else if constexpr (std::is_same_v<FieldType, NE::Scripting::Vec3>) {
			RegisterVec3(fullName,
				[owner, structMember, field]() -> NE::Math::Vec3 {
					auto sdkVec = (owner->*structMember).*field;
					return NE::Math::Vec3(sdkVec.x, sdkVec.y, sdkVec.z);
				},
				[owner, structMember, field](const NE::Math::Vec3& v) {
					(owner->*structMember).*field = NE::Scripting::Vec3(v.x, v.y, v.z);
				});
		}
	}

	template<typename Owner, typename StructType>
	void RegisterReflectableStruct(const std::string& structName,
		Owner* owner,
		StructType Owner::* structMember)
		requires NE::Core::Reflectable<StructType>
	{
		// Use the reflection system to iterate over struct fields automatically
		auto& structInstance = owner->*structMember;

		NE::Core::ForEachField(structInstance, [&](auto const& desc, auto& fieldValue) {
			std::string fullName = structName + "." + std::string(desc.name);

			using FieldT = std::remove_reference_t<decltype(fieldValue)>;

			if constexpr (std::is_same_v<FieldT, int>) {
				RegisterInt(fullName,
					[owner, structMember, member = desc.member]() -> int {
						return (owner->*structMember).*member;
					},
					[owner, structMember, member = desc.member](int v) {
						(owner->*structMember).*member = v;
					});
			}
			else if constexpr (std::is_same_v<FieldT, float>) {
				RegisterFloat(fullName,
					[owner, structMember, member = desc.member]() -> float {
						return (owner->*structMember).*member;
					},
					[owner, structMember, member = desc.member](float v) {
						(owner->*structMember).*member = v;
					});
			}
			else if constexpr (std::is_same_v<FieldT, bool>) {
				RegisterBool(fullName,
					[owner, structMember, member = desc.member]() -> bool {
						return (owner->*structMember).*member;
					},
					[owner, structMember, member = desc.member](bool v) {
						(owner->*structMember).*member = v;
					});
			}
			else if constexpr (std::is_same_v<FieldT, std::string>) {
				RegisterString(fullName,
					[owner, structMember, member = desc.member]() -> std::string {
						return (owner->*structMember).*member;
					},
					[owner, structMember, member = desc.member](const std::string& v) {
						(owner->*structMember).*member = v;
					});
			}
			else if constexpr (std::is_same_v<FieldT, NE::Math::Vec3>) {
				RegisterVec3(fullName,
					[owner, structMember, member = desc.member]() -> NE::Math::Vec3 {
						return (owner->*structMember).*member;
					},
					[owner, structMember, member = desc.member](const NE::Math::Vec3& v) {
						(owner->*structMember).*member = v;
					});
			}
			else if constexpr (std::is_same_v<FieldT, NE::Scripting::Vec3>) {
				RegisterVec3(fullName,
					[owner, structMember, member = desc.member]() -> NE::Math::Vec3 {
						auto sdkVec = (owner->*structMember).*member;
						return NE::Math::Vec3(sdkVec.x, sdkVec.y, sdkVec.z);
					},
					[owner, structMember, member = desc.member](const NE::Math::Vec3& v) {
						(owner->*structMember).*member = NE::Scripting::Vec3(v.x, v.y, v.z);
					});
			}
			});
	}

	// ============ NEW: VECTOR REGISTRATION ============

	template<typename T>
	void RegisterVector(const std::string& name, std::vector<T>* vec) {
		Entry e;
		e.typeToken = "vector<" + GetTypeName<T>() + ">";

		// Get entire vector as string (for serialization)
		e.getStr = [vec]() {
			std::ostringstream oss;
			oss << vec->size();
			for (const auto& item : *vec) {
				oss << " " << ToString(item);
			}
			return oss.str();
			};

		// Set entire vector from string
		e.setStr = [vec](const std::string& s) {
			try {
				std::istringstream iss(s);
				size_t size;
				iss >> size;
				vec->clear();
				vec->reserve(size);
				for (size_t i = 0; i < size; ++i) {
					T item;
					if (!FromString(iss, item)) return false;
					vec->push_back(item);
				}
				return true;
			}
			catch (...) { return false; }
			};

		// Array operations
		e.getSize = [vec]() { return vec->size(); };

		e.getElement = [vec](size_t idx) -> std::string {
			if (idx < vec->size()) {
				return ToString((*vec)[idx]);
			}
			return "";
			};

		e.setElement = [vec](size_t idx, const std::string& s) -> bool {
			if (idx < vec->size()) {
				T value;
				std::istringstream iss(s);
				if (FromString(iss, value)) {
					(*vec)[idx] = value;
					return true;
				}
			}
			return false;
			};

		e.addElement = [vec]() {
			vec->push_back(T{}); // Add default-constructed element
			};

		e.removeElement = [vec](size_t idx) {
			if (idx < vec->size()) {
				vec->erase(vec->begin() + idx);
			}
			};

		m_entries[name] = std::move(e);
	}

	// ============ NEW: STRUCT REGISTRATION ============

	template<typename T>
	void RegisterStruct(const std::string& name, T* structPtr) {
		Entry e;
		e.typeToken = "struct";

		// We'll serialize struct as JSON-like format: "{field1:value1,field2:value2}"
		e.getStr = [structPtr, name]() {
			// This requires reflection - for now, just return placeholder
			return "struct(" + name + ")";
			};

		e.setStr = [structPtr](const std::string& s) {
			// Deserialize from string
			return false; // TODO: Implement struct deserialization
			};

		m_entries[name] = std::move(e);
	}

	// Helper: Get type name as string
	template<typename T>
	static std::string GetTypeName() {
		if constexpr (std::is_same_v<T, int>) return "int";
		else if constexpr (std::is_same_v<T, float>) return "float";
		else if constexpr (std::is_same_v<T, bool>) return "bool";
		else if constexpr (std::is_same_v<T, std::string>) return "string";
		else if constexpr (std::is_same_v<T, NE::Math::Vec3>) return "vec3";
		else if constexpr (std::is_same_v<T, NE::Scripting::Vec3>) return "vec3";
		else return "unknown";
	}

	// Helper: Convert value to string
	template<typename T>
	static std::string ToString(const T& value) {
		if constexpr (std::is_same_v<T, std::string>) {
			return value;
		}
		else if constexpr (std::is_same_v<T, bool>) {
			return value ? "1" : "0";
		}
		// ? FIX: Handle std::vector<bool>::reference (proxy object)
		else if constexpr (std::is_same_v<T, std::vector<bool>::reference>) {
			return value ? "1" : "0";
		}
		else if constexpr (std::is_arithmetic_v<T>) {
			return std::to_string(value);
		}
		else if constexpr (std::is_same_v<T, NE::Math::Vec3>) {
			std::ostringstream oss;
			oss << value.x << " " << value.y << " " << value.z;
			return oss.str();
		}
		else if constexpr (std::is_same_v<T, NE::Scripting::Vec3>) {
			std::ostringstream oss;
			oss << value.x << " " << value.y << " " << value.z;
			return oss.str();
		}
		else {
			return "unsupported";
		}
	}

	// Helper: Parse value from stream
	template<typename T>
	static bool FromString(std::istringstream& iss, T& value) {
		if constexpr (std::is_same_v<T, std::string>) {
			iss >> value;
			return !iss.fail();
		}
		else if constexpr (std::is_same_v<T, bool>) {
			int temp;
			iss >> temp;
			value = (temp != 0);
			return !iss.fail();
		}
		// ? FIX: Handle std::vector<bool>::reference (proxy object)
		else if constexpr (std::is_same_v<T, std::vector<bool>::reference>) {
			int temp;
			iss >> temp;
			value = (temp != 0);
			return !iss.fail();
		}
		else if constexpr (std::is_arithmetic_v<T>) {
			iss >> value;
			return !iss.fail();
		}
		else if constexpr (std::is_same_v<T, NE::Math::Vec3>) {
			iss >> value.x >> value.y >> value.z;
			return !iss.fail();
		}
		else if constexpr (std::is_same_v<T, NE::Scripting::Vec3>) {
			iss >> value.x >> value.y >> value.z;
			return !iss.fail();
		}
		else {
			return false;
		}
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

	// Array/Vector operations
	size_t GetArraySize(const std::string& name) const {
		auto it = m_entries.find(name);
		return (it != m_entries.end() && it->second.getSize) ? it->second.getSize() : 0;
	}

	std::string GetArrayElement(const std::string& name, size_t index) const {
		auto it = m_entries.find(name);
		return (it != m_entries.end() && it->second.getElement) ? it->second.getElement(index) : "";
	}

	bool SetArrayElement(const std::string& name, size_t index, const std::string& value) {
		auto it = m_entries.find(name);
		return (it != m_entries.end() && it->second.setElement) ? it->second.setElement(index, value) : false;
	}

	void AddArrayElement(const std::string& name) {
		auto it = m_entries.find(name);
		if (it != m_entries.end() && it->second.addElement) {
			it->second.addElement();
		}
	}

	void RemoveArrayElement(const std::string& name, size_t index) {
		auto it = m_entries.find(name);
		if (it != m_entries.end() && it->second.removeElement) {
			it->second.removeElement(index);
		}
	}

private:

	std::unordered_map<std::string, Entry> m_entries;
	std::unordered_map<std::string, std::vector<std::string>> m_enumOptions; // Enum name -> options
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

// NEW: Macro to register vector
#ifndef REGISTER_VECTOR
#define REGISTER_VECTOR(member) \
 m_fields.RegisterVector(#member, &member)
#endif

// NEW: Macro to register struct
#ifndef REGISTER_STRUCT
#define REGISTER_STRUCT(member) \
 m_fields.RegisterStruct(#member, &member)
#endif

// NEW: Macro to register reflectable struct (uses NE_REFLECT) 
#ifndef REGISTER_REFLECTABLE_STRUCT
#define REGISTER_REFLECTABLE_STRUCT(member) \
    m_fields.RegisterReflectableStruct(#member, this, &std::remove_reference_t<decltype(*this)>::member)
#endif

// NEW: Simple macro to register struct fields (up to 10 fields)
// Usage: REGISTER_STRUCT_FIELDS(stats, health, maxHealth, stamina, level);
#ifndef REGISTER_STRUCT_FIELDS

// Helper to register a single struct field using the template function
#define REGISTER_STRUCT_FIELD(structName, field) \
    m_fields.RegisterStructField(#structName, #field, this, &std::remove_reference_t<decltype(*this)>::structName, &std::remove_reference_t<decltype(structName)>::field)

// Main macro using __VA_ARGS__ (supports 1-10 fields)
#define REGISTER_STRUCT_FIELDS(structName, ...) \
    EXPAND_STRUCT_FIELDS(structName, __VA_ARGS__)

// Expansion macro that applies REGISTER_STRUCT_FIELD to each argument
#define EXPAND_STRUCT_FIELDS(structName, ...) \
    FOR_EACH_FIELD(REGISTER_STRUCT_FIELD, structName, __VA_ARGS__)

// FOR_EACH macro (supports up to 10 fields - can be extended)
#define FOR_EACH_FIELD(macro, structName, ...) \
    GET_MACRO(__VA_ARGS__, \
 FE_10, FE_9, FE_8, FE_7, FE_6, FE_5, FE_4, FE_3, FE_2, FE_1)(macro, structName, __VA_ARGS__)

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, NAME, ...) NAME

#define FE_1(m, s, a1) m(s, a1);
#define FE_2(m, s, a1, a2) m(s, a1); m(s, a2);
#define FE_3(m, s, a1, a2, a3) m(s, a1); m(s, a2); m(s, a3);
#define FE_4(m, s, a1, a2, a3, a4) m(s, a1); m(s, a2); m(s, a3); m(s, a4);
#define FE_5(m, s, a1, a2, a3, a4, a5) m(s, a1); m(s, a2); m(s, a3); m(s, a4); m(s, a5);
#define FE_6(m, s, a1, a2, a3, a4, a5, a6) m(s, a1); m(s, a2); m(s, a3); m(s, a4); m(s, a5); m(s, a6);
#define FE_7(m, s, a1, a2, a3, a4, a5, a6, a7) m(s, a1); m(s, a2); m(s, a3); m(s, a4); m(s, a5); m(s, a6); m(s, a7);
#define FE_8(m, s, a1, a2, a3, a4, a5, a6, a7, a8) m(s, a1); m(s, a2); m(s, a3); m(s, a4); m(s, a5); m(s, a6); m(s, a7); m(s, a8);
#define FE_9(m, s, a1, a2, a3, a4, a5, a6, a7, a8, a9) m(s, a1); m(s, a2); m(s, a3); m(s, a4); m(s, a5); m(s, a6); m(s, a7); m(s, a8); m(s, a9);
#define FE_10(m, s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) m(s, a1); m(s, a2); m(s, a3); m(s, a4); m(s, a5); m(s, a6); m(s, a7); m(s, a8); m(s, a9); m(s, a10);

#endif // REGISTER_STRUCT_FIELDS