/**
 * @file Components.h
 * @brief SDK-level component definitions for scripting
 *
 * This header provides access to engine component structures.
 * Scripts can read/write component fields through the ECS API.
 */

#pragma once

// Include the actual component definitions from the engine
// This is a thin wrapper to avoid scripts needing internal engine paths
#include "../../src/ECS/Components/Transform.hpp"
#include "../../src/ECS/Components/Light.hpp"
#include "../../src/ECS/Components/Collider.hpp"

// Note: This header provides access to:
// - NE::ECS::Component::Transform - Position, rotation, scale
// - NE::ECS::Component::Light - Lighting properties
// - NE::ECS::Component::Collider - Physics collision shapes
//
// All component structures are defined in the engine and exposed here.
// The reflection macros are already defined in the engine component files.
