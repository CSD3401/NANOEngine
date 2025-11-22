#pragma once
#include "EngineAPI.hpp"

/**
 * @file ScriptBase.hpp
 * @brief CRTP helper that automatically provides inspector interface for scripts
 * 
 * ScriptBase eliminates boilerplate by automatically implementing all inspector
 * interface methods (GetExposedFieldNames, GetFieldType, GetArraySize, etc.)
 * 
 * **Usage:**
 * ```cpp
 * class MyScript : public ScriptBase<MyScript> {
 * public:
 *     MyScript() {
 *         REGISTER_FIELD(speed);
 *         REGISTER_VECTOR(enemies);
 *         REGISTER_ENUM(state);
 *     }
 *     // ... your script methods ...
 * private:
 *     float speed = 5.0f;
 *     std::vector<std::string> enemies;
 * };
 * ```
 * 
 * **Benefits:**
 * - No need to manually override inspector interface methods
 * - Automatic support for vectors, enums, structs
 * - Works with hot-reloading
 * - Type-safe CRTP pattern
 * 
 * **Supported Field Types:**
 * - Primitives: float, int, bool, string, Vec3
 * - Vectors: std::vector<T> (where T is any supported type)
 * - Enums: Registered with REGISTER_ENUM macro
 * - Structs: Registered with REGISTER_REFLECTABLE_STRUCT macro
 * - Component References: TransformRef, RigidbodyRef, AudioSourceRef
 * 
 * @tparam Derived The derived script class (CRTP pattern)
 */
template<typename Derived>
class ScriptBase : public IScript {
public:
    // === Automatic Inspector Interface ===
    // These methods are automatically provided for all derived scripts
    
    std::vector<std::string> GetExposedFieldNames() const override { 
    return m_fields.GetNames(); 
    }
    
    std::string GetFieldType(const std::string& name) const override { 
        return m_fields.GetType(name); 
    }
    
    std::string GetFieldValueAsString(const std::string& name) const override { 
        return m_fields.GetValue(name); 
    }
    
    bool SetFieldValueFromString(const std::string& name, const std::string& value) override { 
        return m_fields.SetValue(name, value); 
    }

    // Enum support
    std::vector<std::string> GetEnumOptions(const std::string& fieldName) const override {
 return m_fields.GetEnumOptions(fieldName);
    }

    // Array/Vector support
    size_t GetArraySize(const std::string& fieldName) const override {
        return m_fields.GetArraySize(fieldName);
    }

    std::string GetArrayElement(const std::string& fieldName, size_t index) const override {
        return m_fields.GetArrayElement(fieldName, index);
    }

    bool SetArrayElement(const std::string& fieldName, size_t index, const std::string& value) override {
        return m_fields.SetArrayElement(fieldName, index, value);
    }

    void AddArrayElement(const std::string& fieldName) override {
    m_fields.AddArrayElement(fieldName);
    }

    void RemoveArrayElement(const std::string& fieldName, size_t index) override {
        m_fields.RemoveArrayElement(fieldName, index);
    }

protected:
    // Field registry - accessible to derived classes
    ExposedFieldRegistry m_fields;
};
