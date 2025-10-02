#include "IScript.hpp" 
#include <Math/Vec3.hpp>
#include <sstream>
#include <unordered_map>
#include <functional>

// PIMPL implementation to hide std containers from DLL interface
class IScript::FieldRegistry {
public:
    struct FieldEntry {
        std::string typeToken;
        void* memberPtr;
        std::function<std::string()> getValue;
        std::function<bool(const std::string&)> setValue;
    };

    std::unordered_map<std::string, FieldEntry> fields;
};

NE::ECS::Entity IScript::GetEntity() const {
    return m_entity;
}

IScript::~IScript() {
    delete m_fieldRegistry;
}

void IScript::LinkToEngine(NE::ECS::ComponentManager* componentManager) {
    m_componentManager = componentManager;
    
    // Initialize field registry if not already done
    if (!m_fieldRegistry) {
        m_fieldRegistry = new FieldRegistry();
    }
}

// === Field management implementation ===

std::vector<std::string> IScript::GetExposedFieldNames() const {
    if (!m_fieldRegistry) {
        return {};
    }
    
    std::vector<std::string> names;
    names.reserve(m_fieldRegistry->fields.size());
    for (const auto& [name, entry] : m_fieldRegistry->fields) {
        names.push_back(name);
    }
    return names;
}

std::string IScript::GetFieldType(const std::string& name) const {
    if (!m_fieldRegistry) {
        return {};
    }
    
    auto it = m_fieldRegistry->fields.find(name);
    if (it != m_fieldRegistry->fields.end()) {
        return it->second.typeToken;
    }
    return {};
}

std::string IScript::GetFieldValueAsString(const std::string& name) const {
    if (!m_fieldRegistry) {
        return {};
    }
    
    auto it = m_fieldRegistry->fields.find(name);
    if (it != m_fieldRegistry->fields.end()) {
        return it->second.getValue();
    }
    return {};
}

bool IScript::SetFieldValueFromString(const std::string& name, const std::string& value) {
    if (!m_fieldRegistry) {
        return false;
    }
    
    auto it = m_fieldRegistry->fields.find(name);
    if (it != m_fieldRegistry->fields.end()) {
        return it->second.setValue(value);
    }
    return false;
}

void IScript::RegisterFloatField(const std::string& name, float* memberPtr) {
    // Initialize field registry if not already done
    if (!m_fieldRegistry) {
        m_fieldRegistry = new FieldRegistry();
    }
    
    FieldRegistry::FieldEntry entry;
    entry.typeToken = "float";
    entry.memberPtr = memberPtr;
    entry.getValue = [memberPtr]() -> std::string {
        return std::to_string(*memberPtr);
    };
    entry.setValue = [memberPtr](const std::string& value) -> bool {
        try {
            *memberPtr = std::stof(value);
            return true;
        }
        catch (...) {
            return false;
        }
    };
    m_fieldRegistry->fields[name] = std::move(entry);
}

void IScript::RegisterIntField(const std::string& name, int* memberPtr) {
    // Initialize field registry if not already done
    if (!m_fieldRegistry) {
        m_fieldRegistry = new FieldRegistry();
    }
    
    FieldRegistry::FieldEntry entry;
    entry.typeToken = "int";
    entry.memberPtr = memberPtr;
    entry.getValue = [memberPtr]() -> std::string {
        return std::to_string(*memberPtr);
    };
    entry.setValue = [memberPtr](const std::string& value) -> bool {
        try {
            *memberPtr = std::stoi(value);
            return true;
        }
        catch (...) {
            return false;
        }
    };
    m_fieldRegistry->fields[name] = std::move(entry);
}

void IScript::RegisterBoolField(const std::string& name, bool* memberPtr) {
    // Initialize field registry if not already done
    if (!m_fieldRegistry) {
        m_fieldRegistry = new FieldRegistry();
    }
    
    FieldRegistry::FieldEntry entry;
    entry.typeToken = "bool";
    entry.memberPtr = memberPtr;
    entry.getValue = [memberPtr]() -> std::string {
        return *memberPtr ? "1" : "0";
    };
    entry.setValue = [memberPtr](const std::string& value) -> bool {
        if (value == "1" || value == "true") {
            *memberPtr = true;
            return true;
        }
        if (value == "0" || value == "false") {
            *memberPtr = false;
            return true;
        }
        return false;
    };
    m_fieldRegistry->fields[name] = std::move(entry);
}

void IScript::RegisterStringField(const std::string& name, std::string* memberPtr) {
    // Initialize field registry if not already done
    if (!m_fieldRegistry) {
        m_fieldRegistry = new FieldRegistry();
    }
    
    FieldRegistry::FieldEntry entry;
    entry.typeToken = "string";
    entry.memberPtr = memberPtr;
    entry.getValue = [memberPtr]() -> std::string {
        return *memberPtr;
    };
    entry.setValue = [memberPtr](const std::string& value) -> bool {
        try {
            *memberPtr = value;
            return true;
        }
        catch (...) {
            return false;
        }
    };
    m_fieldRegistry->fields[name] = std::move(entry);
}

void IScript::RegisterVec3Field(const std::string& name, NE::Math::Vec3* memberPtr) {
    // Initialize field registry if not already done
    if (!m_fieldRegistry) {
        m_fieldRegistry = new FieldRegistry();
    }
    
    FieldRegistry::FieldEntry entry;
    entry.typeToken = "vec3";
    entry.memberPtr = memberPtr;
    entry.getValue = [memberPtr]() -> std::string {
        std::ostringstream oss;
        oss << memberPtr->x << ' ' << memberPtr->y << ' ' << memberPtr->z;
        return oss.str();
    };
    entry.setValue = [memberPtr](const std::string& value) -> bool {
        try {
            std::istringstream iss(value);
            float x, y, z;
            if (!(iss >> x >> y >> z)) {
                return false;
            }
            memberPtr->x = x;
            memberPtr->y = y;
            memberPtr->z = z;
            return true;
        }
        catch (...) {
            return false;
        }
    };
    m_fieldRegistry->fields[name] = std::move(entry);
}
