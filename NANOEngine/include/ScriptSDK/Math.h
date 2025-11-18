/**
 * @file Math.h
 * @brief SDK-level Math types interface
 *
 * This header provides access to engine math types used in components.
 * Scripts primarily use NE::Scripting::Vec3 which auto-converts to Math::Vec3.
 */

#pragma once

// Include engine math types
// Scripts need these for component structures (Transform, Light, etc.)
#include "../../src/Math/Vec3.hpp"
#include "../../src/Math/Mat4.hpp"

// Note: Scripts should primarily use NE::Scripting::Vec3 from ScriptTypes.h
// The Scripting::Vec3 type has implicit conversion to/from Math::Vec3
//
// Math::Vec3 - Engine's internal 3D vector type
// Math::Mat4 - Engine's internal 4x4 matrix type
//
// These are used internally by component structures but scripts
// can seamlessly convert between Scripting::Vec3 and Math::Vec3
