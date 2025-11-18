/**
 * @file ECS.h
 * @brief SDK-level ECS (Entity Component System) interface
 *
 * This header provides access to ECS operations for scripts.
 * All functions are exported from NANOEngine.dll.
 */

#pragma once

// Include the actual ECS exports from the engine
// This is a thin wrapper to avoid scripts needing internal engine paths
#include "../../src/ECS/Core/Entity.hpp"
#include "../../src/EditorInterface/ECSExports.hpp"

// Note: This header provides access to:
// - NE::ECS::Entity - Entity ID type (uint32_t)
// - NE::ECS::NO_ENTITY - Invalid entity constant
// - NE::ECS::MAX_ENTITIES - Maximum number of entities
// - NE::ECS::Command - Functions to get/modify components
// - NE::ECS::Query - Functions to check component existence
//
// Available namespaced functions:
//   NE::ECS::Command::GetEntityTransform(entity)
//   NE::ECS::Command::GetEntityLight(entity)
//   NE::ECS::Command::GetEntityCollider(entity)
//   NE::ECS::Command::GetComponent<T>(entity)
//   ... and more
//
//   NE::ECS::Query::HasTransform(entity)
//   NE::ECS::Query::HasLight(entity)
//   ... and more
