/**
 * @file ScriptMacros.h
 * @brief Convenience macros for script field registration
 *
 * These macros simplify field registration in script constructors.
 *
 * Usage:
 * @code
 * class MyScript : public NE::Scripting::IScript {
 * public:
 *     MyScript() {
 *         SCRIPT_FIELD(speed, Float);
 *         SCRIPT_FIELD(maxHealth, Int);
 *         SCRIPT_FIELD(isActive, Bool);
 *         SCRIPT_FIELD(spawnPoint, Vec3);
 *         SCRIPT_COMPONENT_REF(target, Transform);
 *     }
 *
 * private:
 *     float speed = 5.0f;
 *     int maxHealth = 100;
 *     bool isActive = true;
 *     NE::Scripting::Vec3 spawnPoint{0, 0, 0};
 *     NE::Scripting::TransformRef target;
 * };
 * @endcode
 */

#pragma once

//=============================================================================
// FIELD REGISTRATION MACROS
//=============================================================================

/**
 * Register a field for editor exposure.
 * @param fieldName The member variable name
 * @param fieldType The type (Float, Int, Bool, String, Vec3)
 */
#ifndef SCRIPT_FIELD
#define SCRIPT_FIELD(fieldName, fieldType) \
    Register##fieldType##Field(#fieldName, &this->fieldName)
#endif

/**
 * Register a component reference field.
 * @param fieldName The member variable name
 * @param ComponentType The component type (Transform, Rigidbody, AudioSource)
 */
#ifndef SCRIPT_COMPONENT_REF
#define SCRIPT_COMPONENT_REF(fieldName, ComponentType) \
    Register##ComponentType##RefField(#fieldName, &this->fieldName)
#endif

//=============================================================================
// LEGACY COMPATIBILITY MACROS
//=============================================================================

/**
 * Legacy macro for field registration (same as SCRIPT_FIELD).
 * Provided for backward compatibility with existing scripts.
 */
#ifndef SCRIPT_REGISTER_FIELD
#define SCRIPT_REGISTER_FIELD(fieldName, fieldType) \
    SCRIPT_FIELD(fieldName, fieldType)
#endif

/**
 * Legacy macro alias for SCRIPT_FIELD.
 */
#ifndef REGISTER_FIELD
#define REGISTER_FIELD(fieldName) \
    RegisterFloatField(#fieldName, &this->fieldName)
#endif
