/**
 * @file Renderer.h
 * @brief SDK-level Renderer interface
 *
 * This header provides access to renderer operations for scripts.
 * All functions are exported from NANOEngine.dll.
 */

#pragma once

// Include the actual renderer exports from the engine
// This is a thin wrapper to avoid scripts needing internal engine paths
#include "../../src/EditorInterface/RendererExports.hpp"

// Note: This header provides access to:
// - NE::Renderer::Command - Functions to control rendering
//
// Available namespaced functions:
//   NE::Renderer::Command::AssignMaterial(entity, materialUUID)
//   NE::Renderer::Command::SetVisible(entity, visible)
//   ... and more
