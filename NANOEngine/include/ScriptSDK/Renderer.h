/**
 * @file Renderer.h
 * @brief SDK-level Renderer interface
 *
 * This header provides access to renderer operations for scripts.
 * All functions are exported from NANOEngine.dll.
 *
 * IMPORTANT: This header is STANDALONE - no internal engine includes.
 * Declarations are duplicated from the engine and must be kept in sync.
 */

#pragma once

#include <cstdint>
#include <string>

namespace NE {
namespace Renderer {

    //=========================================================================
    // QUERY NAMESPACE - Read-only renderer queries
    //=========================================================================

    namespace Query {
        /// Get the model UUID assigned to an entity
        /// @param e Entity ID
        /// @return Model UUID string, or "empty uuid" if none assigned
        __declspec(dllimport) std::string GetModel(uint32_t e);

        /// Get the material UUID assigned to an entity
        /// @param e Entity ID
        /// @return Material UUID string, or "empty uuid" if none assigned
        __declspec(dllimport) std::string GetMaterial(uint32_t e);
    }

    //=========================================================================
    // COMMAND NAMESPACE - Renderer commands
    //=========================================================================

    namespace Command {
        /// Assign a model to an entity by UUID
        /// @param e Entity ID
        /// @param uuid Model asset UUID string
        __declspec(dllimport) void AssignModel(uint32_t e, const std::string& uuid);

        /// Assign a material to an entity by UUID
        /// @param e Entity ID
        /// @param uuid Material asset UUID string
        __declspec(dllimport) void AssignMaterial(uint32_t e, const std::string& uuid);
    }

} // namespace Renderer
} // namespace NE
